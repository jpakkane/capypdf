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

#include <pdfpage.hpp>
#include <pdfgen.hpp>
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

PdfPage::PdfPage(PdfGen *g, PdfColorConverter *cm) : g(g), cm(cm), cmd_appender(commands) {}

PdfPage::~PdfPage() {
    try {
        if(!is_finalized) {
            finalize();
        }
    } catch(const std::exception &e) {
        printf("FAIL: %s\n", e.what());
    }
}

void PdfPage::finalize() {
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
    g->add_page(resources, buf);
}

void PdfPage::build_resource_dict() {
    auto resource_appender = std::back_inserter(resources);
    resources = "<<\n";
    resources += "  /ColorSpace ";
    switch(g->opts.output_colorspace) {
    case PDF_DEVICE_RGB:
        resources += "/DeviceRGB\n";
        break;
    case PDF_DEVICE_GRAY:
        resources += "/DeviceGray\n";
        break;
    case PDF_DEVICE_CMYK:
        resources += "/DeviceCMYK\n";
        break;
    }
    if(!used_images.empty()) {
        resources += "  /XObject <<\n";
        for(const auto &i : used_images) {
            fmt::format_to(resource_appender, "    /Image{} {} 0 R\n", i, i);
        }

        resources += "  >>\n";
    }
    if(!used_fonts.empty()) {
        resources += "  /Font <<\n";
        for(const auto &i : used_fonts) {
            fmt::format_to(resource_appender, "    /Font{} {} 0 R\n", i, i);
        }
        resources += "  >>\n";
    }
    if(!used_colorspaces.empty() || uses_all_colorspace) {
        resources += "  /ColorSpace <<\n";
        if(uses_all_colorspace) {
            fmt::format_to(resource_appender, "    /All {} 0 R\n", g->separation_objects.at(0));
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

void PdfPage::cmd_q() { commands += "q\n"; }

void PdfPage::cmd_Q() { commands += "Q\n"; }

void PdfPage::cmd_re(double x, double y, double w, double h) {
    fmt::format_to(cmd_appender, "{} {} {} {} re\n", x, y, w, h);
}

void PdfPage::cmd_f() { commands += "f\n"; }

void PdfPage::cmd_S() { commands += "S\n"; }

void PdfPage::cmd_h() { commands += "h\n"; }

void PdfPage::cmd_m(double x, double y) { fmt::format_to(cmd_appender, "{} {} m\n", x, y); }

void PdfPage::cmd_l(double x, double y) { fmt::format_to(cmd_appender, "{} {} l\n", x, y); }

void PdfPage::cmd_w(double w) { fmt::format_to(cmd_appender, "{} w\n", w); }

void PdfPage::cmd_c(double x1, double y1, double x2, double y2, double x3, double y3) {
    fmt::format_to(cmd_appender, "{} {} {} {} {} {} c\n", x1, y1, x2, y2, x3, y3);
}

void PdfPage::cmd_cs(std::string_view cspace_name) {
    fmt::format_to(cmd_appender, "{} cs\n", cspace_name);
}

void PdfPage::cmd_scn(double value) { fmt::format_to(cmd_appender, "{} scn\n", value); }

void PdfPage::cmd_CS(std::string_view cspace_name) {
    fmt::format_to(cmd_appender, "{} CS\n", cspace_name);
}

void PdfPage::cmd_SCN(double value) { fmt::format_to(cmd_appender, "{} SCN\n", value); }

void PdfPage::cmd_RG(double r, double g, double b) {
    fmt::format_to(cmd_appender, "{} {} {} RG\n", r, g, b);
}
void PdfPage::cmd_G(double gray) { fmt::format_to(cmd_appender, "{} G\n", gray); }

void PdfPage::cmd_K(double c, double m, double y, double k) {
    fmt::format_to(cmd_appender, "{} {} {} {} K\n", c, m, y, k);
}

void PdfPage::cmd_rg(double r, double g, double b) {
    fmt::format_to(cmd_appender, "{} {} {} rg\n", r, g, b);
}
void PdfPage::cmd_g(double gray) { fmt::format_to(cmd_appender, "{} g\n", gray); }

void PdfPage::cmd_k(double c, double m, double y, double k) {
    fmt::format_to(cmd_appender, "{} {} {} {} k\n", c, m, y, k);
}

void PdfPage::cmd_gs(std::string_view gs_name) {
    fmt::format_to(cmd_appender, "/{} gs\n", gs_name);
}

void PdfPage::set_stroke_color(const DeviceRGBColor &c) {
    switch(g->opts.output_colorspace) {
    case PDF_DEVICE_RGB: {
        cmd_RG(c.r.v(), c.g.v(), c.b.v());
        break;
    }
    case PDF_DEVICE_GRAY: {
        DeviceGrayColor gray = cm->to_gray(c);
        cmd_G(gray.v.v());
        break;
    }
    case PDF_DEVICE_CMYK: {
        DeviceCMYKColor cmyk = cm->to_cmyk(c);
        cmd_K(cmyk.c.v(), cmyk.m.v(), cmyk.y.v(), cmyk.k.v());
        break;
    }
    }
}

void PdfPage::set_nonstroke_color(const DeviceRGBColor &c) {
    switch(g->opts.output_colorspace) {
    case PDF_DEVICE_RGB: {
        cmd_rg(c.r.v(), c.g.v(), c.b.v());
        break;
    }
    case PDF_DEVICE_GRAY: {
        DeviceGrayColor gray = cm->to_gray(c);
        cmd_g(gray.v.v());
        break;
    }
    case PDF_DEVICE_CMYK: {
        DeviceCMYKColor cmyk = cm->to_cmyk(c);
        cmd_k(cmyk.c.v(), cmyk.m.v(), cmyk.y.v(), cmyk.k.v());
        break;
    }
    default:
        throw std::runtime_error("Unreachable!");
    }
}

void PdfPage::set_nonstroke_color(const DeviceGrayColor &c) {
    // Assumes that switching to the gray colorspace is always ok.
    // If it is not, fix to do the same switch() as above.
    cmd_g(c.v.v());
}

void PdfPage::set_separation_stroke_color(SeparationId id, LimitDouble value) {
    const auto idnum = g->separation_object_number(id);
    used_colorspaces.insert(idnum);
    std::string csname = fmt::format("/CSpace{}", idnum);
    cmd_CS(csname);
    cmd_SCN(value.v());
}

void PdfPage::set_separation_nonstroke_color(SeparationId id, LimitDouble value) {
    const auto idnum = g->separation_object_number(id);
    used_colorspaces.insert(idnum);
    std::string csname = fmt::format("/CSpace{}", idnum);
    cmd_cs(csname);
    cmd_scn(value.v());
}

void PdfPage::set_all_stroke_color() {
    uses_all_colorspace = true;
    cmd_CS("/All");
    cmd_SCN(1.0);
}

void PdfPage::draw_image(ImageId im_id) {
    auto obj_num = g->image_object_number(im_id);
    used_images.insert(obj_num);
    fmt::format_to(cmd_appender, "/Image{} Do\n", obj_num);
}

void PdfPage::cmd_cm(double m1, double m2, double m3, double m4, double m5, double m6) {
    fmt::format_to(
        cmd_appender, "{:.4f} {:.4f} {:.4f} {:.4f} {:.4f} {:.4f} cm\n", m1, m2, m3, m4, m5, m6);
}

void PdfPage::scale(double xscale, double yscale) { cmd_cm(xscale, 0, 0, yscale, 0, 0); }

void PdfPage::translate(double xtran, double ytran) { cmd_cm(1.0, 0, 0, 1.0, xtran, ytran); }

void PdfPage::rotate(double angle) {
    cmd_cm(cos(angle), sin(angle), -sin(angle), cos(angle), 0.0, 0.0);
}

struct IconvCloser {
    iconv_t t;
    ~IconvCloser() { iconv_close(t); }
};

void PdfPage::render_utf8_text(
    std::string_view text, FontId fid, double pointsize, double x, double y) {
    // PDF text code requires us to keep track of the highest
    // codepoint used thus far in order to generate the widths array.
    auto to_codepoint = iconv_open("UCS-4LE", "UTF-8");
    if(errno != 0) {
        throw std::runtime_error(strerror(errno));
    }
    IconvCloser ic{to_codepoint};
    auto to_utf16 = iconv_open("UTF-16BE", "UCS-4LE");
    if(errno != 0) {
        throw std::runtime_error(strerror(errno));
    }
    IconvCloser ic2{to_utf16};
    FT_Face face = g->font_objects.at(fid.id).font.get();
    if(!face) {
        throw std::runtime_error(
            "Tried to use builtin font to render UTF-8. They only support ASCII.");
    }
    auto &font_data = g->font_objects.at(fid.id);
    used_fonts.insert(font_data.font_obj);
    fmt::format_to(cmd_appender,
                   R"(BT
  /Font{} {} Tf
  {} {} Td
  <)",
                   font_data.font_obj,
                   pointsize,
                   x,
                   y);
    uint32_t codepoint = 0;
    auto in_ptr = (char *)text.data();
    auto in_bytes = text.length();
    while(in_ptr < text.data() + text.size()) {
        auto out_ptr = (char *)&codepoint;
        auto out_bytes = sizeof(codepoint);
        errno = 0;
        auto iconv_result = iconv(to_codepoint, &in_ptr, &in_bytes, &out_ptr, &out_bytes);
        if(iconv_result == (size_t)-1 && errno != E2BIG) {
            throw std::runtime_error(strerror(errno));
        }
        errno = 0;
        // Font subsetting would happen at this point.

        // auto glyph_index = g->glyph_for_codepoint(face, codepoint);
        auto glyph_index = codepoint;
        char *ucs4_in = (char *)&glyph_index;
        char u16buf[4];
        char *u16_out = u16buf;
        size_t in_ucs4_size = 4;
        size_t out_u16_size = sizeof(u16buf);
        iconv_result = iconv(to_utf16, &ucs4_in, &in_ucs4_size, &u16_out, &out_u16_size);
        if(errno) {
            throw std::runtime_error(strerror(errno));
        }
        for(const char *iterator = u16buf; iterator != u16_out; ++iterator) {
            fmt::format_to(cmd_appender, "{:02X}", (unsigned char)(*iterator));
        }
    }
    fmt::format_to(cmd_appender, "> Tj\nET\n");
    assert(in_bytes == 0);
}

void PdfPage::render_ascii_text_builtin(
    const char *ascii_text, BuiltinFonts font_id, double pointsize, double x, double y) {
    auto font_object = g->font_object_number(g->get_builtin_font_id(font_id));
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

void PdfPage::draw_unit_circle() {
    const double control = 0.5523 / 2;
    cmd_m(0, 0.5);
    cmd_c(control, 0.5, 0.5, control, 0.5, 0);
    cmd_c(0.5, -control, control, -0.5, 0, -0.5);
    cmd_c(-control, -0.5, -0.5, -control, -0.5, 0);
    cmd_c(-0.5, control, -control, 0.5, 0, 0.5);
}

void PdfPage::draw_unit_box() { cmd_re(-0.5, -0.5, 1, 1); }

void PdfPage::add_graphics_state(std::string_view name, const GraphicsState &gs) {
    gstates.emplace_back(GsEntries{std::string{name}, gs});
}
