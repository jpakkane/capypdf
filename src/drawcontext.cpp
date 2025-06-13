// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <drawcontext.hpp>
#include <generator.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_IMAGE_H
#include <utils.hpp>
#include <array>
#include <cmath>
#include <cassert>

namespace capypdf::internal {

namespace {

void write_matrix(ObjectFormatter &fmt, const PdfMatrix &gm) {
    fmt.add_token("/Matrix");
    fmt.begin_array();
    fmt.add_token(gm.a);
    fmt.add_token(gm.b);
    fmt.add_token(gm.c);
    fmt.add_token(gm.d);
    fmt.add_token(gm.e);
    fmt.add_token(gm.f);
    fmt.end_array();
}

} // namespace

GstatePopper::~GstatePopper() { ctx->cmd_Q(); }

PdfDrawContext::PdfDrawContext(PdfDocument *doc,
                               PdfColorConverter *cm,
                               CapyPDF_Draw_Context_Type dtype,
                               const PdfRectangle &area)
    : doc(doc), cm(cm), context_type{dtype}, bbox{area} {}

PdfDrawContext::~PdfDrawContext() {}

rvoe<DCSerialization> PdfDrawContext::serialize() {
    if(context_type == CAPY_DC_FORM_XOBJECT) {
        ObjectFormatter fmt;
        fmt.begin_dict();
        fmt.add_token_pair("/Type", "/XObject");
        fmt.add_token_pair("/Subtype", "/Form");
        fmt.add_token("/BBox");
        fmt.begin_array();
        fmt.add_token(bbox.x1);
        fmt.add_token(bbox.y1);
        fmt.add_token(bbox.x2);
        fmt.add_token(bbox.y2);
        fmt.end_array();
        fmt.add_token("/Resources");
        build_resource_dict(fmt);
        if(group_matrix) {
            write_matrix(fmt, group_matrix.value());
        }
        ERC(commands, cmds.steal());
        return SerializedXObject{std::move(fmt), std::move(commands)};
    } else if(context_type == CAPY_DC_TRANSPARENCY_GROUP) {
        ObjectFormatter fmt;
        fmt.begin_dict();
        fmt.add_token_pair("/Type", "/XObject");
        fmt.add_token_pair("/Subtype", "/Form");
        fmt.add_token("/BBox");
        fmt.begin_array();
        fmt.add_token(bbox.x1);
        fmt.add_token(bbox.y1);
        fmt.add_token(bbox.x2);
        fmt.add_token(bbox.y2);
        fmt.end_array();
        if(custom_props.transparency_props) {
            fmt.add_token("/Group");
            custom_props.transparency_props->serialize(fmt);
        }
        if(group_matrix) {
            write_matrix(fmt, group_matrix.value());
        }
        fmt.add_token("/Resources");
        build_resource_dict(fmt);
        ERC(commands, cmds.steal());
        return SerializedXObject{std::move(fmt), std::move(commands)};
    } else if(context_type == CAPY_DC_COLOR_TILING) {
        ObjectFormatter fmt;
        fmt.begin_dict();
        fmt.add_token_pair("/Type", "/Pattern");
        fmt.add_token_pair("/PatternType", "1");
        fmt.add_token_pair("/PaintType", "1");
        fmt.add_token_pair("/TilingType", "1");
        fmt.add_token("/BBox");
        fmt.begin_array();
        fmt.add_token(bbox.x1);
        fmt.add_token(bbox.y1);
        fmt.add_token(bbox.x2);
        fmt.add_token(bbox.y2);
        fmt.end_array();
        if(group_matrix) {
            write_matrix(fmt, group_matrix.value());
        }
        fmt.add_token_pair("/XStep", get_w());
        fmt.add_token_pair("/YStep", get_h());
        fmt.add_token("/Resources");
        build_resource_dict(fmt);
        ERC(commands, cmds.steal());
        return SerializedXObject{std::move(fmt), std::move(commands)};
    } else {
        assert(!group_matrix);
        SerializedBasicContext sc;
        ObjectFormatter fmt;
        build_resource_dict(fmt);
        sc.resource_dict = fmt.steal();
        ERC(commands, cmds.steal());
        sc.command_stream = std::move(commands);
        return DCSerialization{std::move(sc)};
    }
}

void PdfDrawContext::clear() {
    used_images.clear();
    used_subset_fonts.clear();
    used_fonts.clear();
    used_colorspaces.clear();
    used_gstates.clear();
    used_shadings.clear();
    used_patterns.clear();
    used_widgets.clear();
    used_annotations.clear();
    used_ocgs.clear();
    used_trgroups.clear();
    sub_navigations.clear();
    transition.reset();
    is_finalized = false;
    uses_all_colorspace = false;
    custom_props = PageProperties{};
    group_matrix.reset();
    cmds.clear();
}

void PdfDrawContext::build_resource_dict(ObjectFormatter &fmt) {
    fmt.begin_dict();
    pystd2025::CString scratch;
    if(!used_images.is_empty() || !used_form_xobjects.is_empty() || !used_trgroups.is_empty()) {
        fmt.add_token("/XObject");
        fmt.begin_dict();
        if(!used_images.is_empty()) {
            for(const auto &i : used_images) {
                scratch = pystd2025::format("/Image%d", i);
                fmt.add_token(scratch);
                fmt.add_object_ref(i);
            }
        }
        if(!used_form_xobjects.is_empty()) {
            for(const auto &fx : used_form_xobjects) {
                scratch = pystd2025::format("/FXO%d", fx);
                fmt.add_token(scratch);
                fmt.add_object_ref(fx);
            }
        }
        if(!used_trgroups.is_empty()) {
            for(const auto &tg : used_trgroups) {
                auto objnum = doc->transparency_groups.at(tg.id);
                scratch = pystd2025::format("/TG%d", objnum);
                fmt.add_token(scratch);
                fmt.add_object_ref(objnum);
            }
        }
        fmt.end_dict();
    }
    if(!used_fonts.is_empty() || !used_subset_fonts.is_empty()) {
        fmt.add_token("/Font");
        fmt.begin_dict();
        for(const auto &i : used_fonts) {
            scratch = pystd2025::format("/Font%d}", i);
            fmt.add_token(scratch);
            fmt.add_object_ref(i);
        }
        for(const auto &i : used_subset_fonts) {
            const auto &bob = doc->get(i.fid);
            assert(i.subset_id == 0);
            scratch = pystd2025::format("/SFont%d", bob.font_obj);
            fmt.add_token(scratch);
            fmt.add_object_ref(bob.font_obj);
        }
        fmt.end_dict();
    }
    if(!used_colorspaces.is_empty() || uses_all_colorspace) {
        fmt.add_token("/ColorSpace");
        fmt.begin_dict();
        if(uses_all_colorspace) {
            fmt.add_token("/All");
            fmt.add_object_ref(doc->separation_objects.at(0));
        }
        for(const auto &i : used_colorspaces) {
            scratch = pystd2025::format("/CSpace%d", i);
            fmt.add_token(scratch);
            fmt.add_object_ref(i);
        }
        fmt.end_dict();
    }
    if(!used_gstates.is_empty()) {
        fmt.add_token("/ExtGState");
        fmt.begin_dict();
        for(const auto &s : used_gstates) {
            scratch = pystd2025::format("/GS%d", s);
            fmt.add_token(scratch);
            fmt.add_object_ref(s);
        }
        fmt.end_dict();
    }
    if(!used_shadings.is_empty()) {
        fmt.add_token("/Shading");
        fmt.begin_dict();
        for(const auto &s : used_shadings) {
            scratch = pystd2025::format("/SH%d", s);
            fmt.add_token(scratch);
            fmt.add_object_ref(doc->shadings.at(s).object_number);
        }
        fmt.end_dict();
    }
    if(!used_patterns.is_empty()) {
        fmt.add_token("/Pattern");
        fmt.begin_dict();
        for(const auto &s : used_patterns) {
            scratch = pystd2025::format("/Pattern-%d", s);
            fmt.add_token(scratch);
            fmt.add_object_ref(s);
        }
        fmt.end_dict();
    }
    if(!used_ocgs.is_empty()) {
        fmt.add_token("/Properties");
        fmt.begin_dict();
        for(const auto &ocg : used_ocgs) {
            auto objnum = doc->ocg_object_number(ocg);
            scratch = pystd2025::format("/oc%d", objnum);
            fmt.add_token(scratch);
            fmt.add_object_ref(objnum);
        }
        fmt.end_dict();
    }
    fmt.end_dict();
}

rvoe<NoReturnValue> PdfDrawContext::add_form_widget(CapyPDF_FormWidgetId widget) {
    if(used_widgets.contains(widget)) {
        RETERR(AnnotationReuse);
    }
    used_widgets.insert(widget);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::annotate(CapyPDF_AnnotationId annotation) {
    if(used_annotations.contains(annotation)) {
        RETERR(AnnotationReuse);
    }
    used_annotations.insert(annotation);
    RETOK;
}

GstatePopper PdfDrawContext::push_gstate() {
    cmd_q();
    return GstatePopper(this);
}

rvoe<NoReturnValue> PdfDrawContext::cmd_b() {
    cmds.append("b");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_B() {
    cmds.append("B");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_bstar() {
    cmds.append("b*");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Bstar() {
    cmds.append("B*");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_BDC(const asciistring &name,
                                            pystd2025::Optional<CapyPDF_StructureItemId> sid,
                                            const BDCTags *attributes) {
    if(!sid && !attributes) {
        fprintf(stderr, "%s", "Must specify sid or attributes. Otherwise use BMC.\n");
        std::abort();
    }
    cmds.append_indent();
    cmds.append_raw("/");
    cmds.append_raw(name.sv());
    cmds.append_raw(" <<\n");
    cmds.indent(DrawStateType::Dictionary);
    if(sid) {
        ERC(MCID_id, add_bcd_structure(sid.value()));
        cmds.append_dict_entry("/MCID", MCID_id);
    }
    if(attributes) {
        for(const auto &[key, value] : *attributes) {
            cmds.append_dict_entry_string(key->c_str(), value->c_str());
        }
    }
    cmds.append(">>");
    ERCV(cmds.dedent(DrawStateType::Dictionary));
    cmds.append("BDC");
    ERCV(cmds.indent(DrawStateType::MarkedContent));
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_BDC(CapyPDF_StructureItemId sid,
                                            const BDCTags *attributes) {
    const auto &itemtype = doc->structure_items.at(sid.id).stype;
    if(auto builtin = std::get_if<CapyPDF_Structure_Type>(&itemtype)) {
        ERC(astr, asciistring::from_cstr(structure_type_names.at(*builtin)));
        return cmd_BDC(astr, sid, attributes);
    } else if(auto role = std::get_if<CapyPDF_RoleId>(&itemtype)) {
        auto quoted = bytes2pdfstringliteral(doc->rolemap.at(role->id).name.view(), false);
        ERC(astr, asciistring::from_cstr(quoted.c_str()));
        return cmd_BDC(astr, sid, attributes);
    } else {
        std::abort();
    }
}

rvoe<NoReturnValue> PdfDrawContext::cmd_BDC(CapyPDF_OptionalContentGroupId ocgid) {
    used_ocgs.insert(ocgid);
    ERCV(cmds.indent(DrawStateType::MarkedContent));
    auto cmd = pystd2025::format("/OC /oc%d BDC\n", doc->ocg_object_number(ocgid));
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_BMC(pystd2025::CStringView tag) {
    if(tag.size() < 2 || tag.front() == '/') {
        RETERR(SlashStart);
    }
    ERCV(cmds.indent(DrawStateType::MarkedContent));
    pystd2025::CString tag_(tag);
    auto cmd = pystd2025::format("/%s BMC\n", tag_.c_str());
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue>
PdfDrawContext::cmd_c(double x1, double y1, double x2, double y2, double x3, double y3) {
    auto cmd = pystd2025::format("%f %f %f %f %f %f c\n", x1, y1, x2, y2, x3, y3);
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue>
PdfDrawContext::cmd_cm(double m1, double m2, double m3, double m4, double m5, double m6) {
    auto cmd = pystd2025::format("%f %f %f %f %f %f cm\n", m1, m2, m3, m4, m5, m6);
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_CS(pystd2025::CStringView cspace_name) {
    cmds.append_command(cspace_name, "CS");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_cs(pystd2025::CStringView cspace_name) {
    cmds.append_command(cspace_name, "cs");
    RETOK;
}

rvoe<NoReturnValue>
PdfDrawContext::cmd_d(double *dash_array, size_t dash_array_length, double phase) {
    if(dash_array_length == 0) {
        RETERR(ZeroLengthArray);
    }
    for(size_t i = 0; i < dash_array_length; ++i) {
        if(dash_array[i] < 0) {
            RETERR(NegativeDash);
        }
    }
    pystd2025::CString cmd;
    cmd += "[ ";
    for(size_t i = 0; i < dash_array_length; ++i) {
        pystd2025::format_append(cmd, "%f ", dash_array[i]);
    }
    pystd2025::format_append(cmd, "] %f d\n", phase);
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Do(CapyPDF_FormXObjectId fxoid) {
    CHECK_INDEXNESS(fxoid.id, doc->form_xobjects);
    auto cmd = pystd2025::format("/FXO%d Do\n", doc->form_xobjects[fxoid.id].xobj_num);
    cmds.append(cmd);
    used_form_xobjects.insert(doc->form_xobjects[fxoid.id].xobj_num);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Do(CapyPDF_TransparencyGroupId trid) {
    CHECK_INDEXNESS(trid.id, doc->transparency_groups);
    auto cmd = pystd2025::format("/TG%d Do\n", doc->transparency_groups[trid.id]);
    cmds.append(cmd);
    used_trgroups.insert(trid);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Do(CapyPDF_ImageId im_id) {
    CHECK_INDEXNESS(im_id.id, doc->image_info);
    auto obj_num = doc->image_object_number(im_id);
    used_images.insert(obj_num);
    auto cmd = pystd2025::format("/Image%d Do\n", obj_num);
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_EMC() { return cmds.EMC(); }

rvoe<NoReturnValue> PdfDrawContext::cmd_f() {
    cmds.append("f");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_fstar() {
    cmds.append("f*");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_G(LimitDouble gray) { return serialize_G(gray); }

rvoe<NoReturnValue> PdfDrawContext::cmd_g(LimitDouble gray) { return serialize_g(gray); }

rvoe<NoReturnValue> PdfDrawContext::cmd_gs(CapyPDF_GraphicsStateId gid) {
    CHECK_INDEXNESS(gid.id, doc->document_objects);
    used_gstates.insert(gid.id);
    auto cmd = pystd2025::format("/GS%d gs\n", gid.id);
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_h() {
    cmds.append("h");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_i(double flatness) {
    if(flatness < 0 || flatness > 100) {
        RETERR(InvalidFlatness);
    }
    cmds.append_command(flatness, "i");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_j(CapyPDF_Line_Join join_style) {
    CHECK_ENUM(join_style, CAPY_LJ_BEVEL);
    cmds.append_command((int)join_style, "j");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_J(CapyPDF_Line_Cap cap_style) {
    CHECK_ENUM(cap_style, CAPY_LC_PROJECTION);
    cmds.append_command((int)cap_style, "J");
    RETOK;
}

rvoe<NoReturnValue>
PdfDrawContext::cmd_K(LimitDouble c, LimitDouble m, LimitDouble y, LimitDouble k) {
    return serialize_K(c, m, y, k);
}

rvoe<NoReturnValue>
PdfDrawContext::cmd_k(LimitDouble c, LimitDouble m, LimitDouble y, LimitDouble k) {
    return serialize_k(c, m, y, k);
}

rvoe<NoReturnValue> PdfDrawContext::cmd_l(double x, double y) {
    auto cmd = pystd2025::format("%f %f l\n", x, y);
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_m(double x, double y) {
    auto cmd = pystd2025::format("%f %f m\n", x, y);
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_M(double miterlimit) {
    cmds.append_command(miterlimit, "M");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_n() {
    cmds.append("n");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_q() {
    cmds.q();
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Q() { return cmds.Q(); }

rvoe<NoReturnValue> PdfDrawContext::cmd_re(double x, double y, double w, double h) {
    auto cmd = pystd2025::format("%f %f %f %f re\n", x, y, w, h);
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_RG(LimitDouble r, LimitDouble g, LimitDouble b) {
    return serialize_RG(r, g, b);
}

rvoe<NoReturnValue> PdfDrawContext::cmd_rg(LimitDouble r, LimitDouble g, LimitDouble b) {
    return serialize_rg(r, g, b);
}

rvoe<NoReturnValue> PdfDrawContext::cmd_ri(CapyPDF_Rendering_Intent ri) {
    CHECK_ENUM(ri, CAPY_RI_PERCEPTUAL);
    auto cmd = pystd2025::format("/%s ri\n", rendering_intent_names.at((int)ri));
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_s() {
    cmds.append("s");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_S() {
    cmds.append("S");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_SCN(double value) {
    cmds.append_command(value, "SCN");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_scn(double value) {
    cmds.append_command(value, "scn");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_sh(CapyPDF_ShadingId shid) {
    CHECK_INDEXNESS(shid.id, doc->document_objects);
    used_shadings.insert(shid.id);
    auto cmd = pystd2025::format("/SH%d sh\n", shid.id);
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Tr(CapyPDF_Text_Mode mode) {
    CHECK_ENUM(mode, CAPY_TEXT_CLIP);
    cmds.append_command((int)mode, "Tr");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_v(double x2, double y2, double x3, double y3) {
    auto cmd = pystd2025::format("%f %f %f %f v\n", x2, y2, x3, y3);
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_w(double w) {
    if(w < 0) {
        RETERR(NegativeLineWidth);
    }
    cmds.append_command(w, "w");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_W() {
    cmds.append("W");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Wstar() {
    cmds.append("W*");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_y(double x1, double y1, double x3, double y3) {
    auto cmd = pystd2025::format("%f %f %f %f y\n", x1, y1, x3, y3);
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::serialize_G(LimitDouble gray) {
    cmds.append_command(gray.v(), "G");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::serialize_g(LimitDouble gray) {
    cmds.append_command(gray.v(), "g");
    RETOK;
}

rvoe<NoReturnValue>
PdfDrawContext::serialize_K(LimitDouble c, LimitDouble m, LimitDouble y, LimitDouble k) {
    cmds.append_command(c.v(), m.v(), y.v(), k.v(), "K");
    RETOK;
}

rvoe<NoReturnValue>
PdfDrawContext::serialize_k(LimitDouble c, LimitDouble m, LimitDouble y, LimitDouble k) {
    cmds.append_command(c.v(), m.v(), y.v(), k.v(), "k");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::serialize_RG(LimitDouble r, LimitDouble g, LimitDouble b) {
    cmds.append_command(r.v(), g.v(), b.v(), "RG");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::serialize_rg(LimitDouble r, LimitDouble g, LimitDouble b) {
    cmds.append_command(r.v(), g.v(), b.v(), "rg");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const Color &c, bool stroke) {
    if(auto cv = std::get_if<DeviceRGBColor>(&c)) {
        return set_color(*cv, stroke);
    } else if(auto cv = std::get_if<DeviceGrayColor>(&c)) {
        return set_color(*cv, stroke);
    } else if(auto cv = std::get_if<DeviceCMYKColor>(&c)) {
        return set_color(*cv, stroke);
    } else if(auto cv = std::get_if<ICCColor>(&c)) {
        return set_color(*cv, stroke);
    } else if(auto cv = std::get_if<LabColor>(&c)) {
        return set_color(*cv, stroke);
    } else if(auto cv = std::get_if<CapyPDF_PatternId>(&c)) {
        return set_color(*cv, stroke);
    } else if(auto cv = std::get_if<SeparationColor>(&c)) {
        return set_color(*cv, stroke);
    } else {
        fprintf(stderr, "Given colorspace not supported yet.\n");
        std::abort();
    }
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::convert_to_output_cs_and_set_color(const DeviceRGBColor &c,
                                                                       bool stroke) {
    switch(doc->docprops.output_colorspace) {
    case CAPY_DEVICE_CS_RGB: {
        return set_color(c, stroke);
    }
    case CAPY_DEVICE_CS_GRAY: {
        DeviceGrayColor gray = cm->to_gray(c);
        return set_color(gray, stroke);
    }
    case CAPY_DEVICE_CS_CMYK: {
        ERC(cmyk, cm->to_cmyk(c));
        return set_color(cmyk, stroke);
    }
    }
    std::abort();
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const DeviceRGBColor &c, bool stroke) {
    if(stroke) {
        return cmd_RG(c.r.v(), c.g.v(), c.b.v());
    } else {
        return cmd_rg(c.r.v(), c.g.v(), c.b.v());
    }
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const DeviceGrayColor &c, bool stroke) {
    if(stroke) {
        return cmd_G(c.v.v());
    } else {
        return cmd_g(c.v.v());
    }
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const DeviceCMYKColor &c, bool stroke) {
    if(stroke) {
        return cmd_K(c.c.v(), c.m.v(), c.y.v(), c.k.v());
    } else {
        return cmd_k(c.c.v(), c.m.v(), c.y.v(), c.k.v());
    }
}

rvoe<NoReturnValue> PdfDrawContext::convert_to_output_cs_and_set_color(const DeviceCMYKColor &c,
                                                                       bool stroke) {
    switch(doc->docprops.output_colorspace) {
    case CAPY_DEVICE_CS_RGB: {
        ERC(rgb, cm->to_rgb(c));
        return set_color(rgb, stroke);
    }
    case CAPY_DEVICE_CS_GRAY: {
        ERC(gray, cm->to_gray(c));
        return set_color(gray, stroke);
    }
    case CAPY_DEVICE_CS_CMYK: {
        return set_color(c, stroke);
    }
    default:
        RETERR(Unreachable);
    }
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const ICCColor &icc, bool stroke) {
    CHECK_INDEXNESS(icc.id.id, doc->icc_profiles);
    const auto &icc_info = doc->get(icc.id);
    if(icc_info.num_channels != (int32_t)icc.values.size()) {
        RETERR(IncorrectColorChannelCount);
    }
    used_colorspaces.insert(icc_info.object_num);
    auto cmd = pystd2025::format("/CSpace%d %s\n", icc_info.object_num, stroke ? "CS" : "cs");
    cmds.append(cmd);
    cmd.clear();
    for(const auto &i : icc.values) {
        pystd2025::format_append(cmd, "%f ", i);
    }
    cmd += stroke ? "SCN" : "scn";
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::convert_to_output_cs_and_set_color(const DeviceGrayColor &c,
                                                                       bool stroke) {
    // Assumes that switching to the gray colorspace is always ok.
    return set_color(c, stroke);
}

rvoe<NoReturnValue> PdfDrawContext::set_color(CapyPDF_PatternId id, bool stroke) {
    if(stroke) {
        ERCV(cmd_CS("/Pattern"));
    } else {
        ERCV(cmd_cs("/Pattern"));
    }
    used_patterns.insert(id.id);
    auto cmd = pystd2025::format("/Pattern-%d %s\n", id.id, stroke ? "SCN" : "scn");
    cmds.append(cmd);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const SeparationColor &color, bool stroke) {
    CHECK_INDEXNESS(color.id.id, doc->separation_objects);
    const auto idnum = doc->separation_object_number(color.id);
    used_colorspaces.insert(idnum);
    auto csname = pystd2025::format("/CSpace%d", idnum);
    if(stroke) {
        cmd_CS(csname);
        cmd_SCN(color.v.v());
    } else {
        cmd_cs(csname);
        cmd_scn(color.v.v());
    }
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const LabColor &c, bool stroke) {
    CHECK_INDEXNESS(c.id.id, doc->document_objects);
    used_colorspaces.insert(c.id.id);
    auto csname = pystd2025::format("/CSpace%d", c.id.id);
    if(stroke) {
        cmd_CS(csname);
    } else {
        cmd_cs(csname);
    }
    auto cmd = pystd2025::format("%f %f %f %s\n", c.l, c.a, c.b, stroke ? "SCN" : "scn");
    cmds.append(cmd);
    RETOK;
}

void PdfDrawContext::set_all_stroke_color() {
    uses_all_colorspace = true;
    cmd_CS("/All");
    cmd_SCN(1.0);
}

void PdfDrawContext::scale(double xscale, double yscale) { cmd_cm(xscale, 0, 0, yscale, 0, 0); }

void PdfDrawContext::translate(double xtran, double ytran) { cmd_cm(1.0, 0, 0, 1.0, xtran, ytran); }

void PdfDrawContext::rotate(double angle) {
    cmd_cm(cos(angle), sin(angle), -sin(angle), cos(angle), 0.0, 0.0);
}

rvoe<NoReturnValue> PdfDrawContext::render_text(
    const u8string &text, CapyPDF_FontId fid, double pointsize, double x, double y) {
    PdfText t(this);
    t.cmd_Tf(fid, pointsize);
    t.cmd_Td(x, y);
    t.cmd_Tj(text);
    return render_text(t);
}

rvoe<NoReturnValue> PdfDrawContext::serialize_charsequence(const TextEvents &charseq,
                                                           CommandStreamFormatter &serialisation,
                                                           CapyPDF_FontId &current_font) {
    CHECK_INDEXNESS(current_font.id, doc->font_objects);
    bool is_first = true;
    auto glyph_appender_lambda =
        [this, &serialisation, &is_first](const SubsetGlyph &current_subset_glyph) {
            used_subset_fonts.insert(current_subset_glyph.ss);
            if(is_first) {
                serialisation.append_indent();
                serialisation.append_raw("[ ");
            }
            auto glyphid = pystd2025::format("<%04x> ", current_subset_glyph.glyph_id);
            serialisation.append_raw(glyphid);
        };
    for(const auto &e : charseq) {
        if(auto kval = std::get_if<KerningValue>(&e)) {
            if(is_first) {
                serialisation.append_indent();
                serialisation.append_raw("[ ");
            }
            auto v = pystd2025::format("%d ", kval->v);
            serialisation.append_raw(v);
        } else if(auto uglyph = std::get_if<UnicodeCharacter>(&e)) {
            const auto codepoint = uglyph->codepoint;
            ERC(current_subset_glyph, doc->get_subset_glyph(current_font, codepoint, {}));
            glyph_appender_lambda(current_subset_glyph);
        } else if(auto u8str = std::get_if<u8string>(&e)) {
            if(u8str->empty()) {
                continue;
            }
            ERC(subset, doc->get_subset_glyph(current_font, *u8str->begin(), {}));
            used_subset_fonts.insert(subset.ss);
            if(is_first) {
                serialisation.append_indent();
                serialisation.append_raw("[ ");
            }
            serialisation.append_raw("<");
            for(const auto codepoint : *u8str) {
                ERC(current_subset_glyph, doc->get_subset_glyph(current_font, codepoint, {}));
                auto cmd = pystd2025::format("%04x", current_subset_glyph.glyph_id);
                serialisation.append_raw(cmd);
            }
            serialisation.append_raw("> ");
        } else if(auto actualtext = std::get_if<ActualTextStart>(&e)) {
            auto u16 = utf8_to_pdfutf16be(actualtext->text);
            serialisation.append_raw("] TJ\n");
            auto cmd = pystd2025::format("/Span << /ActualText %s >> BDC", u16.c_str());
            serialisation.append(cmd);
            serialisation.append_raw("[");
        } else if(std::holds_alternative<ActualTextEnd>(e)) {
            serialisation.append_raw("] TJ\n");
            serialisation.append("EMC [");
        } else if(auto glyphitem = std::get_if<GlyphItem>(&e)) {
            ERC(current_subset_glyph,
                doc->get_subset_glyph(
                    current_font, glyphitem->unicode_codepoint, glyphitem->glyph_id));
            glyph_appender_lambda(current_subset_glyph);
        } else if(auto glyphtextitem = std::get_if<GlyphTextItem>(&e)) {
            ERC(current_subset_glyph,
                doc->get_subset_glyph(
                    current_font, glyphtextitem->source_text, glyphtextitem->glyph_id));
            glyph_appender_lambda(current_subset_glyph);
        } else {
            fprintf(stderr, "Not implemented yet.\n");
            std::abort();
        }
        is_first = false;
    }
    serialisation.append_raw("] TJ\n");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::render_text(const PdfText &textobj) {
    if(textobj.creator() != this) {
        RETERR(WrongDrawContext);
    }
    ERCV(validate_text_contents(textobj));

    CapyPDF_FontId current_font{-1};
    double current_pointsize{-1};

    ERCV(cmds.BT());
    for(const auto &e : textobj.get_events()) {
        if(std::holds_alternative<TStar_arg>(e)) {
            cmds.append("T*");
        } else if(auto *tc = std::get_if<Tc_arg>(&e)) {
            cmds.append_command(tc->val, "Tc");
        } else if(auto *td = std::get_if<Td_arg>(&e)) {
            cmds.append_command(td->tx, td->ty, "Td");
        } else if(auto *tD = std::get_if<TD_arg>(&e)) {
            cmds.append_command(tD->tx, tD->ty, "TD");
        } else if(auto *tf = std::get_if<Tf_arg>(&e)) {
            current_font = tf->font;
            current_pointsize = tf->pointsize;
            auto cmd = pystd2025::format(
                "/SFont%d %f Tf\n", doc->get(current_font).font_obj, current_pointsize);
            cmds.append(cmd);
            FontSubset fs;
            fs.subset_id = 0;
            fs.fid = current_font;
            used_subset_fonts.insert(fs);
        } else if(auto *tj = std::get_if<Tj_arg>(&e)) {
            cmds.append_indent();
            cmds.append_raw("<");
            pystd2025::CString tmp;
            for(const auto c : tj->text) {
                ERC(current_subset_glyph, doc->get_subset_glyph(current_font, c, {}));
                pystd2025::format_append(tmp, "%04X", current_subset_glyph.glyph_id);
            }
            cmds.append_raw(tmp);
            cmds.append_raw("> Tj\n");
        } else if(auto *tJ = std::get_if<TJ_arg>(&e)) {
            ERCV((serialize_charsequence(tJ->elements, cmds, current_font)));
        } else if(auto *tL = std::get_if<TL_arg>(&e)) {
            cmds.append_command(tL->leading, "TL");
        } else if(auto *tm_ = std::get_if<Tm_arg>(&e)) {
            auto &tm = *tm_;
            auto cmd = pystd2025::format(
                "%f %f %f %f %f %f Tm\n", tm.m.a, tm.m.b, tm.m.c, tm.m.d, tm.m.e, tm.m.f);
            cmds.append(cmd);
        } else if(auto *tr = std::get_if<Tr_arg>(&e)) {
            cmds.append_command((int)tr->rmode, "Tr");
        } else if(auto *ts = std::get_if<Ts_arg>(&e)) {
            cmds.append_command(ts->rise, "Ts");
        } else if(auto *tz = std::get_if<Tz_arg>(&e)) {
            cmds.append_command(tz->scaling, "Tz");
        } else if(auto *sitem = std::get_if<StructureItem>(&e)) {
            // FIXME, convert to a serialize method and make
            // this and cmd_BDC use that.
            ERC(mcid_id, add_bcd_structure(sitem->sid));
            auto item = doc->structure_items.at(sitem->sid.id).stype;
            if(auto itemid = std::get_if<CapyPDF_Structure_Type>(&item)) {
                const auto &itemstr = structure_type_names.at(*itemid);
                auto cmd = pystd2025::format("/%s << /MCID %d >>\n", itemstr, mcid_id);
                cmds.append(cmd);
                cmds.append("BDC");
            } else if(auto ri = std::get_if<CapyPDF_RoleId>(&item)) {
                const auto &role = *ri;
                auto rolename = bytes2pdfstringliteral(doc->rolemap.at(role.id).name.view());
                auto cmd = pystd2025::format("%s << /MCID %d >>\n", rolename.c_str(), mcid_id);
                cmds.append(cmd);
                cmds.append("BDC");
            } else {
                fprintf(stderr, "FIXME 1\n");
                std::abort();
            }
            ERCV(cmds.indent(DrawStateType::MarkedContent));
        } else if(std::holds_alternative<Emc_arg>(e)) {
            cmds.EMC();
        } else if(auto *sarg_ = std::get_if<Stroke_arg>(&e)) {
            auto &sarg = *sarg_;
            if(auto rgb = std::get_if<DeviceRGBColor>(&sarg.c)) {
                ERCV(serialize_RG(rgb->r, rgb->g, rgb->b));
            } else if(auto gray = std::get_if<DeviceGrayColor>(&sarg.c)) {
                ERCV(serialize_G(gray->v));
            } else if(auto cmyk = std::get_if<DeviceCMYKColor>(&sarg.c)) {
                ERCV(serialize_K(cmyk->c, cmyk->m, cmyk->y, cmyk->k));
            } else if(auto icc = std::get_if<ICCColor>(&sarg.c)) {
                CHECK_INDEXNESS(icc->id.id, doc->icc_profiles);
                const auto &icc_info = doc->get(icc->id);
                if(icc_info.num_channels != (int32_t)icc->values.size()) {
                    RETERR(IncorrectColorChannelCount);
                }
                used_colorspaces.insert(icc_info.object_num);
                pystd2025::CString cmd;
                pystd2025::format_append(cmd, "/CSpace%d CS\n", icc_info.object_num);
                cmds.append(cmd);
                cmd.clear();

                for(const auto &i : icc->values) {
                    pystd2025::format_append(cmd, "%f ", i);
                }
                cmd += " SCN";
                cmds.append(cmd);
            } else if(auto id = std::get_if<CapyPDF_PatternId>(&sarg.c)) {
                used_patterns.insert(id->id);
                cmds.append("/Pattern CS\n");
                auto cmd = pystd2025::format("/Pattern-%d SCN\n", id->id);
                cmds.append(cmd);
            } else {
                printf("Given text stroke colorspace not supported yet.\n");
                std::abort();
            }
        } else if(auto *nsarg_ = std::get_if<Nonstroke_arg>(&e)) {
            auto &nsarg = *nsarg_;

            if(auto rgb = std::get_if<DeviceRGBColor>(&nsarg.c)) {
                ERCV(serialize_rg(rgb->r, rgb->g, rgb->b));
            } else if(auto gray = std::get_if<DeviceGrayColor>(&nsarg.c)) {
                ERCV(serialize_g(gray->v));
            } else if(auto cmyk = std::get_if<DeviceCMYKColor>(&nsarg.c)) {
                ERCV(serialize_k(cmyk->c, cmyk->m, cmyk->y, cmyk->k));
            } else if(auto icc = std::get_if<ICCColor>(&nsarg.c)) {
                CHECK_INDEXNESS(icc->id.id, doc->icc_profiles);
                const auto &icc_info = doc->get(icc->id);
                if(icc_info.num_channels != (int32_t)icc->values.size()) {
                    RETERR(IncorrectColorChannelCount);
                }
                used_colorspaces.insert(icc_info.object_num);
                auto cmd = pystd2025::format("/CSpace%d cs\n", icc_info.object_num);
                for(const auto &i : icc->values) {
                    pystd2025::format_append(cmd, "%f ", i);
                }
                cmd += "scn";
                cmds.append(cmd);
            } else if(auto id = std::get_if<CapyPDF_PatternId>(&nsarg.c)) {
                used_patterns.insert(id->id);
                cmds.append("/Pattern cs\n");
                auto cmd = pystd2025::format("/Pattern-%d scn\n", id->id);
                cmds.append(cmd);
            } else {
                printf("Given text nonstroke colorspace not supported yet.\n");
                std::abort();
            }
        } else if(auto *w = std::get_if<w_arg>(&e)) {
            cmds.append_command(w->width, "w");
        } else if(auto *M = std::get_if<M_arg>(&e)) {
            cmds.append_command(M->miterlimit, "M");
        } else if(auto *j = std::get_if<j_arg>(&e)) {
            CHECK_ENUM(j->join_style, CAPY_LJ_BEVEL);
            cmds.append_command((int)j->join_style, "j");
        } else if(auto *J = std::get_if<J_arg>(&e)) {
            CHECK_ENUM(J->cap_style, CAPY_LC_PROJECTION);
            cmds.append_command((int)J->cap_style, "J");
        } else if(auto *dash = std::get_if<d_arg>(&e)) {
            if(dash->array.size() == 0) {
                RETERR(ZeroLengthArray);
            }
            for(auto val : dash->array) {
                if(val < 0) {
                    RETERR(NegativeDash);
                }
            }
            pystd2025::CString cmd("[ ");
            for(auto val : dash->array) {
                pystd2025::format_append(cmd, "%f ", val);
            }
            pystd2025::format_append(cmd, " ] %f d\n", dash->phase);
            cmds.append(cmd);
        } else if(auto *gs = std::get_if<gs_arg>(&e)) {
            CHECK_INDEXNESS(gs->gid.id, doc->document_objects);
            used_gstates.insert(gs->gid.id);
            auto cmd = pystd2025::format("/GS%d gs\n", gs->gid.id);
            cmds.append(cmd);
        }
    }
    ERCV(cmds.ET());
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::validate_text_contents(const PdfText &text) {
    pystd2025::Optional<CapyPDF_FontId> font;
    for(const auto &e : text.get_events()) {
        if(const auto *Tf = std::get_if<Tf_arg>(&e)) {
            font = Tf->font;
        } else if(const auto *text_arg = std::get_if<Tj_arg>(&e)) {
            if(!font) {
                RETERR(FontNotSpecified);
                for(const auto &codepoint : text_arg->text) {
                    if(!doc->font_has_character(font.value(), codepoint)) {
                        RETERR(MissingGlyph);
                    }
                }
            }
        } else if(const auto *TJ = std::get_if<TJ_arg>(&e)) {
            if(!font) {
                RETERR(FontNotSpecified);
            }
            for(const auto &te : TJ->elements) {
                if(const auto *unicode = std::get_if<UnicodeCharacter>(&te)) {
                    if(!doc->font_has_character(font.value(), unicode->codepoint)) {
                        RETERR(MissingGlyph);
                    } else if(const auto *u8str = std::get_if<u8string>(&te)) {
                        for(const auto &codepoint : *u8str) {
                            if(!doc->font_has_character(font.value(), codepoint)) {
                                RETERR(MissingGlyph);
                            }
                        }
                    }
                }
            }
        }
    }
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::render_glyphs(const std::vector<PdfGlyph> &glyphs,
                                                  CapyPDF_FontId fid,
                                                  double pointsize) {
    CHECK_INDEXNESS(fid.id, doc->font_objects);
    double prev_x = 0;
    double prev_y = 0;
    if(glyphs.empty()) {
        RETOK;
    }
    auto &font_data = doc->get(fid);
    // FIXME, do per character.
    // const auto &bob =
    //    doc->font_objects.at(doc->get_subset_glyph(fid,
    //    glyphs.front().codepoint).ss.fid.id);
    cmds.append("BT");
    ERCV(cmds.indent(DrawStateType::Text));
    auto cmd = pystd2025::format("/SFont%d %f Tf", font_data.font_obj, pointsize);
    cmds.append(cmd);
    for(const auto &g : glyphs) {
        ERC(current_subset_glyph, doc->get_subset_glyph(fid, g.codepoint, {}));
        // const auto &bob = doc->font_objects.at(current_subset_glyph.ss.fid.id);
        used_subset_fonts.insert(current_subset_glyph.ss);
        cmds.append_command(g.x - prev_x, g.y - prev_y, "Td");
        prev_x = g.x;
        prev_y = g.y;
        cmd.clear();
        pystd2025::format_append(cmd, "<%04x>", (unsigned char)current_subset_glyph.glyph_id);
        cmds.append_command(pystd2025::CStringView(cmd.data(), cmd.size()), "Tj");
    }
    ERCV(cmds.dedent(DrawStateType::Text));
    cmds.append("ET");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::render_pdfdoc_text_builtin(const char *pdfdoc_encoded_text,
                                                               CapyPDF_Builtin_Fonts font_id,
                                                               double pointsize,
                                                               double x,
                                                               double y) {
    if(!doc->docprops.subtype.contains<pystd2025::Monostate>()) {
        RETERR(BadOperationForIntent);
    }
    auto font_object = doc->font_object_number(doc->get_builtin_font_id(font_id));
    used_fonts.insert(font_object);
    cmds.append("BT");
    ERCV(cmds.indent(DrawStateType::Text));
    auto cmd = pystd2025::format("/Font%d %f Tf", font_object, pointsize);
    cmds.append(cmd);
    cmd.clear();
    cmds.append_command(x, y, "Td");
    cmds.append_command(pdfstring_quote(pdfdoc_encoded_text), "Tj");
    ERCV(cmds.dedent(DrawStateType::Text));
    cmds.append("ET");
    RETOK;
}

void PdfDrawContext::draw_unit_circle() {
    const double control = 0.5523 / 2;
    cmd_m(0, 0.5);
    cmd_c(control, 0.5, 0.5, control, 0.5, 0);
    cmd_c(0.5, -control, control, -0.5, 0, -0.5);
    cmd_c(-control, -0.5, -0.5, -control, -0.5, 0);
    cmd_c(-0.5, control, -control, 0.5, 0, 0.5);
}

void PdfDrawContext::draw_unit_box() { cmd_re(-0.5, -0.5, 1, 1); }

rvoe<NoReturnValue> PdfDrawContext::set_transition(const Transition &tr) {
    if(context_type != CAPY_DC_PAGE) {
        RETERR(InvalidDrawContextType);
    }
    transition = tr;
    RETOK;
}

rvoe<NoReturnValue>
PdfDrawContext::add_simple_navigation(pystd2025::Span<const CapyPDF_OptionalContentGroupId> navs,
                                      const pystd2025::Optional<Transition> &tr) {
    if(context_type != CAPY_DC_PAGE) {
        RETERR(InvalidDrawContextType);
    }
    if(!sub_navigations.empty()) {
        std::abort();
    }
    for(const auto &sn : navs) {
        if(!used_ocgs.contains(sn)) {
            RETERR(UnusedOcg);
        }
    }
    for(const auto &sn : navs) {
        sub_navigations.emplace_back(SubPageNavigation{sn, tr});
    }
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::set_custom_page_properties(const PageProperties &new_props) {
    if(context_type != CAPY_DC_PAGE) {
        RETERR(InvalidDrawContextType);
    }
    custom_props = new_props;
    RETOK;
}

rvoe<NoReturnValue>
PdfDrawContext::set_transparency_properties(const TransparencyGroupProperties &props) {
    if(!(context_type == CAPY_DC_PAGE || context_type == CAPY_DC_TRANSPARENCY_GROUP)) {
        RETERR(WrongDCForTransp);
    }
    // This is not the greatest solution, but store the value in
    // page properties even for a transparency group as we already have it.
    // Having two variables for the same thing would be more confusing.
    custom_props.transparency_props = props;
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::set_group_matrix(const PdfMatrix &mat) {
    if(!(context_type == CAPY_DC_TRANSPARENCY_GROUP || context_type == CAPY_DC_COLOR_TILING ||
         context_type == CAPY_DC_FORM_XOBJECT)) {
        RETERR(WrongDCForMatrix);
    }
    group_matrix = mat;
    RETOK;
}

rvoe<int32_t> PdfDrawContext::add_bcd_structure(CapyPDF_StructureItemId sid) {
    for(const auto &id : used_structures) {
        if(id == sid) {
            RETERR(StructureReuse);
        }
    }
    used_structures.push_back(sid);
    return (int32_t)used_structures.size() - 1;
}

} // namespace capypdf::internal
