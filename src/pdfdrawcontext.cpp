/*
 * Copyright 2022 Jussi Pakkanen
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

#define CHECK_COLORCOMPONENT(c)                                                                    \
    if(c < 0 || c > 1) {                                                                           \
        return ErrorCode::ColorOutOfRange;                                                         \
    }

#define CHECK_INDEXNESS(ind, container)                                                            \
    if((ind < 0) || ((size_t)ind >= container.size())) {                                           \
        return ErrorCode::BadId;                                                                   \
    }

#define CHECK_ENUM(v, max_enum_val)                                                                \
    if((int)v < 0 || ((int)v > (int)max_enum_val)) {                                               \
        return ErrorCode::BadEnum;                                                                 \
    }

namespace A4PDF {

GstatePopper::~GstatePopper() { ctx->cmd_Q(); }

PdfDrawContext::PdfDrawContext(PdfDocument *doc,
                               PdfColorConverter *cm,
                               A4PDF_Draw_Context_Type dtype)
    : doc(doc), cm(cm), context_type{dtype}, cmd_appender(commands) {}

PdfDrawContext::~PdfDrawContext() {}

SerializedContext PdfDrawContext::serialize() {
    SerializedContext sc;
    sc.dict = build_resource_dict();
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
    is_finalized = false;
    uses_all_colorspace = false;
}

std::string PdfDrawContext::build_resource_dict() {
    std::string resources;
    auto resource_appender = std::back_inserter(resources);
    resources = "<<\n";
    if(!used_images.empty()) {
        resources += "  /XObject <<\n";
        for(const auto &i : used_images) {
            fmt::format_to(resource_appender, "    /Image{} {} 0 R\n", i, i);
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

GstatePopper PdfDrawContext::push_gstate() {
    cmd_q();
    return GstatePopper(this);
}

ErrorCode PdfDrawContext::cmd_b() {
    commands += "b\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_B() {
    commands += "B\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_bstar() {
    commands += "b*\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_Bstar() {
    commands += "B*\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_c(double x1, double y1, double x2, double y2, double x3, double y3) {
    fmt::format_to(cmd_appender, "{} {} {} {} {} {} c\n", x1, y1, x2, y2, x3, y3);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_cm(double m1, double m2, double m3, double m4, double m5, double m6) {
    fmt::format_to(
        cmd_appender, "{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} cm\n", m1, m2, m3, m4, m5, m6);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_CS(std::string_view cspace_name) {
    fmt::format_to(cmd_appender, "{} CS\n", cspace_name);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_cs(std::string_view cspace_name) {
    fmt::format_to(cmd_appender, "{} cs\n", cspace_name);
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
    commands += "[ ";
    for(size_t i = 0; i < dash_array_length; ++i) {
        fmt::format_to(cmd_appender, "{} ", dash_array[i]);
    }
    fmt::format_to(cmd_appender, " ] {} d\n", phase);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_f() {
    commands += "f\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_fstar() {
    commands += "f*\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_G(double gray) {
    CHECK_COLORCOMPONENT(gray);
    fmt::format_to(cmd_appender, "{} G\n", gray);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_g(double gray) {
    CHECK_COLORCOMPONENT(gray);
    fmt::format_to(cmd_appender, "{} g\n", gray);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_gs(GstateId gid) {
    CHECK_INDEXNESS(gid.id, doc->document_objects);
    used_gstates.insert(gid.id);
    fmt::format_to(cmd_appender, "/GS{} gs\n", gid.id);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_h() {
    commands += "h\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_i(double flatness) {
    if(flatness < 0 || flatness > 100) {
        return ErrorCode::InvalidFlatness;
    }
    fmt::format_to(cmd_appender, "{} i\n", flatness);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_j(A4PDF_Line_Join join_style) {
    CHECK_ENUM(join_style, A4PDF_Bevel_Join);
    fmt::format_to(cmd_appender, "{} j\n", (int)join_style);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_J(A4PDF_Line_Cap cap_style) {
    CHECK_ENUM(cap_style, A4PDF_Projection_Square_Cap);
    fmt::format_to(cmd_appender, "{} J\n", (int)cap_style);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_K(double c, double m, double y, double k) {
    CHECK_COLORCOMPONENT(c);
    CHECK_COLORCOMPONENT(m);
    CHECK_COLORCOMPONENT(y);
    CHECK_COLORCOMPONENT(k);
    fmt::format_to(cmd_appender, "{} {} {} {} K\n", c, m, y, k);
    return ErrorCode::NoError;
}
ErrorCode PdfDrawContext::cmd_k(double c, double m, double y, double k) {
    CHECK_COLORCOMPONENT(c);
    CHECK_COLORCOMPONENT(m);
    CHECK_COLORCOMPONENT(y);
    CHECK_COLORCOMPONENT(k);
    fmt::format_to(cmd_appender, "{} {} {} {} k\n", c, m, y, k);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_l(double x, double y) {
    fmt::format_to(cmd_appender, "{} {} l\n", x, y);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_m(double x, double y) {
    fmt::format_to(cmd_appender, "{} {} m\n", x, y);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_M(double miterlimit) {
    fmt::format_to(cmd_appender, "{} M\n", miterlimit);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_n() {
    commands += "n\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_q() {
    commands += "q\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_Q() {
    commands += "Q\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_re(double x, double y, double w, double h) {
    fmt::format_to(cmd_appender, "{} {} {} {} re\n", x, y, w, h);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_RG(double r, double g, double b) {
    CHECK_COLORCOMPONENT(r);
    CHECK_COLORCOMPONENT(g);
    CHECK_COLORCOMPONENT(b);
    fmt::format_to(cmd_appender, "{} {} {} RG\n", r, g, b);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_rg(double r, double g, double b) {
    CHECK_COLORCOMPONENT(r);
    CHECK_COLORCOMPONENT(g);
    CHECK_COLORCOMPONENT(b);
    fmt::format_to(cmd_appender, "{} {} {} rg\n", r, g, b);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_ri(A4PDF_Rendering_Intent ri) {
    CHECK_ENUM(ri, A4PDF_RI_PERCEPTUAL);
    fmt::format_to(cmd_appender, "/{} ri\n", rendering_intent_names.at((int)ri));
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_s() {
    commands += "s\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_S() {
    commands += "S\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_SCN(double value) {
    fmt::format_to(cmd_appender, "{} SCN\n", value);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_scn(double value) {
    fmt::format_to(cmd_appender, "{} scn\n", value);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_sh(ShadingId shid) {
    CHECK_INDEXNESS(shid.id, doc->document_objects);
    used_shadings.insert(shid.id);
    fmt::format_to(cmd_appender, "/SH{} sh\n", shid.id);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_Tr(A4PDF_Text_Rendering_Mode mode) {
    CHECK_ENUM(mode, A4PDF_Text_Clip);
    fmt::format_to(cmd_appender, "{} Tr\n", (int)mode);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_v(double x2, double y2, double x3, double y3) {
    fmt::format_to(cmd_appender, "{} {} {} {} v\n", x2, y2, x3, y3);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_w(double w) {
    if(w < 0) {
        return ErrorCode::NegativeLineWidth;
    }
    fmt::format_to(cmd_appender, "{} w\n", w);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_W() {
    commands += "W\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_Wstar() {
    commands += "W*\n";
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_y(double x1, double y1, double x3, double y3) {
    fmt::format_to(cmd_appender, "{} {} {} {} y\n", x1, y1, x3, y3);
    return ErrorCode::NoError;
}

void PdfDrawContext::set_stroke_color(const DeviceRGBColor &c) {
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
        DeviceCMYKColor cmyk = cm->to_cmyk(c);
        cmd_K(cmyk.c.v(), cmyk.m.v(), cmyk.y.v(), cmyk.k.v());
        break;
    }
    }
}

ErrorCode
PdfDrawContext::set_nonstroke_color(IccColorId icc_id, const double *values, int32_t num_values) {
    CHECK_INDEXNESS(icc_id.id, doc->icc_profiles);
    const auto &icc_info = doc->icc_profiles.at(icc_id.id);
    if(icc_info.num_channels != num_values) {
        throw std::runtime_error("Incorrect number of color channels.");
    }
    used_colorspaces.insert(icc_info.object_num);
    fmt::format_to(cmd_appender, "/CSpace{} cs\n", icc_info.object_num);
    for(int32_t i = 0; i < num_values; ++i) {
        fmt::format_to(cmd_appender, "{:} ", values[i]);
    }
    fmt::format_to(cmd_appender, "scn\n", icc_info.object_num);
    return ErrorCode::NoError;
}

ErrorCode
PdfDrawContext::set_stroke_color(IccColorId icc_id, const double *values, int32_t num_values) {
    CHECK_INDEXNESS(icc_id.id, doc->icc_profiles);
    const auto &icc_info = doc->icc_profiles.at(icc_id.id);
    if(icc_info.num_channels != num_values) {
        throw std::runtime_error("Incorrect number of color channels.");
    }
    used_colorspaces.insert(icc_info.object_num);
    fmt::format_to(cmd_appender, "/CSpace{} CS\n", icc_info.object_num);
    for(int32_t i = 0; i < num_values; ++i) {
        fmt::format_to(cmd_appender, "{:} ", values[i]);
    }
    fmt::format_to(cmd_appender, "SCN\n", icc_info.object_num);
    return ErrorCode::NoError;
}

void PdfDrawContext::set_nonstroke_color(const DeviceRGBColor &c) {
    switch(doc->opts.output_colorspace) {
    case A4PDF_DEVICE_RGB: {
        cmd_rg(c.r.v(), c.g.v(), c.b.v());
        break;
    }
    case A4PDF_DEVICE_GRAY: {
        DeviceGrayColor gray = cm->to_gray(c);
        cmd_g(gray.v.v());
        break;
    }
    case A4PDF_DEVICE_CMYK: {
        DeviceCMYKColor cmyk = cm->to_cmyk(c);
        cmd_k(cmyk.c.v(), cmyk.m.v(), cmyk.y.v(), cmyk.k.v());
        break;
    }
    default:
        throw std::runtime_error("Unreachable!");
    }
}

void PdfDrawContext::set_nonstroke_color(const DeviceGrayColor &c) {
    // Assumes that switching to the gray colorspace is always ok.
    // If it is not, fix to do the same switch() as above.
    cmd_g(c.v.v());
}

void PdfDrawContext::set_nonstroke_color(PatternId id) {
    if(context_type != A4PDF_Page_Context) {
        throw std::runtime_error("Patterns can only be used in page contexts.");
    }
    used_patterns.insert(id.id);
    cmd_cs("/Pattern");
    fmt::format_to(cmd_appender, "/Pattern-{} scn\n", id.id);
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
    fmt::format_to(cmd_appender, "{:f} {:f} {:f} SCN\n", c.l, c.a, c.b);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::set_nonstroke_color(LabId lid, const LabColor &c) {
    CHECK_INDEXNESS(lid.id, doc->document_objects);
    used_colorspaces.insert(lid.id);
    std::string csname = fmt::format("/CSpace{}", lid.id);
    cmd_cs(csname);
    fmt::format_to(cmd_appender, "{:f} {:f} {:f} scn\n", c.l, c.a, c.b);
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
    fmt::format_to(cmd_appender, "/Image{} Do\n", obj_num);
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
    CHECK_INDEXNESS(fid.id, doc->font_objects);
    if(text.empty()) {
        return ErrorCode::NoError;
    }
    errno = 0;
    auto to_codepoint = iconv_open("UCS-4LE", "UTF-8");
    if(errno != 0) {
        throw std::runtime_error(strerror(errno));
    }
    IconvCloser ic{to_codepoint};
    FT_Face face = doc->fonts.at(doc->font_objects.at(fid.id).font_index_tmp).fontdata.face.get();
    if(!face) {
        throw std::runtime_error(
            "Tried to use builtin font to render UTF-8. They only support ASCII.");
    }

    uint32_t previous_codepoint = -1;
    auto in_ptr = (char *)text.data();
    auto in_bytes = text.length();
    FontSubset previous_subset;
    previous_subset.fid.id = -1;
    previous_subset.subset_id = -1;
    // Freetype does not support GPOS kerning because it is context-sensitive.
    // So this method might produce incorrect kerning. Users that need precision
    // need to use the glyph based rendering method.
    const bool has_kerning = FT_HAS_KERNING(face);
    while(in_ptr < text.data() + text.size()) {
        uint32_t codepoint{0};
        auto out_ptr = (char *)&codepoint;
        auto out_bytes = sizeof(codepoint);
        errno = 0;
        auto iconv_result = iconv(to_codepoint, &in_ptr, &in_bytes, &out_ptr, &out_bytes);
        if(iconv_result == (size_t)-1 && errno != E2BIG) {
            throw std::runtime_error(strerror(errno));
        }
        // FIXME: check if we need to change font subset,
        auto current_subset_glyph = doc->get_subset_glyph(fid, codepoint);
        const auto &bob = doc->font_objects.at(current_subset_glyph.ss.fid.id);
        used_subset_fonts.insert(current_subset_glyph.ss);
        if(previous_subset.subset_id == -1) {
            fmt::format_to(cmd_appender,
                           R"(BT
  /SFont{}-{} {} Tf
  {} {} Td
  [ <)",
                           bob.font_obj,
                           current_subset_glyph.ss.subset_id,
                           pointsize,
                           x,
                           y);
            previous_subset = current_subset_glyph.ss;
            // Add to used fonts.
        } else if(current_subset_glyph.ss != previous_subset) {
            fmt::format_to(cmd_appender,
                           R"() ] TJ
  /SFont{}-{} {} Tf
  [ <)",
                           bob.font_obj,
                           current_subset_glyph.ss.subset_id,
                           pointsize);
            previous_subset = current_subset_glyph.ss;
            // Add to used fonts.
        }

        if(has_kerning && previous_codepoint != (uint32_t)-1) {
            FT_Vector kerning;
            const auto index_left = FT_Get_Char_Index(face, previous_codepoint);
            const auto index_right = FT_Get_Char_Index(face, codepoint);
            auto ec = FT_Get_Kerning(face, index_left, index_right, FT_KERNING_DEFAULT, &kerning);
            if(ec != 0) {
                throw std::runtime_error("Getting kerning data failed.");
            }
            if(kerning.x != 0) {
                // The value might be a integer, fraction or something else.
                // None of the fonts I tested had kerning that Freetype recognized,
                // so don't know if this actually works.
                fmt::format_to(cmd_appender, ">{}<", int(kerning.x));
            }
        }
        fmt::format_to(cmd_appender, "{:02x}", (unsigned char)current_subset_glyph.glyph_id);
        // fmt::format_to(cmd_appender, "<{:02x}>", (unsigned char)current_subset_glyph.glyph_id);
        previous_codepoint = codepoint;
    }
    fmt::format_to(cmd_appender, "> ] TJ\nET\n");
    assert(in_bytes == 0);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::render_text(const PdfText &textobj) {
    std::string serialisation{"BT\n"};
    auto app = std::back_inserter(serialisation);
    int32_t current_subset{-1};
    A4PDF_FontId current_font{-1};
    double current_pointsize{-1};
    for(const auto &e : textobj.get_events()) {
        if(std::holds_alternative<Tf_arg>(e)) {
            current_font = std::get<Tf_arg>(e).font;
            current_subset = -1;
            current_pointsize = std::get<Tf_arg>(e).pointsize;
        } else if(std::holds_alternative<Td_arg>(e)) {
            const auto &td = std::get<Td_arg>(e);
            fmt::format_to(app, "  {} {} Td\n", td.tx, td.ty);
        } else if(std::holds_alternative<TJ_arg>(e)) {
            const auto &tj = std::get<TJ_arg>(e);
            bool is_first = true;
            for(const auto &e : tj.elements) {
                if(std::holds_alternative<double>(e)) {
                    if(is_first) {
                        serialisation += "  [ ";
                    }
                    fmt::format_to(app, "{} ", std::get<double>(e));
                } else {
                    assert(std::holds_alternative<uint32_t>(e));
                    const auto codepoint = std::get<uint32_t>(e);
                    auto current_subset_glyph = doc->get_subset_glyph(current_font, codepoint);
                    used_subset_fonts.insert(current_subset_glyph.ss);
                    if(current_subset_glyph.ss.subset_id != current_subset) {
                        if(!is_first) {
                            serialisation += "] TJ\n";
                        }
                        fmt::format_to(
                            app,
                            "  /SFont{}-{} {} Tf\n  [ ",
                            doc->font_objects.at(current_subset_glyph.ss.fid.id).font_obj,
                            current_subset_glyph.ss.subset_id,
                            current_pointsize);
                    }
                    current_font = current_subset_glyph.ss.fid;
                    current_subset = current_subset_glyph.ss.subset_id;
                    fmt::format_to(app, "<{:02x}> ", current_subset_glyph.glyph_id);
                }
                is_first = false;
            }
            serialisation += "] TJ\n";
        } else {
            fprintf(stderr, "Not implemented yet.\n");
            std::abort();
        }
    }
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
                   R"(BT
  /Font{} {} Tf
  {} {} Td
  (\{:o}) Tj
ET
)",
                   font_data.font_obj,
                   pointsize,
                   x,
                   y,
                   font_glyph_id);
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
                   R"(BT
  /SFont{}-{} {} Tf
)",
                   font_data.font_obj,
                   0,
                   pointsize);
    for(const auto &g : glyphs) {
        auto current_subset_glyph = doc->get_subset_glyph(fid, g.codepoint);
        // const auto &bob = doc->font_objects.at(current_subset_glyph.ss.fid.id);
        used_subset_fonts.insert(current_subset_glyph.ss);
        fmt::format_to(cmd_appender, "  {} {} Td\n", g.x - prev_x, g.y - prev_y);
        prev_x = g.x;
        prev_y = g.y;
        fmt::format_to(
            cmd_appender, "  <{:02x}> Tj\n", (unsigned char)current_subset_glyph.glyph_id);
    }
    fmt::format_to(cmd_appender, "ET\n");
    return ErrorCode::NoError;
}

void PdfDrawContext::render_ascii_text_builtin(
    const char *ascii_text, A4PDF_Builtin_Fonts font_id, double pointsize, double x, double y) {
    auto font_object = doc->font_object_number(doc->get_builtin_font_id(font_id));
    used_fonts.insert(font_object);
    std::string cleaned_text;
    for(const auto c : std::string_view(ascii_text)) {
        if((unsigned char)c > 127) {
            cleaned_text += ' ';
        } else if(c == '(') {
            cleaned_text += "\\(";
        } else if(c == '\\') {
            cleaned_text += "\\\\";
        } else if(c == ')') {
            cleaned_text += "\\)";
        } else {
            cleaned_text += c;
        }
    }
    fmt::format_to(cmd_appender,
                   R"(BT
  /Font{} {} Tf
  {} {} Td
  ({}) Tj
ET
)",
                   font_object,
                   pointsize,
                   x,
                   y,
                   cleaned_text);
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
