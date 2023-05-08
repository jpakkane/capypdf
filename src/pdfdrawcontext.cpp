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
#include FT_FREETYPE_H
#include FT_IMAGE_H
#include <utils.hpp>
#include <lcms2.h>
#include <iconv.h>
#include <fmt/core.h>
#include <array>
#include <cmath>
#include <cassert>
#include <memory>

namespace A4PDF {

GstatePopper::~GstatePopper() { ctx->cmd_Q(); }

PdfDrawContext::PdfDrawContext(
    PdfDocument *doc, PdfColorConverter *cm, A4PDF_Draw_Context_Type dtype, double w, double h)
    : doc(doc), cm(cm), context_type{dtype}, cmd_appender(commands), form_xobj_w{w},
      form_xobj_h{h} {}

PdfDrawContext::~PdfDrawContext() {}

void PdfDrawContext::set_form_xobject_size(double w, double h) {
    assert(context_type == A4PDF_Form_XObject_Context);
    form_xobj_w = w;
    form_xobj_h = h;
}

DCSerialization PdfDrawContext::serialize() {
    SerializedBasicContext sc;
    sc.dict = build_resource_dict();
    if(context_type == A4PDF_Form_XObject_Context) {
        std::string dict = fmt::format(
            R"(<<
  /Type /XObject
  /Subtype /Form
  /BBox [ {} {} {} {} ]
  /Resources {}
  /Length {}
>>
)",
            0,
            0,
            form_xobj_w,
            form_xobj_h,
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
    ind.clear();
    is_finalized = false;
    uses_all_colorspace = false;
}

std::string PdfDrawContext::build_resource_dict() {
    std::string resources;
    auto resource_appender = std::back_inserter(resources);
    resources = "<<\n";
    if(!used_images.empty() || !used_form_xobjects.empty()) {
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
    resources += ">>\n";
    return resources;
}

ErrorCode PdfDrawContext::add_form_widget(A4PDF_FormWidgetId widget) {
    if(used_widgets.find(widget) != used_widgets.end()) {
        return ErrorCode::AnnotationReuse;
    }
    used_widgets.insert(widget);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::annotate(A4PDF_AnnotationId annotation) {
    if(used_annotations.find(annotation) != used_annotations.end()) {
        return ErrorCode::AnnotationReuse;
    }
    used_annotations.insert(annotation);
    return ErrorCode::NoError;
}

GstatePopper PdfDrawContext::push_gstate() {
    cmd_q();
    return GstatePopper(this);
}

ErrorCode PdfDrawContext::cmd_b() {
    commands += ind;
    commands += "b\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_B() {
    commands += ind;
    commands += "B\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_bstar() {
    commands += ind;
    commands += "b*\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_Bstar() {
    commands += ind;
    commands += "B*\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_BDC(A4PDF_StructureItemId sid) {
    ++marked_depth;
    used_structures.insert(sid);
    fmt::format_to(cmd_appender,
                   R"({}/P << /MCID {} >>
{}BDC
)",
                   ind,
                   sid.id,
                   ind);
    indent(DrawStateType::MarkedContent);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_BMC(std::string_view tag) {
    if(tag.empty() || tag.front() != '/') {
        std::abort();
    }
    ++marked_depth;
    fmt::format_to(cmd_appender, "{}{} BMC\n", ind, tag);
    indent(DrawStateType::MarkedContent);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_c(double x1, double y1, double x2, double y2, double x3, double y3) {
    fmt::format_to(cmd_appender, "{}{} {} {} {} {} {} c\n", ind, x1, y1, x2, y2, x3, y3);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_cm(double m1, double m2, double m3, double m4, double m5, double m6) {
    fmt::format_to(cmd_appender,
                   "{}{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} cm\n",
                   ind,
                   m1,
                   m2,
                   m3,
                   m4,
                   m5,
                   m6);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_CS(std::string_view cspace_name) {
    fmt::format_to(cmd_appender, "{}{} CS\n", ind, cspace_name);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_cs(std::string_view cspace_name) {
    fmt::format_to(cmd_appender, "{}{} cs\n", ind, cspace_name);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_d(double *dash_array, size_t dash_array_length, double phase) {
    if(dash_array_length == 0) {
        return ErrorCode::ZeroLengthArray;
    }
    for(size_t i = 0; i < dash_array_length; ++i) {
        if(dash_array[i] < 0) {
            return ErrorCode::NegativeDash;
        }
    }
    commands += ind;
    commands += "[ ";
    for(size_t i = 0; i < dash_array_length; ++i) {
        fmt::format_to(cmd_appender, "{} ", dash_array[i]);
    }
    fmt::format_to(cmd_appender, " ] {} d\n", phase);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_Do(A4PDF_FormXObjectId fxoid) {
    if(context_type != A4PDF_Page_Context) {
        return ErrorCode::InvalidDrawContextType;
    }
    CHECK_INDEXNESS(fxoid.id, doc->form_xobjects);
    fmt::format_to(cmd_appender, "{}/FXO{} Do\n", ind, doc->form_xobjects[fxoid.id].xobj_num);
    used_form_xobjects.insert(doc->form_xobjects[fxoid.id].xobj_num);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_EMC() {
    if(marked_depth == 0) {
        return ErrorCode::EmcOnEmpty;
    }
    --marked_depth;
    auto rc = dedent(DrawStateType::MarkedContent);
    if(!rc) {
        return rc.error();
    }
    commands += ind;
    commands += "EMC\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_f() {
    commands += ind;
    commands += "f\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_fstar() {
    commands += ind;
    commands += "f*\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_G(double gray) {
    CHECK_COLORCOMPONENT(gray);
    fmt::format_to(cmd_appender, "{}{} G\n", gray, ind);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_g(double gray) {
    CHECK_COLORCOMPONENT(gray);
    fmt::format_to(cmd_appender, "{}{} g\n", gray, ind);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_gs(GstateId gid) {
    CHECK_INDEXNESS(gid.id, doc->document_objects);
    used_gstates.insert(gid.id);
    fmt::format_to(cmd_appender, "{}/GS{} gs\n", gid.id, ind);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_h() {
    commands += ind;
    commands += "h\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_i(double flatness) {
    if(flatness < 0 || flatness > 100) {
        return ErrorCode::InvalidFlatness;
    }
    fmt::format_to(cmd_appender, "{}{} i\n", ind, flatness);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_j(A4PDF_Line_Join join_style) {
    CHECK_ENUM(join_style, A4PDF_Bevel_Join);
    fmt::format_to(cmd_appender, "{}{} j\n", ind, (int)join_style);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_J(A4PDF_Line_Cap cap_style) {
    CHECK_ENUM(cap_style, A4PDF_Projection_Square_Cap);
    fmt::format_to(cmd_appender, "{}{} J\n", ind, (int)cap_style);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_K(double c, double m, double y, double k) {
    CHECK_COLORCOMPONENT(c);
    CHECK_COLORCOMPONENT(m);
    CHECK_COLORCOMPONENT(y);
    CHECK_COLORCOMPONENT(k);
    fmt::format_to(cmd_appender, "{}{} {} {} {} K\n", ind, c, m, y, k);
    return ErrorCode::NoError;
}
ErrorCode PdfDrawContext::cmd_k(double c, double m, double y, double k) {
    CHECK_COLORCOMPONENT(c);
    CHECK_COLORCOMPONENT(m);
    CHECK_COLORCOMPONENT(y);
    CHECK_COLORCOMPONENT(k);
    fmt::format_to(cmd_appender, "{}{} {} {} {} k\n", ind, c, m, y, k);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_l(double x, double y) {
    fmt::format_to(cmd_appender, "{}{} {} l\n", ind, x, y);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_m(double x, double y) {
    fmt::format_to(cmd_appender, "{}{} {} m\n", ind, x, y);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_M(double miterlimit) {
    fmt::format_to(cmd_appender, "{}{} M\n", ind, miterlimit);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_n() {
    commands += ind;
    commands += "n\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_q() {
    commands += ind;
    commands += "q\n";
    indent(DrawStateType::SaveState);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_Q() {
    auto rc = dedent(DrawStateType::SaveState);
    if(!rc) {
        return rc.error();
    }
    commands += ind;
    commands += "Q\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_re(double x, double y, double w, double h) {
    fmt::format_to(cmd_appender, "{}{} {} {} {} re\n", ind, x, y, w, h);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_RG(double r, double g, double b) {
    CHECK_COLORCOMPONENT(r);
    CHECK_COLORCOMPONENT(g);
    CHECK_COLORCOMPONENT(b);
    fmt::format_to(cmd_appender, "{}{} {} {} RG\n", ind, r, g, b);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_rg(double r, double g, double b) {
    CHECK_COLORCOMPONENT(r);
    CHECK_COLORCOMPONENT(g);
    CHECK_COLORCOMPONENT(b);
    fmt::format_to(cmd_appender, "{}{} {} {} rg\n", ind, r, g, b);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_ri(A4PDF_Rendering_Intent ri) {
    CHECK_ENUM(ri, A4PDF_RI_PERCEPTUAL);
    fmt::format_to(cmd_appender, "{}/{} ri\n", ind, rendering_intent_names.at((int)ri));
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_s() {
    commands += ind;
    commands += "s\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_S() {
    commands += ind;
    commands += "S\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_SCN(double value) {
    fmt::format_to(cmd_appender, "{}{} SCN\n", ind, value);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_scn(double value) {
    fmt::format_to(cmd_appender, "{}{} scn\n", ind, value);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_sh(ShadingId shid) {
    CHECK_INDEXNESS(shid.id, doc->document_objects);
    used_shadings.insert(shid.id);
    fmt::format_to(cmd_appender, "{}/SH{} sh\n", ind, shid.id);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_Tr(A4PDF_Text_Rendering_Mode mode) {
    CHECK_ENUM(mode, A4PDF_Text_Clip);
    fmt::format_to(cmd_appender, "{}{} Tr\n", ind, (int)mode);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_v(double x2, double y2, double x3, double y3) {
    fmt::format_to(cmd_appender, "{}{} {} {} {} v\n", ind, x2, y2, x3, y3);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_w(double w) {
    if(w < 0) {
        return ErrorCode::NegativeLineWidth;
    }
    fmt::format_to(cmd_appender, "{}{} w\n", ind, w);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_W() {
    commands += ind;
    commands += "W\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_Wstar() {
    commands += ind;
    commands += "W*\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_y(double x1, double y1, double x3, double y3) {
    fmt::format_to(cmd_appender, "{}{} {} {} {} y\n", ind, x1, y1, x3, y3);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::set_stroke_color(const DeviceRGBColor &c) {
    switch(doc->opts.output_colorspace) {
    case A4PDF_DEVICE_RGB: {
        cmd_RG(c.r.v(), c.g.v(), c.b.v());
        break;
    }
    case A4PDF_DEVICE_GRAY: {
        DeviceGrayColor gray = cm->to_gray(c);
        cmd_G(gray.v.v());
        break;
    }
    case A4PDF_DEVICE_CMYK: {
        auto cmyk_var = cm->to_cmyk(c);
        if(cmyk_var) {
            auto &cmyk = cmyk_var.value();
            return cmd_K(cmyk.c.v(), cmyk.m.v(), cmyk.y.v(), cmyk.k.v());
        }
        return cmyk_var.error();
        break;
    }
    }
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::set_nonstroke_color(A4PDF_IccColorSpaceId icc_id,
                                              const double *values,
                                              int32_t num_values) {
    CHECK_INDEXNESS(icc_id.id, doc->icc_profiles);
    const auto &icc_info = doc->icc_profiles.at(icc_id.id);
    if(icc_info.num_channels != num_values) {
        return ErrorCode::IncorrectColorChannelCount;
    }
    used_colorspaces.insert(icc_info.object_num);
    fmt::format_to(cmd_appender, "{}/CSpace{} cs\n", ind, icc_info.object_num);
    for(int32_t i = 0; i < num_values; ++i) {
        fmt::format_to(cmd_appender, "{:} ", values[i]);
    }
    fmt::format_to(cmd_appender, "scn\n", icc_info.object_num);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::set_stroke_color(A4PDF_IccColorSpaceId icc_id,
                                           const double *values,
                                           int32_t num_values) {
    CHECK_INDEXNESS(icc_id.id, doc->icc_profiles);
    const auto &icc_info = doc->icc_profiles.at(icc_id.id);
    if(icc_info.num_channels != num_values) {
        return ErrorCode::IncorrectColorChannelCount;
    }
    used_colorspaces.insert(icc_info.object_num);
    fmt::format_to(cmd_appender, "{}/CSpace{} CS\n", ind, icc_info.object_num);
    for(int32_t i = 0; i < num_values; ++i) {
        fmt::format_to(cmd_appender, "{:} ", values[i]);
    }
    fmt::format_to(cmd_appender, "SCN\n", icc_info.object_num);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::set_nonstroke_color(const DeviceRGBColor &c) {
    switch(doc->opts.output_colorspace) {
    case A4PDF_DEVICE_RGB: {
        return cmd_rg(c.r.v(), c.g.v(), c.b.v());
    }
    case A4PDF_DEVICE_GRAY: {
        DeviceGrayColor gray = cm->to_gray(c);
        return cmd_g(gray.v.v());
    }
    case A4PDF_DEVICE_CMYK: {
        auto cmyk_var = cm->to_cmyk(c);
        if(cmyk_var) {
            auto &cmyk = cmyk_var.value();
            return cmd_k(cmyk.c.v(), cmyk.m.v(), cmyk.y.v(), cmyk.k.v());
        }
        return cmyk_var.error();
    }
    default:
        return ErrorCode::Unreachable;
    }
}

ErrorCode PdfDrawContext::set_nonstroke_color(const DeviceGrayColor &c) {
    // Assumes that switching to the gray colorspace is always ok.
    // If it is not, fix to do the same switch() as above.
    return cmd_g(c.v.v());
}

ErrorCode PdfDrawContext::set_nonstroke_color(PatternId id) {
    if(context_type != A4PDF_Page_Context) {
        return ErrorCode::PatternNotAccepted;
    }
    used_patterns.insert(id.id);
    auto rc = cmd_cs("/Pattern");
    if(rc != ErrorCode::NoError) {
        return rc;
    }
    fmt::format_to(cmd_appender, "{}/Pattern-{} scn\n", ind, id.id);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::set_separation_stroke_color(SeparationId id, LimitDouble value) {
    CHECK_INDEXNESS(id.id, doc->separation_objects);
    const auto idnum = doc->separation_object_number(id);
    used_colorspaces.insert(idnum);
    std::string csname = fmt::format("/CSpace{}", idnum);
    cmd_CS(csname);
    cmd_SCN(value.v());
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::set_separation_nonstroke_color(SeparationId id, LimitDouble value) {
    CHECK_INDEXNESS(id.id, doc->separation_objects);
    const auto idnum = doc->separation_object_number(id);
    used_colorspaces.insert(idnum);
    std::string csname = fmt::format("/CSpace{}", idnum);
    cmd_cs(csname);
    cmd_scn(value.v());
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::set_stroke_color(LabId lid, const LabColor &c) {
    CHECK_INDEXNESS(lid.id, doc->document_objects);
    used_colorspaces.insert(lid.id);
    std::string csname = fmt::format("/CSpace{}", lid.id);
    cmd_CS(csname);
    fmt::format_to(cmd_appender, "{}{:f} {:f} {:f} SCN\n", ind, c.l, c.a, c.b);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::set_nonstroke_color(LabId lid, const LabColor &c) {
    CHECK_INDEXNESS(lid.id, doc->document_objects);
    used_colorspaces.insert(lid.id);
    std::string csname = fmt::format("/CSpace{}", lid.id);
    cmd_cs(csname);
    fmt::format_to(cmd_appender, "{}{:f} {:f} {:f} scn\n", ind, c.l, c.a, c.b);
    return ErrorCode::NoError;
}

void PdfDrawContext::set_all_stroke_color() {
    uses_all_colorspace = true;
    cmd_CS("/All");
    cmd_SCN(1.0);
}

ErrorCode PdfDrawContext::draw_image(A4PDF_ImageId im_id) {
    CHECK_INDEXNESS(im_id.id, doc->image_info);
    auto obj_num = doc->image_object_number(im_id);
    used_images.insert(obj_num);
    fmt::format_to(cmd_appender, "{}/Image{} Do\n", ind, obj_num);
    return ErrorCode::NoError;
}

void PdfDrawContext::scale(double xscale, double yscale) { cmd_cm(xscale, 0, 0, yscale, 0, 0); }

void PdfDrawContext::translate(double xtran, double ytran) { cmd_cm(1.0, 0, 0, 1.0, xtran, ytran); }

void PdfDrawContext::rotate(double angle) {
    cmd_cm(cos(angle), sin(angle), -sin(angle), cos(angle), 0.0, 0.0);
}

struct IconvCloser {
    iconv_t t;
    ~IconvCloser() { iconv_close(t); }
};

ErrorCode PdfDrawContext::render_utf8_text(
    std::string_view text, A4PDF_FontId fid, double pointsize, double x, double y) {
    PdfText t;
    t.cmd_Tf(fid, pointsize);
    t.cmd_Td(x, y);
    t.render_text(text);
    return render_text(t);
}

rvoe<NoReturnValue> PdfDrawContext::serialize_charsequence(const std::vector<CharItem> &charseq,
                                                           std::string &serialisation,
                                                           A4PDF_FontId &current_font,
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
            fmt::format_to(app, "{} ", std::get<double>(e));
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

ErrorCode PdfDrawContext::utf8_to_kerned_chars(std::string_view utf8_text,
                                               std::vector<CharItem> &charseq,
                                               A4PDF_FontId fid) {
    CHECK_INDEXNESS(fid.id, doc->font_objects);
    if(utf8_text.empty()) {
        return ErrorCode::NoError;
    }
    errno = 0;
    auto to_codepoint = iconv_open("UCS-4LE", "UTF-8");
    if(errno != 0) {
        perror(nullptr);
        return ErrorCode::IconvError;
    }
    IconvCloser ic{to_codepoint};
    FT_Face face = doc->fonts.at(doc->font_objects.at(fid.id).font_index_tmp).fontdata.face.get();
    if(!face) {
        return ErrorCode::BuiltinFontNotSupported;
    }

    uint32_t previous_codepoint = -1;
    auto in_ptr = (char *)utf8_text.data();
    auto in_bytes = utf8_text.length();
    // Freetype does not support GPOS kerning because it is context-sensitive.
    // So this method might produce incorrect kerning. Users that need precision
    // need to use the glyph based rendering method.
    const bool has_kerning = FT_HAS_KERNING(face);
    while(in_ptr < utf8_text.data() + utf8_text.size()) {
        uint32_t codepoint{0};
        auto out_ptr = (char *)&codepoint;
        auto out_bytes = sizeof(codepoint);
        errno = 0;
        auto iconv_result = iconv(to_codepoint, &in_ptr, &in_bytes, &out_ptr, &out_bytes);
        if(iconv_result == (size_t)-1 && errno != E2BIG) {
            return ErrorCode::BadUtf8;
        }

        if(has_kerning && previous_codepoint != (uint32_t)-1) {
            FT_Vector kerning;
            const auto index_left = FT_Get_Char_Index(face, previous_codepoint);
            const auto index_right = FT_Get_Char_Index(face, codepoint);
            auto ec = FT_Get_Kerning(face, index_left, index_right, FT_KERNING_DEFAULT, &kerning);
            if(ec != 0) {
                return ErrorCode::FreeTypeError;
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
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::render_text(const PdfText &textobj) {
    std::string serialisation{ind + "BT\n"};
    indent(DrawStateType::Text);
    std::back_insert_iterator<std::string> app = std::back_inserter(serialisation);
    int32_t current_subset{-1};
    A4PDF_FontId current_font{-1};
    double current_pointsize{-1};
    for(const auto &e : textobj.get_events()) {
        if(std::holds_alternative<TStar_arg>(e)) {
            serialisation += ind;
            serialisation += "T*\n";
        } else if(std::holds_alternative<Tc_arg>(e)) {
            const auto &tc = std::get<Tc_arg>(e);
            fmt::format_to(app, "{}{} Tc\n", ind, tc.val);
        } else if(std::holds_alternative<Td_arg>(e)) {
            const auto &td = std::get<Td_arg>(e);
            fmt::format_to(app, "{}{} {} Td\n", ind, td.tx, td.ty);
        } else if(std::holds_alternative<TD_arg>(e)) {
            const auto &tD = std::get<TD_arg>(e);
            fmt::format_to(app, "{}{} {} TD\n", ind, tD.tx, tD.ty);
        } else if(std::holds_alternative<Tf_arg>(e)) {
            current_font = std::get<Tf_arg>(e).font;
            current_subset = -1;
            current_pointsize = std::get<Tf_arg>(e).pointsize;
        } else if(std::holds_alternative<Text_arg>(e)) {
            const auto &tj = std::get<Text_arg>(e);
            std::vector<CharItem> charseq;
            auto ec = utf8_to_kerned_chars(tj.utf8_text, charseq, current_font);
            if(ec != ErrorCode::NoError) {
                return ec;
            }
            auto rv = serialize_charsequence(
                charseq, serialisation, current_font, current_subset, current_pointsize);
            if(!rv) {
                return rv.error();
            }
        } else if(std::holds_alternative<TJ_arg>(e)) {
            const auto &tJ = std::get<TJ_arg>(e);
            auto rc = serialize_charsequence(
                tJ.elements, serialisation, current_font, current_subset, current_pointsize);
            if(!rc) {
                return rc.error();
            }
        } else if(std::holds_alternative<TL_arg>(e)) {
            const auto &tL = std::get<TL_arg>(e);
            fmt::format_to(app, "{}{} TL\n", ind, tL.leading);
        } else if(std::holds_alternative<Tm_arg>(e)) {
            const auto &tm = std::get<Tm_arg>(e);
            fmt::format_to(
                app, "{}{} {} {} {} {} {} Tm\n", ind, tm.a, tm.b, tm.c, tm.d, tm.e, tm.f);
        } else if(std::holds_alternative<Ts_arg>(e)) {
            const auto &ts = std::get<Ts_arg>(e);
            fmt::format_to(app, "{}{} Ts\n", ind, ts.rise);
        } else if(std::holds_alternative<Tw_arg>(e)) {
            const auto &tw = std::get<Tw_arg>(e);
            fmt::format_to(app, "{}{} Tw\n", ind, tw.width);
        } else if(std::holds_alternative<Tz_arg>(e)) {
            const auto &tz = std::get<Tz_arg>(e);
            fmt::format_to(app, "{}{} Tz\n", ind, tz.scaling);
        } else if(std::holds_alternative<A4PDF_StructureItemId>(e)) {
            const auto &sid = std::get<A4PDF_StructureItemId>(e);
            used_structures.insert(sid);
            fmt::format_to(app, "{}/P << /MCID {} >>\n{}BDC\n", ind, sid.id, ind);
            indent(DrawStateType::MarkedContent);
        } else if(std::holds_alternative<Emc_arg>(e)) {
            auto rc = dedent(DrawStateType::MarkedContent);
            if(!rc) {
                return rc.error();
            }
            fmt::format_to(app, "{}EMC\n", ind);
        } else {
            fprintf(stderr, "Not implemented yet.\n");
            std::abort();
        }
    }
    auto rc = dedent(DrawStateType::Text);
    if(!rc) {
        return rc.error();
    }
    serialisation += ind;
    serialisation += "ET\n";
    commands += serialisation;
    return ErrorCode::NoError;
}

void PdfDrawContext::render_raw_glyph(
    uint32_t glyph, A4PDF_FontId fid, double pointsize, double x, double y) {
    auto &font_data = doc->font_objects.at(fid.id);
    // used_fonts.insert(font_data.font_obj);

    const auto font_glyph_id = doc->glyph_for_codepoint(
        doc->fonts.at(font_data.font_index_tmp).fontdata.face.get(), glyph);
    fmt::format_to(cmd_appender,
                   R"({}BT
{}  /Font{} {} Tf
{}  {} {} Td
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

ErrorCode PdfDrawContext::render_glyphs(const std::vector<PdfGlyph> &glyphs,
                                        A4PDF_FontId fid,
                                        double pointsize) {
    CHECK_INDEXNESS(fid.id, doc->font_objects);
    double prev_x = 0;
    double prev_y = 0;
    if(glyphs.empty()) {
        return ErrorCode::NoError;
    }
    auto &font_data = doc->font_objects.at(fid.id);
    // FIXME, do per character.
    // const auto &bob =
    //    doc->font_objects.at(doc->get_subset_glyph(fid, glyphs.front().codepoint).ss.fid.id);
    fmt::format_to(cmd_appender,
                   R"({}BT
{}  /SFont{}-{} {} Tf
)",
                   ind,
                   ind,
                   font_data.font_obj,
                   0,
                   pointsize);
    for(const auto &g : glyphs) {
        auto rv = doc->get_subset_glyph(fid, g.codepoint);
        if(!rv) {
            return rv.error();
        }
        auto &current_subset_glyph = rv.value();
        // const auto &bob = doc->font_objects.at(current_subset_glyph.ss.fid.id);
        used_subset_fonts.insert(current_subset_glyph.ss);
        fmt::format_to(cmd_appender, "  {} {} Td\n", g.x - prev_x, g.y - prev_y);
        prev_x = g.x;
        prev_y = g.y;
        fmt::format_to(
            cmd_appender, "  <{:02x}> Tj\n", (unsigned char)current_subset_glyph.glyph_id);
    }
    fmt::format_to(cmd_appender, "{}ET\n", ind);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::render_pdfdoc_text_builtin(const char *pdfdoc_encoded_text,
                                                     A4PDF_Builtin_Fonts font_id,
                                                     double pointsize,
                                                     double x,
                                                     double y) {
    if(doc->opts.subtype) {
        return ErrorCode::BadOperationForIntent;
    }
    auto font_object = doc->font_object_number(doc->get_builtin_font_id(font_id));
    used_fonts.insert(font_object);
    fmt::format_to(cmd_appender,
                   R"({}BT
{}  /Font{} {} Tf
{}  {} {} Td
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
    return ErrorCode::NoError;
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

} // namespace A4PDF
