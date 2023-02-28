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

namespace A4PDF {

GstatePopper::~GstatePopper() { ctx->cmd_Q(); }

PdfDrawContext::PdfDrawContext(PdfDocument *doc,
                               PdfColorConverter *cm,
                               A4PDF_Draw_Context_Type dtype)
    : doc(doc), cm(cm), context_type{dtype}, cmd_appender(commands) {
    setup_initial_cs();
}

void PdfDrawContext::setup_initial_cs() {
    if(doc->opts.output_colorspace == A4PDF_DEVICE_GRAY) {
        commands += "/DeviceGray CS\n/DeviceGray cs\n";
    } else if(doc->opts.output_colorspace == A4PDF_DEVICE_CMYK) {
        commands += "/DeviceCMYK CS\n/DeviceCMYK cs\n";
    }
    // The default is DeviceRGB.
    // FIXME add ICC here if needed.
}

PdfDrawContext::~PdfDrawContext() {}

void PdfDrawContext::finalize() {
    if(is_finalized) {
        throw std::runtime_error("Tried to finalize a page object twice.");
    }
    is_finalized = true;
    std::string buf;
    std::string resources = build_resource_dict();
    buf = fmt::format(
        R"(<<
  /Length {}
>>
stream
{}
endstream
)",
        commands.size(),
        commands);
    doc->add_page(resources, buf);
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

    setup_initial_cs();
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

void PdfDrawContext::cmd_q() { commands += "q\n"; }

void PdfDrawContext::cmd_Q() { commands += "Q\n"; }

ErrorCode PdfDrawContext::cmd_re(double x, double y, double w, double h) {
    fmt::format_to(cmd_appender, "{} {} {} {} re\n", x, y, w, h);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_f() {
    commands += "f\n";
    return ErrorCode::NoError;
}

void PdfDrawContext::cmd_S() { commands += "S\n"; }

void PdfDrawContext::cmd_s() { commands += "s\n"; }

void PdfDrawContext::cmd_h() { commands += "h\n"; }

void PdfDrawContext::cmd_B() { commands += "B\n"; }

void PdfDrawContext::cmd_Bstar() { commands += "B*\n"; }

void PdfDrawContext::cmd_sh(ShadingId shid) {
    used_shadings.insert(shid.id);
    fmt::format_to(cmd_appender, "/SH{} sh\n", shid.id);
}

void PdfDrawContext::cmd_n() { commands += "n\n"; }

void PdfDrawContext::cmd_W() { commands += "W\n"; }

void PdfDrawContext::cmd_Wstar() { commands += "W*\n"; }

void PdfDrawContext::cmd_m(double x, double y) { fmt::format_to(cmd_appender, "{} {} m\n", x, y); }

void PdfDrawContext::cmd_l(double x, double y) { fmt::format_to(cmd_appender, "{} {} l\n", x, y); }

ErrorCode PdfDrawContext::cmd_w(double w) {
    if(w < 0) {
        return ErrorCode::NegativeLineWidth;
    }
    fmt::format_to(cmd_appender, "{} w\n", w);
    return ErrorCode::NoError;
}

void PdfDrawContext::cmd_c(double x1, double y1, double x2, double y2, double x3, double y3) {
    fmt::format_to(cmd_appender, "{} {} {} {} {} {} c\n", x1, y1, x2, y2, x3, y3);
}

void PdfDrawContext::cmd_cs(std::string_view cspace_name) {
    fmt::format_to(cmd_appender, "{} cs\n", cspace_name);
}

void PdfDrawContext::cmd_scn(double value) { fmt::format_to(cmd_appender, "{} scn\n", value); }

void PdfDrawContext::cmd_CS(std::string_view cspace_name) {
    fmt::format_to(cmd_appender, "{} CS\n", cspace_name);
}

void PdfDrawContext::cmd_SCN(double value) { fmt::format_to(cmd_appender, "{} SCN\n", value); }

ErrorCode PdfDrawContext::cmd_J(A4PDF_Line_Cap cap_style) {
    fmt::format_to(cmd_appender, "{} J\n", (int)cap_style);
    return ErrorCode::NoError;
}

ErrorCode PdfDrawContext::cmd_j(A4PDF_Line_Join join_style) {
    fmt::format_to(cmd_appender, "{} j\n", (int)join_style);
    return ErrorCode::NoError;
}

void PdfDrawContext::cmd_RG(double r, double g, double b) {
    fmt::format_to(cmd_appender, "{} {} {} RG\n", r, g, b);
}
void PdfDrawContext::cmd_G(double gray) { fmt::format_to(cmd_appender, "{} G\n", gray); }

void PdfDrawContext::cmd_K(double c, double m, double y, double k) {
    fmt::format_to(cmd_appender, "{} {} {} {} K\n", c, m, y, k);
}

void PdfDrawContext::cmd_rg(double r, double g, double b) {
    fmt::format_to(cmd_appender, "{} {} {} rg\n", r, g, b);
}
void PdfDrawContext::cmd_g(double gray) { fmt::format_to(cmd_appender, "{} g\n", gray); }

void PdfDrawContext::cmd_k(double c, double m, double y, double k) {
    fmt::format_to(cmd_appender, "{} {} {} {} k\n", c, m, y, k);
}

void PdfDrawContext::cmd_gs(GstateId gid) {
    used_gstates.insert(gid.id);
    fmt::format_to(cmd_appender, "/GS{} gs\n", gid.id);
}

void PdfDrawContext::cmd_Tr(A4PDF_Text_Rendering_Mode mode) {
    fmt::format_to(cmd_appender, "{} Tr\n", (int)mode);
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

void PdfDrawContext::set_nonstroke_color(IccColorId icc_id,
                                         const double *values,
                                         int32_t num_values) {
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
}

void PdfDrawContext::set_stroke_color(IccColorId icc_id, const double *values, int32_t num_values) {
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

void PdfDrawContext::set_separation_stroke_color(SeparationId id, LimitDouble value) {
    const auto idnum = doc->separation_object_number(id);
    used_colorspaces.insert(idnum);
    std::string csname = fmt::format("/CSpace{}", idnum);
    cmd_CS(csname);
    cmd_SCN(value.v());
}

void PdfDrawContext::set_separation_nonstroke_color(SeparationId id, LimitDouble value) {
    const auto idnum = doc->separation_object_number(id);
    used_colorspaces.insert(idnum);
    std::string csname = fmt::format("/CSpace{}", idnum);
    cmd_cs(csname);
    cmd_scn(value.v());
}

void PdfDrawContext::set_stroke_color(LabId lid, const LabColor &c) {
    used_colorspaces.insert(lid.id);
    std::string csname = fmt::format("/CSpace{}", lid.id);
    cmd_CS(csname);
    fmt::format_to(cmd_appender, "{:f} {:f} {:f} SCN\n", c.l, c.a, c.b);
}

void PdfDrawContext::set_nonstroke_color(LabId lid, const LabColor &c) {
    used_colorspaces.insert(lid.id);
    std::string csname = fmt::format("/CSpace{}", lid.id);
    cmd_cs(csname);
    fmt::format_to(cmd_appender, "{:f} {:f} {:f} scn\n", c.l, c.a, c.b);
}

void PdfDrawContext::set_all_stroke_color() {
    uses_all_colorspace = true;
    cmd_CS("/All");
    cmd_SCN(1.0);
}

void PdfDrawContext::draw_image(ImageId im_id) {
    auto obj_num = doc->image_object_number(im_id);
    used_images.insert(obj_num);
    fmt::format_to(cmd_appender, "/Image{} Do\n", obj_num);
}

void PdfDrawContext::cmd_cm(double m1, double m2, double m3, double m4, double m5, double m6) {
    fmt::format_to(
        cmd_appender, "{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} cm\n", m1, m2, m3, m4, m5, m6);
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

void PdfDrawContext::render_utf8_text(
    std::string_view text, A4PDF_FontId fid, double pointsize, double x, double y) {
    if(text.empty()) {
        return;
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
        // Need to change font subset?
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

void PdfDrawContext::render_glyphs(const std::vector<PdfGlyph> &glyphs,
                                   A4PDF_FontId fid,
                                   double pointsize) {
    double prev_x = 0;
    double prev_y = 0;
    if(glyphs.empty()) {
        return;
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
