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

#include <pdfpagebuilder.hpp>
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

namespace {

const std::array<const char *, 16> blend_mode_names{
    "Normal",
    "Multiply",
    "Screen",
    "Overlay",
    "Darken",
    "Lighten",
    "ColorDodge",
    "ColorBurn",
    "HardLight",
    "SoftLight",
    "Difference",
    "Exclusion",
    "Hue",
    "Saturation",
    "Color",
    "Luminosity",
};

const std::array<const char *, 4> intent_names{
    "RelativeColorimetric",
    "AbsoluteColorimetric",
    "Saturation",
    "Perceptual",
};

} // namespace

namespace A4PDF {

GstatePopper::~GstatePopper() { ctx->cmd_Q(); }

PdfPageBuilder::PdfPageBuilder(PdfDocument *doc, PdfColorConverter *cm)
    : doc(doc), cm(cm), cmd_appender(commands) {
    setup_initial_cs();
}

void PdfPageBuilder::setup_initial_cs() {
    if(doc->opts.output_colorspace == A4PDF_DEVICE_GRAY) {
        commands += "/DeviceGray CS\n/DeviceGray cs\n";
    } else if(doc->opts.output_colorspace == A4PDF_DEVICE_CMYK) {
        commands += "/DeviceCMYK CS\n/DeviceCMYK cs\n";
    }
    // The default is DeviceRGB.
    // FIXME add ICC here if needed.
}

PdfPageBuilder::~PdfPageBuilder() {}

void PdfPageBuilder::finalize() {
    if(is_finalized) {
        throw std::runtime_error("Tried to finalize a page object twice.");
    }
    is_finalized = true;
    std::string buf;
    build_resource_dict();
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

void PdfPageBuilder::clear() {
    resources.clear();
    commands.clear();
    used_images.clear();
    used_subset_fonts.clear();
    used_fonts.clear();
    used_colorspaces.clear();
    gstates.clear();
    is_finalized = false;
    uses_all_colorspace = false;

    setup_initial_cs();
}

void PdfPageBuilder::build_resource_dict() {
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
    if(!gstates.empty()) {
        resources += "  /ExtGState <<\n";
        for(const auto &s : gstates) {
            fmt::format_to(resource_appender, "    /{} <<\n", s.name);
            if(s.state.blend_mode) {
                fmt::format_to(
                    resource_appender, "      /BM /{}\n", blend_mode_names.at(*s.state.blend_mode));
            }
            if(s.state.intent) {
                fmt::format_to(resource_appender,
                               "      /RenderingIntent /{}\n",
                               intent_names.at(*s.state.intent));
            }
            fmt::format_to(resource_appender, "    >>\n");
        }
        resources += "  >>\n";
    }
    resources += ">>\n";
}

GstatePopper PdfPageBuilder::push_gstate() {
    cmd_q();
    return GstatePopper(this);
}

void PdfPageBuilder::cmd_q() { commands += "q\n"; }

void PdfPageBuilder::cmd_Q() { commands += "Q\n"; }

void PdfPageBuilder::cmd_re(double x, double y, double w, double h) {
    fmt::format_to(cmd_appender, "{} {} {} {} re\n", x, y, w, h);
}

void PdfPageBuilder::cmd_f() { commands += "f\n"; }

void PdfPageBuilder::cmd_S() { commands += "S\n"; }

void PdfPageBuilder::cmd_s() { commands += "s\n"; }

void PdfPageBuilder::cmd_h() { commands += "h\n"; }

void PdfPageBuilder::cmd_B() { commands += "B\n"; }

void PdfPageBuilder::cmd_Bstar() { commands += "B*\n"; }

void PdfPageBuilder::cmd_n() { commands += "n\n"; }

void PdfPageBuilder::cmd_W() { commands += "W\n"; }

void PdfPageBuilder::cmd_Wstar() { commands += "W*\n"; }

void PdfPageBuilder::cmd_m(double x, double y) { fmt::format_to(cmd_appender, "{} {} m\n", x, y); }

void PdfPageBuilder::cmd_l(double x, double y) { fmt::format_to(cmd_appender, "{} {} l\n", x, y); }

void PdfPageBuilder::cmd_w(double w) { fmt::format_to(cmd_appender, "{} w\n", w); }

void PdfPageBuilder::cmd_c(double x1, double y1, double x2, double y2, double x3, double y3) {
    fmt::format_to(cmd_appender, "{} {} {} {} {} {} c\n", x1, y1, x2, y2, x3, y3);
}

void PdfPageBuilder::cmd_cs(std::string_view cspace_name) {
    fmt::format_to(cmd_appender, "{} cs\n", cspace_name);
}

void PdfPageBuilder::cmd_scn(double value) { fmt::format_to(cmd_appender, "{} scn\n", value); }

void PdfPageBuilder::cmd_CS(std::string_view cspace_name) {
    fmt::format_to(cmd_appender, "{} CS\n", cspace_name);
}

void PdfPageBuilder::cmd_SCN(double value) { fmt::format_to(cmd_appender, "{} SCN\n", value); }

void PdfPageBuilder::cmd_RG(double r, double g, double b) {
    fmt::format_to(cmd_appender, "{} {} {} RG\n", r, g, b);
}
void PdfPageBuilder::cmd_G(double gray) { fmt::format_to(cmd_appender, "{} G\n", gray); }

void PdfPageBuilder::cmd_K(double c, double m, double y, double k) {
    fmt::format_to(cmd_appender, "{} {} {} {} K\n", c, m, y, k);
}

void PdfPageBuilder::cmd_rg(double r, double g, double b) {
    fmt::format_to(cmd_appender, "{} {} {} rg\n", r, g, b);
}
void PdfPageBuilder::cmd_g(double gray) { fmt::format_to(cmd_appender, "{} g\n", gray); }

void PdfPageBuilder::cmd_k(double c, double m, double y, double k) {
    fmt::format_to(cmd_appender, "{} {} {} {} k\n", c, m, y, k);
}

void PdfPageBuilder::cmd_gs(std::string_view gs_name) {
    fmt::format_to(cmd_appender, "/{} gs\n", gs_name);
}

void PdfPageBuilder::set_stroke_color(const DeviceRGBColor &c) {
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

void PdfPageBuilder::set_nonstroke_color(const DeviceRGBColor &c) {
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

void PdfPageBuilder::set_nonstroke_color(const DeviceGrayColor &c) {
    // Assumes that switching to the gray colorspace is always ok.
    // If it is not, fix to do the same switch() as above.
    cmd_g(c.v.v());
}

void PdfPageBuilder::set_separation_stroke_color(SeparationId id, LimitDouble value) {
    const auto idnum = doc->separation_object_number(id);
    used_colorspaces.insert(idnum);
    std::string csname = fmt::format("/CSpace{}", idnum);
    cmd_CS(csname);
    cmd_SCN(value.v());
}

void PdfPageBuilder::set_separation_nonstroke_color(SeparationId id, LimitDouble value) {
    const auto idnum = doc->separation_object_number(id);
    used_colorspaces.insert(idnum);
    std::string csname = fmt::format("/CSpace{}", idnum);
    cmd_cs(csname);
    cmd_scn(value.v());
}

void PdfPageBuilder::set_all_stroke_color() {
    uses_all_colorspace = true;
    cmd_CS("/All");
    cmd_SCN(1.0);
}

void PdfPageBuilder::draw_image(ImageId im_id) {
    auto obj_num = doc->image_object_number(im_id);
    used_images.insert(obj_num);
    fmt::format_to(cmd_appender, "/Image{} Do\n", obj_num);
}

void PdfPageBuilder::cmd_cm(double m1, double m2, double m3, double m4, double m5, double m6) {
    fmt::format_to(
        cmd_appender, "{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} cm\n", m1, m2, m3, m4, m5, m6);
}

void PdfPageBuilder::scale(double xscale, double yscale) { cmd_cm(xscale, 0, 0, yscale, 0, 0); }

void PdfPageBuilder::translate(double xtran, double ytran) { cmd_cm(1.0, 0, 0, 1.0, xtran, ytran); }

void PdfPageBuilder::rotate(double angle) {
    cmd_cm(cos(angle), sin(angle), -sin(angle), cos(angle), 0.0, 0.0);
}

struct IconvCloser {
    iconv_t t;
    ~IconvCloser() { iconv_close(t); }
};

void PdfPageBuilder::render_utf8_text(
    std::string_view text, FontId fid, double pointsize, double x, double y) {
    if(text.empty()) {
        return;
    }
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

void PdfPageBuilder::render_raw_glyph(
    uint32_t glyph, FontId fid, double pointsize, double x, double y) {
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

void PdfPageBuilder::render_ascii_text_builtin(
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

void PdfPageBuilder::draw_unit_circle() {
    const double control = 0.5523 / 2;
    cmd_m(0, 0.5);
    cmd_c(control, 0.5, 0.5, control, 0.5, 0);
    cmd_c(0.5, -control, control, -0.5, 0, -0.5);
    cmd_c(-control, -0.5, -0.5, -control, -0.5, 0);
    cmd_c(-0.5, control, -control, 0.5, 0, 0.5);
}

void PdfPageBuilder::draw_unit_box() { cmd_re(-0.5, -0.5, 1, 1); }

void PdfPageBuilder::add_graphics_state(std::string_view name, const GraphicsState &gs) {
    gstates.emplace_back(GsEntries{std::string{name}, gs});
}

} // namespace A4PDF
