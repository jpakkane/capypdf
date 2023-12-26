/*
 * Copyright 2022-2023 Jussi Pakkanen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <pdfdrawcontext.hpp>
#include <pdfgen.hpp>
#include <ft2build.h>
#include <string_view>
#include FT_FREETYPE_H
#include FT_IMAGE_H
#include <utils.hpp>
#include <fmt/core.h>
#include <array>
#include <cmath>
#include <cassert>
#include <memory>

namespace capypdf {

GstatePopper::~GstatePopper() { ctx->cmd_Q(); }

PdfDrawContext::PdfDrawContext(
    PdfDocument *doc, PdfColorConverter *cm, CapyPDF_Draw_Context_Type dtype, double w, double h)
    : doc(doc), cm(cm), context_type{dtype}, cmd_appender(commands), w{w}, h{h} {}

PdfDrawContext::~PdfDrawContext() {}

DCSerialization PdfDrawContext::serialize(const TransparencyGroupExtra *trinfo) {
    SerializedBasicContext sc;
    sc.dict = build_resource_dict();
    if(context_type == CAPY_DC_FORM_XOBJECT) {
        std::string dict = fmt::format(
            R"(<<
  /Type /XObject
  /Subtype /Form
  /BBox [ {:f} {:f} {:f} {:f} ]
  /Resources {}
  /Length {}
>>
)",
            0.0,
            0.0,
            w,
            h,
            sc.dict,
            commands.size());
        return SerializedXObject{std::move(dict), commands};
    } else if(context_type == CAPY_DC_TRANSPARENCY_GROUP) {
        std::string dict = R"(<<
  /Type /XObject
  /Subtype /Form
)";
        auto app = std::back_inserter(dict);
        fmt::format_to(app, "  /BBox [ {:f} {:f} {:f} {:f} ]\n", 0.0, 0.0, w, h);
        dict += R"(  /Group <<
    /S /Transparency
)";
        if(trinfo) {
            if(trinfo->I) {
                fmt::format_to(app, "    /I {}\n", *trinfo->I ? "true" : "false");
            }
            if(trinfo->K) {
                fmt::format_to(app, "    /K {}\n", *trinfo->K ? "true" : "false");
            }
        }
        fmt::format_to(app,
                       R"(  >>
  /Resources {}
  /Length {}
>>
)",
                       sc.dict,
                       commands.size());
        return SerializedXObject{std::move(dict), commands};
    } else {
        sc.commands = fmt::format(
            R"(<<
  /Length {}
>>
stream
{}
endstream
)",
            commands.size(),
            commands);
    }
    return sc;
}

void PdfDrawContext::clear() {
    commands.clear();
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
    ind.clear();
    sub_navigations.clear();
    transition.reset();
    is_finalized = false;
    uses_all_colorspace = false;
    custom_props = PageProperties{};
}

std::string PdfDrawContext::build_resource_dict() {
    std::string resources;
    auto resource_appender = std::back_inserter(resources);
    resources = "<<\n";
    if(!used_images.empty() || !used_form_xobjects.empty() || !used_trgroups.empty()) {
        resources += "  /XObject <<\n";
        if(!used_images.empty()) {
            for(const auto &i : used_images) {
                fmt::format_to(resource_appender, "    /Image{} {} 0 R\n", i, i);
            }
        }
        if(!used_form_xobjects.empty()) {
            for(const auto &fx : used_form_xobjects) {
                fmt::format_to(resource_appender, "    /FXO{} {} 0 R\n", fx, fx);
            }
        }
        if(!used_trgroups.empty()) {
            for(const auto &tg : used_trgroups) {
                auto objnum = doc->transparency_groups.at(tg.id);
                fmt::format_to(resource_appender, "    /TG{} {} 0 R\n", objnum, objnum);
            }
        }
        resources += "  >>\n";
    }
    if(!used_fonts.empty() || !used_subset_fonts.empty()) {
        resources += "  /Font <<\n";
        for(const auto &i : used_fonts) {
            fmt::format_to(resource_appender, "    /Font{} {} 0 R\n", i, i);
        }
        for(const auto &i : used_subset_fonts) {
            const auto &bob = doc->font_objects.at(i.fid.id);
            fmt::format_to(resource_appender,
                           "    /SFont{}-{} {} 0 R\n",
                           bob.font_obj,
                           i.subset_id,
                           bob.font_obj);
        }
        resources += "  >>\n";
    }
    if(!used_colorspaces.empty() || uses_all_colorspace) {
        resources += "  /ColorSpace <<\n";
        if(uses_all_colorspace) {
            fmt::format_to(resource_appender, "    /All {} 0 R\n", doc->separation_objects.at(0));
        }
        for(const auto &i : used_colorspaces) {
            fmt::format_to(resource_appender, "    /CSpace{} {} 0 R\n", i, i);
        }
        resources += "  >>\n";
    }
    if(!used_gstates.empty()) {
        resources += "  /ExtGState <<\n";
        for(const auto &s : used_gstates) {
            fmt::format_to(resource_appender, "    /GS{} {} 0 R \n", s, s);
        }
        resources += "  >>\n";
    }
    if(!used_shadings.empty()) {
        resources += "  /Shading <<\n";
        for(const auto &s : used_shadings) {
            fmt::format_to(resource_appender, "    /SH{} {} 0 R\n", s, s);
        }
        resources += "  >>\n";
    }
    if(!used_patterns.empty()) {
        resources += "  /Pattern <<\n";
        for(const auto &s : used_patterns) {
            fmt::format_to(resource_appender, "    /Pattern-{} {} 0 R\n", s, s);
        }
        resources += "  >>\n";
    }
    if(!used_ocgs.empty()) {
        resources += "  /Properties <<\n";
        for(const auto &ocg : used_ocgs) {
            auto objnum = doc->ocg_object_number(ocg);
            fmt::format_to(resource_appender, "    /oc{} {} 0 R\n", objnum, objnum);
        }
        resources += "  >>\n";
    }
    resources += ">>\n";
    return resources;
}

rvoe<NoReturnValue> PdfDrawContext::add_form_widget(CapyPDF_FormWidgetId widget) {
    if(used_widgets.find(widget) != used_widgets.end()) {
        RETERR(AnnotationReuse);
    }
    used_widgets.insert(widget);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::annotate(CapyPDF_AnnotationId annotation) {
    if(used_annotations.find(annotation) != used_annotations.end()) {
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
    commands += ind;
    commands += "b\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_B() {
    commands += ind;
    commands += "B\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_bstar() {
    commands += ind;
    commands += "b*\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Bstar() {
    commands += ind;
    commands += "B*\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_BDC(const asciistring &name, CapyPDF_StructureItemId sid) {
    ++marked_depth;
    ERC(MCID_id, add_bcd_structure(sid));
    fmt::format_to(cmd_appender,
                   R"({}/{} << /MCID {} >>
{}BDC
)",
                   ind,
                   name.sv(),
                   MCID_id,
                   ind);
    indent(DrawStateType::MarkedContent);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_BDC(CapyPDF_OptionalContentGroupId ocgid) {
    ++marked_depth;
    used_ocgs.insert(ocgid);
    fmt::format_to(cmd_appender, "{}/OC /oc{} BDC\n", ind, doc->ocg_object_number(ocgid));
    indent(DrawStateType::MarkedContent);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_BMC(std::string_view tag) {
    if(tag.size() < 2 or tag.front() != '/') {
        RETERR(BadBMC);
    }
    ++marked_depth;
    fmt::format_to(cmd_appender, "{}{} BMC\n", ind, tag);
    indent(DrawStateType::MarkedContent);
    RETOK;
}

rvoe<NoReturnValue>
PdfDrawContext::cmd_c(double x1, double y1, double x2, double y2, double x3, double y3) {
    fmt::format_to(
        cmd_appender, "{}{:f} {:f} {:f} {:f} {:f} {:f} c\n", ind, x1, y1, x2, y2, x3, y3);
    RETOK;
}

rvoe<NoReturnValue>
PdfDrawContext::cmd_cm(double m1, double m2, double m3, double m4, double m5, double m6) {
    fmt::format_to(
        cmd_appender, "{}{:f} {:f} {:f} {:f} {:f} {:f} cm\n", ind, m1, m2, m3, m4, m5, m6);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_CS(std::string_view cspace_name) {
    fmt::format_to(cmd_appender, "{}{} CS\n", ind, cspace_name);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_cs(std::string_view cspace_name) {
    fmt::format_to(cmd_appender, "{}{} cs\n", ind, cspace_name);
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
    commands += ind;
    commands += "[ ";
    for(size_t i = 0; i < dash_array_length; ++i) {
        fmt::format_to(cmd_appender, "{:f} ", dash_array[i]);
    }
    fmt::format_to(cmd_appender, " ] {} d\n", phase);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Do(CapyPDF_FormXObjectId fxoid) {
    if(context_type != CAPY_DC_PAGE) {
        RETERR(InvalidDrawContextType);
    }
    CHECK_INDEXNESS(fxoid.id, doc->form_xobjects);
    fmt::format_to(cmd_appender, "{}/FXO{} Do\n", ind, doc->form_xobjects[fxoid.id].xobj_num);
    used_form_xobjects.insert(doc->form_xobjects[fxoid.id].xobj_num);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Do(CapyPDF_TransparencyGroupId trid) {
    if(context_type != CAPY_DC_PAGE) {
        RETERR(InvalidDrawContextType);
    }
    CHECK_INDEXNESS(trid.id, doc->transparency_groups);
    fmt::format_to(cmd_appender, "{}/TG{} Do\n", ind, doc->transparency_groups[trid.id]);
    used_trgroups.insert(trid);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_EMC() {
    if(marked_depth == 0) {
        RETERR(EmcOnEmpty);
    }
    --marked_depth;
    ERCV(dedent(DrawStateType::MarkedContent));
    commands += ind;
    commands += "EMC\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_f() {
    commands += ind;
    commands += "f\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_fstar() {
    commands += ind;
    commands += "f*\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_G(LimitDouble gray) {
    return serialize_G(cmd_appender, ind, gray);
}

rvoe<NoReturnValue> PdfDrawContext::cmd_g(LimitDouble gray) {
    return serialize_g(cmd_appender, ind, gray);
}

rvoe<NoReturnValue> PdfDrawContext::cmd_gs(CapyPDF_GraphicsStateId gid) {
    CHECK_INDEXNESS(gid.id, doc->document_objects);
    used_gstates.insert(gid.id);
    fmt::format_to(cmd_appender, "{}/GS{} gs\n", ind, gid.id);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_h() {
    commands += ind;
    commands += "h\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_i(double flatness) {
    if(flatness < 0 || flatness > 100) {
        RETERR(InvalidFlatness);
    }
    fmt::format_to(cmd_appender, "{}{:f} i\n", ind, flatness);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_j(CapyPDF_Line_Join join_style) {
    CHECK_ENUM(join_style, CAPY_LJ_BEVEL);
    fmt::format_to(cmd_appender, "{}{} j\n", ind, (int)join_style);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_J(CapyPDF_Line_Cap cap_style) {
    CHECK_ENUM(cap_style, CAPY_LC_PROJECTION);
    fmt::format_to(cmd_appender, "{}{} J\n", ind, (int)cap_style);
    RETOK;
}

rvoe<NoReturnValue>
PdfDrawContext::cmd_K(LimitDouble c, LimitDouble m, LimitDouble y, LimitDouble k) {
    return serialize_K(cmd_appender, ind, c, m, y, k);
}

rvoe<NoReturnValue>
PdfDrawContext::cmd_k(LimitDouble c, LimitDouble m, LimitDouble y, LimitDouble k) {
    return serialize_k(cmd_appender, ind, c, m, y, k);
}

rvoe<NoReturnValue> PdfDrawContext::cmd_l(double x, double y) {
    fmt::format_to(cmd_appender, "{}{:f} {:f} l\n", ind, x, y);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_m(double x, double y) {
    fmt::format_to(cmd_appender, "{}{:f} {:f} m\n", ind, x, y);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_M(double miterlimit) {
    fmt::format_to(cmd_appender, "{}{:f} M\n", ind, miterlimit);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_n() {
    commands += ind;
    commands += "n\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_q() {
    commands += ind;
    commands += "q\n";
    indent(DrawStateType::SaveState);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Q() {
    ERCV(dedent(DrawStateType::SaveState));
    commands += ind;
    commands += "Q\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_re(double x, double y, double w, double h) {
    fmt::format_to(cmd_appender, "{}{:f} {:f} {:f} {:f} re\n", ind, x, y, w, h);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_RG(LimitDouble r, LimitDouble g, LimitDouble b) {
    return serialize_RG(cmd_appender, ind, r, g, b);
}

rvoe<NoReturnValue> PdfDrawContext::cmd_rg(LimitDouble r, LimitDouble g, LimitDouble b) {
    return serialize_rg(cmd_appender, ind, r, g, b);
}

rvoe<NoReturnValue> PdfDrawContext::cmd_ri(CapyPDF_Rendering_Intent ri) {
    CHECK_ENUM(ri, CAPY_RI_PERCEPTUAL);
    fmt::format_to(cmd_appender, "{}/{} ri\n", ind, rendering_intent_names.at((int)ri));
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_s() {
    commands += ind;
    commands += "s\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_S() {
    commands += ind;
    commands += "S\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_SCN(double value) {
    fmt::format_to(cmd_appender, "{}{:f} SCN\n", ind, value);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_scn(double value) {
    fmt::format_to(cmd_appender, "{}{:f} scn\n", ind, value);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_sh(CapyPDF_ShadingId shid) {
    CHECK_INDEXNESS(shid.id, doc->document_objects);
    used_shadings.insert(shid.id);
    fmt::format_to(cmd_appender, "{}/SH{} sh\n", ind, shid.id);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Tr(CapyPDF_Text_Mode mode) {
    CHECK_ENUM(mode, CAPY_TEXT_CLIP);
    fmt::format_to(cmd_appender, "{}{} Tr\n", ind, (int)mode);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_v(double x2, double y2, double x3, double y3) {
    fmt::format_to(cmd_appender, "{}{:f} {:f} {:f} {:f} v\n", ind, x2, y2, x3, y3);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_w(double w) {
    if(w < 0) {
        RETERR(NegativeLineWidth);
    }
    fmt::format_to(cmd_appender, "{}{:f} w\n", ind, w);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_W() {
    commands += ind;
    commands += "W\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_Wstar() {
    commands += ind;
    commands += "W*\n";
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::cmd_y(double x1, double y1, double x3, double y3) {
    fmt::format_to(cmd_appender, "{}{:f} {:f} {:f} {:f} y\n", ind, x1, y1, x3, y3);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::serialize_G(std::back_insert_iterator<std::string> &out,
                                                std::string_view indent,
                                                LimitDouble gray) const {
    fmt::format_to(out, "{}{:f} G\n", indent, gray.v());
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::serialize_g(std::back_insert_iterator<std::string> &out,
                                                std::string_view indent,
                                                LimitDouble gray) const {
    fmt::format_to(out, "{}{:f} g\n", indent, gray.v());
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::serialize_K(std::back_insert_iterator<std::string> &out,
                                                std::string_view indent,
                                                LimitDouble c,
                                                LimitDouble m,
                                                LimitDouble y,
                                                LimitDouble k) const {
    fmt::format_to(out, "{}{:f} {:f} {:f} {:f} K\n", ind, c.v(), m.v(), y.v(), k.v());
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::serialize_k(std::back_insert_iterator<std::string> &out,
                                                std::string_view indent,
                                                LimitDouble c,
                                                LimitDouble m,
                                                LimitDouble y,
                                                LimitDouble k) const {
    fmt::format_to(out, "{}{:f} {:f} {:f} {:f} k\n", ind, c.v(), m.v(), y.v(), k.v());
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::serialize_RG(std::back_insert_iterator<std::string> &out,
                                                 std::string_view indent,
                                                 LimitDouble r,
                                                 LimitDouble g,
                                                 LimitDouble b) const {
    fmt::format_to(out, "{}{:f} {:f} {:f} RG\n", indent, r.v(), g.v(), b.v());
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::serialize_rg(std::back_insert_iterator<std::string> &out,
                                                 std::string_view indent,
                                                 LimitDouble r,
                                                 LimitDouble g,
                                                 LimitDouble b) const {
    fmt::format_to(out, "{}{:f} {:f} {:f} rg\n", indent, r.v(), g.v(), b.v());
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const Color &c, bool stroke) {
    if(std::holds_alternative<DeviceRGBColor>(c)) {
        return set_color(std::get<DeviceRGBColor>(c), stroke);
    } else if(std::holds_alternative<DeviceGrayColor>(c)) {
        return set_color(std::get<DeviceGrayColor>(c), stroke);
    } else if(std::holds_alternative<DeviceCMYKColor>(c)) {
        return set_color(std::get<DeviceCMYKColor>(c), stroke);
    } else if(std::holds_alternative<ICCColor>(c)) {
        return set_color(std::get<ICCColor>(c), stroke);
    } else if(std::holds_alternative<LabColor>(c)) {
        return set_color(std::get<LabColor>(c), stroke);
    } else if(std::holds_alternative<CapyPDF_PatternId>(c)) {
        return set_color(std::get<CapyPDF_PatternId>(c), stroke);
    } else if(std::holds_alternative<SeparationColor>(c)) {
        return set_color(std::get<SeparationColor>(c), stroke);
    } else {
        printf("Given colorspace not supported yet.\n");
        fflush(stdout);
        std::abort();
    }
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const DeviceRGBColor &c, bool stroke) {
    switch(doc->opts.output_colorspace) {
    case CAPY_CS_DEVICE_RGB: {
        if(stroke) {
            return cmd_RG(c.r.v(), c.g.v(), c.b.v());
        } else {
            return cmd_rg(c.r.v(), c.g.v(), c.b.v());
        }
    }
    case CAPY_CS_DEVICE_GRAY: {
        DeviceGrayColor gray = cm->to_gray(c);
        if(stroke) {
            return cmd_G(gray.v.v());
        } else {
            return cmd_g(gray.v.v());
        }
    }
    case CAPY_CS_DEVICE_CMYK: {
        ERC(cmyk, cm->to_cmyk(c));
        if(stroke) {
            return cmd_K(cmyk.c.v(), cmyk.m.v(), cmyk.y.v(), cmyk.k.v());
        } else {
            return cmd_k(cmyk.c.v(), cmyk.m.v(), cmyk.y.v(), cmyk.k.v());
        }
        RETOK;
    }
    }
    std::abort();
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const DeviceCMYKColor &c, bool stroke) {
    switch(doc->opts.output_colorspace) {
    case CAPY_CS_DEVICE_RGB: {
        auto rgb_var = cm->to_rgb(c);
        if(stroke) {
            return cmd_RG(rgb_var.r.v(), rgb_var.g.v(), rgb_var.b.v());
        } else {
            return cmd_rg(rgb_var.r.v(), rgb_var.g.v(), rgb_var.b.v());
        }
    }
    case CAPY_CS_DEVICE_GRAY: {
        DeviceGrayColor gray = cm->to_gray(c);
        if(stroke) {
            return cmd_G(gray.v.v());
        } else {
            return cmd_g(gray.v.v());
        }
    }
    case CAPY_CS_DEVICE_CMYK: {
        if(stroke) {
            return cmd_K(c.c.v(), c.m.v(), c.y.v(), c.k.v());
        } else {
            return cmd_k(c.c.v(), c.m.v(), c.y.v(), c.k.v());
        }
    }
    default:
        RETERR(Unreachable);
    }
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const ICCColor &icc, bool stroke) {
    CHECK_INDEXNESS(icc.id.id, doc->icc_profiles);
    const auto &icc_info = doc->icc_profiles.at(icc.id.id);
    if(icc_info.num_channels != (int32_t)icc.values.size()) {
        RETERR(IncorrectColorChannelCount);
    }
    used_colorspaces.insert(icc_info.object_num);
    fmt::format_to(
        cmd_appender, "{}/CSpace{} {}\n", ind, icc_info.object_num, stroke ? "CS" : "cs");
    for(const auto &i : icc.values) {
        fmt::format_to(cmd_appender, "{:f} ", i);
    }
    fmt::format_to(cmd_appender, "{}\n", stroke ? "SCN" : "scn");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const DeviceGrayColor &c, bool stroke) {
    // Assumes that switching to the gray colorspace is always ok.
    // If it is not, fix to do the same switch() as above.
    if(stroke) {
        return cmd_G(c.v.v());
    } else {
        return cmd_g(c.v.v());
    }
}

rvoe<NoReturnValue> PdfDrawContext::set_color(CapyPDF_PatternId id, bool stroke) {
    if(context_type != CAPY_DC_PAGE) {
        RETERR(PatternNotAccepted);
    }
    ERCV(cmd_cs("/Pattern"));
    used_patterns.insert(id.id);
    fmt::format_to(cmd_appender, "{}/Pattern-{} {}\n", ind, id.id, stroke ? "SCN" : "scn");
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::set_color(const SeparationColor &color, bool stroke) {
    CHECK_INDEXNESS(color.id.id, doc->separation_objects);
    const auto idnum = doc->separation_object_number(color.id);
    used_colorspaces.insert(idnum);
    std::string csname = fmt::format("/CSpace{}", idnum);
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
    std::string csname = fmt::format("/CSpace{}", c.id.id);
    cmd_CS(csname);
    fmt::format_to(
        cmd_appender, "{}{:f} {:f} {:f} {}\n", ind, c.l, c.a, c.b, stroke ? "SCN" : "scn");
    RETOK;
}

void PdfDrawContext::set_all_stroke_color() {
    uses_all_colorspace = true;
    cmd_CS("/All");
    cmd_SCN(1.0);
}

rvoe<NoReturnValue> PdfDrawContext::draw_image(CapyPDF_ImageId im_id) {
    CHECK_INDEXNESS(im_id.id, doc->image_info);
    auto obj_num = doc->image_object_number(im_id);
    used_images.insert(obj_num);
    fmt::format_to(cmd_appender, "{}/Image{} Do\n", ind, obj_num);
    RETOK;
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
    t.render_text(text);
    return render_text(t);
}

rvoe<NoReturnValue> PdfDrawContext::serialize_charsequence(const std::vector<CharItem> &charseq,
                                                           std::string &serialisation,
                                                           CapyPDF_FontId &current_font,
                                                           int32_t &current_subset,
                                                           double &current_pointsize) {
    std::back_insert_iterator<std::string> app = std::back_inserter(serialisation);
    bool is_first = true;
    for(const auto &e : charseq) {
        if(std::holds_alternative<double>(e)) {
            if(is_first) {
                serialisation += ind;
                serialisation += "[ ";
            }
            fmt::format_to(app, "{:f} ", std::get<double>(e));
        } else {
            assert(std::holds_alternative<uint32_t>(e));
            const auto codepoint = std::get<uint32_t>(e);
            ERC(current_subset_glyph, doc->get_subset_glyph(current_font, codepoint));
            used_subset_fonts.insert(current_subset_glyph.ss);
            if(current_subset_glyph.ss.subset_id != current_subset) {
                if(!is_first) {
                    serialisation += "] TJ\n";
                }
                fmt::format_to(app,
                               "{}/SFont{}-{} {} Tf\n{}[ ",
                               ind,
                               doc->font_objects.at(current_subset_glyph.ss.fid.id).font_obj,
                               current_subset_glyph.ss.subset_id,
                               current_pointsize,
                               ind);
            } else {
                if(is_first) {
                    serialisation += ind;
                    serialisation += "[ ";
                }
            }
            current_font = current_subset_glyph.ss.fid;
            current_subset = current_subset_glyph.ss.subset_id;
            fmt::format_to(app, "<{:02x}> ", current_subset_glyph.glyph_id);
        }
        is_first = false;
    }
    serialisation += "] TJ\n";
    return NoReturnValue{};
}

rvoe<NoReturnValue> PdfDrawContext::utf8_to_kerned_chars(const u8string &text,
                                                         std::vector<CharItem> &charseq,
                                                         CapyPDF_FontId fid) {
    CHECK_INDEXNESS(fid.id, doc->font_objects);
    if(text.empty()) {
        RETOK;
    }
    FT_Face face = doc->fonts.at(doc->font_objects.at(fid.id).font_index_tmp).fontdata.face.get();
    if(!face) {
        RETERR(BuiltinFontNotSupported);
    }

    uint32_t previous_codepoint = -1;
    // Freetype does not support GPOS kerning because it is context-sensitive.
    // So this method might produce incorrect kerning. Users that need precision
    // need to use the glyph based rendering method.
    const bool has_kerning = FT_HAS_KERNING(face);
    for(const auto &codepoint : text) {
        if(has_kerning && previous_codepoint != (uint32_t)-1) {
            FT_Vector kerning;
            const auto index_left = FT_Get_Char_Index(face, previous_codepoint);
            const auto index_right = FT_Get_Char_Index(face, codepoint);
            auto ec = FT_Get_Kerning(face, index_left, index_right, FT_KERNING_DEFAULT, &kerning);
            if(ec != 0) {
                RETERR(FreeTypeError);
            }
            if(kerning.x != 0) {
                // The value might be a integer, fraction or something else.
                // None of the fonts I tested had kerning that Freetype recognized,
                // so don't know if this actually works.
                charseq.emplace_back((double)kerning.x);
            }
        }
        charseq.emplace_back(codepoint);
        previous_codepoint = codepoint;
    }
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::render_text(const PdfText &textobj) {
    if(textobj.creator() != this) {
        RETERR(WrongDrawContext);
    }
    std::string serialisation{ind + "BT\n"};
    indent(DrawStateType::Text);
    std::back_insert_iterator<std::string> app = std::back_inserter(serialisation);
    int32_t current_subset{-1};
    CapyPDF_FontId current_font{-1};
    double current_pointsize{-1};

    auto visitor = overloaded{
        [&](const TStar_arg &) -> rvoe<NoReturnValue> {
            serialisation += ind;
            serialisation += "T*\n";
            return NoReturnValue{};
        },

        [&](const Tc_arg &tc) -> rvoe<NoReturnValue> {
            fmt::format_to(app, "{}{} Tc\n", ind, tc.val);
            return NoReturnValue{};
        },

        [&](const Td_arg &td) -> rvoe<NoReturnValue> {
            fmt::format_to(app, "{}{:f} {:f} Td\n", ind, td.tx, td.ty);
            return NoReturnValue{};
        },

        [&](const TD_arg &tD) -> rvoe<NoReturnValue> {
            fmt::format_to(app, "{}{:f} {:f} TD\n", ind, tD.tx, tD.ty);
            return NoReturnValue{};
        },

        [&](const Tf_arg &tf) -> rvoe<NoReturnValue> {
            current_font = tf.font;
            current_subset = -1;
            current_pointsize = tf.pointsize;
            return NoReturnValue{};
        },

        [&](const Text_arg &tj) -> rvoe<NoReturnValue> {
            std::vector<CharItem> charseq;
            ERCV(utf8_to_kerned_chars(tj.text, charseq, current_font));
            ERCV(serialize_charsequence(
                charseq, serialisation, current_font, current_subset, current_pointsize));
            return NoReturnValue{};
        },

        [&](const TJ_arg &tJ) -> rvoe<NoReturnValue> {
            auto rc = serialize_charsequence(
                tJ.elements, serialisation, current_font, current_subset, current_pointsize);
            if(!rc) {
                return std::unexpected(rc.error());
            }
            return NoReturnValue{};
        },

        [&](const TL_arg &tL) -> rvoe<NoReturnValue> {
            fmt::format_to(app, "{}{:f} TL\n", ind, tL.leading);
            return NoReturnValue{};
        },

        [&](const Tm_arg &tm) -> rvoe<NoReturnValue> {
            fmt::format_to(app,
                           "{}{:f} {:f} {:f} {:f} {:f} {:f} Tm\n",
                           ind,
                           tm.a,
                           tm.b,
                           tm.c,
                           tm.d,
                           tm.e,
                           tm.f);
            return NoReturnValue{};
        },

        [&](const Tr_arg &tr) -> rvoe<NoReturnValue> {
            fmt::format_to(app, "{}{} Tr\n", ind, (int)tr.rmode);
            return NoReturnValue{};
        },

        [&](const Ts_arg &ts) -> rvoe<NoReturnValue> {
            fmt::format_to(app, "{}{:f} Ts\n", ind, ts.rise);
            return NoReturnValue{};
        },

        [&](const Tw_arg &tw) -> rvoe<NoReturnValue> {
            fmt::format_to(app, "{}{:f} Tw\n", ind, tw.width);
            return NoReturnValue{};
        },

        [&](const Tz_arg &tz) -> rvoe<NoReturnValue> {
            fmt::format_to(app, "{}{:f} Tz\n", ind, tz.scaling);
            return NoReturnValue{};
        },

        [&](const StructureItem &sitem) -> rvoe<NoReturnValue> {
            ERC(mcid_id, add_bcd_structure(sitem.sid));
            fmt::format_to(
                app, "{}/{} << /MCID {} >>\n{}BDC\n", ind, sitem.name.sv(), mcid_id, ind);
            indent(DrawStateType::MarkedContent);
            return NoReturnValue{};
        },

        [&](const Emc_arg &) -> rvoe<NoReturnValue> {
            ERCV(dedent(DrawStateType::MarkedContent));
            fmt::format_to(app, "{}EMC\n", ind);
            return NoReturnValue{};
        },

        [&](const Stroke_arg &sarg) -> rvoe<NoReturnValue> {
            if(std::holds_alternative<DeviceRGBColor>(sarg.c)) {
                auto &rgb = std::get<DeviceRGBColor>(sarg.c);
                ERCV(serialize_RG(app, ind, rgb.r, rgb.g, rgb.b));
            } else if(std::holds_alternative<DeviceGrayColor>(sarg.c)) {
                auto &gray = std::get<DeviceGrayColor>(sarg.c);
                ERCV(serialize_G(app, ind, gray.v));
            } else if(std::holds_alternative<DeviceCMYKColor>(sarg.c)) {
                auto &cmyk = std::get<DeviceCMYKColor>(sarg.c);
                ERCV(serialize_K(app, ind, cmyk.c, cmyk.m, cmyk.y, cmyk.k));
            } else {
                printf("Given text stroke colorspace not supported yet.\n");
                std::abort();
            }
            return NoReturnValue{};
        },

        [&](const Nonstroke_arg &nsarg) -> rvoe<NoReturnValue> {
            if(std::holds_alternative<DeviceRGBColor>(nsarg.c)) {
                auto &rgb = std::get<DeviceRGBColor>(nsarg.c);
                ERCV(serialize_rg(app, ind, rgb.r, rgb.g, rgb.b));
            } else if(std::holds_alternative<DeviceGrayColor>(nsarg.c)) {
                auto &gray = std::get<DeviceGrayColor>(nsarg.c);
                ERCV(serialize_g(app, ind, gray.v));
            } else if(std::holds_alternative<DeviceCMYKColor>(nsarg.c)) {
                auto &cmyk = std::get<DeviceCMYKColor>(nsarg.c);
                ERCV(serialize_k(app, ind, cmyk.c, cmyk.m, cmyk.y, cmyk.k));
            } else {
                printf("Given text nonstroke colorspace not supported yet.\n");
                std::abort();
            }
            return NoReturnValue{};
        },
    };

    for(const auto &e : textobj.get_events()) {
        ERCV(std::visit(visitor, e));
    }
    ERCV(dedent(DrawStateType::Text));
    serialisation += ind;
    serialisation += "ET\n";
    commands += serialisation;
    RETOK;
}

void PdfDrawContext::render_raw_glyph(
    uint32_t glyph, CapyPDF_FontId fid, double pointsize, double x, double y) {
    auto &font_data = doc->font_objects.at(fid.id);
    // used_fonts.insert(font_data.font_obj);

    const auto font_glyph_id = doc->glyph_for_codepoint(
        doc->fonts.at(font_data.font_index_tmp).fontdata.face.get(), glyph);
    fmt::format_to(cmd_appender,
                   R"({}BT
{}  /Font{} {} Tf
{}  {:f} {:f} Td
{}  (\{:o}) Tj
{}ET
)",
                   ind,
                   ind,
                   font_data.font_obj,
                   pointsize,
                   ind,
                   x,
                   y,
                   ind,
                   font_glyph_id,
                   ind);
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
    auto &font_data = doc->font_objects.at(fid.id);
    // FIXME, do per character.
    // const auto &bob =
    //    doc->font_objects.at(doc->get_subset_glyph(fid, glyphs.front().codepoint).ss.fid.id);
    fmt::format_to(cmd_appender,
                   R"({}BT
{}  /SFont{}-{} {:f} Tf
)",
                   ind,
                   ind,
                   font_data.font_obj,
                   0,
                   pointsize);
    for(const auto &g : glyphs) {
        ERC(current_subset_glyph, doc->get_subset_glyph(fid, g.codepoint));
        // const auto &bob = doc->font_objects.at(current_subset_glyph.ss.fid.id);
        used_subset_fonts.insert(current_subset_glyph.ss);
        fmt::format_to(cmd_appender, "  {:f} {:f} Td\n", g.x - prev_x, g.y - prev_y);
        prev_x = g.x;
        prev_y = g.y;
        fmt::format_to(
            cmd_appender, "  <{:02x}> Tj\n", (unsigned char)current_subset_glyph.glyph_id);
    }
    fmt::format_to(cmd_appender, "{}ET\n", ind);
    RETOK;
}

rvoe<NoReturnValue> PdfDrawContext::render_pdfdoc_text_builtin(const char *pdfdoc_encoded_text,
                                                               CapyPDF_Builtin_Fonts font_id,
                                                               double pointsize,
                                                               double x,
                                                               double y) {
    if(doc->opts.subtype) {
        RETERR(BadOperationForIntent);
    }
    auto font_object = doc->font_object_number(doc->get_builtin_font_id(font_id));
    used_fonts.insert(font_object);
    fmt::format_to(cmd_appender,
                   R"({}BT
{}  /Font{} {} Tf
{}  {:f} {:f} Td
{}  {} Tj
{}ET
)",
                   ind,
                   ind,
                   font_object,
                   pointsize,
                   ind,
                   x,
                   y,
                   ind,
                   pdfstring_quote(pdfdoc_encoded_text),
                   ind);
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
    return NoReturnValue{};
}

rvoe<NoReturnValue>
PdfDrawContext::add_simple_navigation(std::span<const CapyPDF_OptionalContentGroupId> navs,
                                      const std::optional<Transition> &tr) {
    if(context_type != CAPY_DC_PAGE) {
        RETERR(InvalidDrawContextType);
    }
    if(!sub_navigations.empty()) {
        std::abort();
    }
    for(const auto &sn : navs) {
        if(used_ocgs.find(sn) == used_ocgs.end()) {
            RETERR(UnusedOcg);
        }
    }
    for(const auto &sn : navs) {
        sub_navigations.emplace_back(SubPageNavigation{sn, tr});
    }
    return NoReturnValue();
}

rvoe<NoReturnValue> PdfDrawContext::set_custom_page_properties(const PageProperties &new_props) {
    if(context_type != CAPY_DC_PAGE) {
        RETERR(InvalidDrawContextType);
    }
    custom_props = new_props;
    return NoReturnValue{};
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

} // namespace capypdf
