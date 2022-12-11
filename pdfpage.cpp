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
#include <lcms2.h>
#include <fmt/core.h>
#include <cmath>

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
    if(!used_images.empty()) {
        resources += "  /XObject <<\n";
        for(const auto &i : used_images) {
            fmt::format_to(resource_appender, "    /Image{} {} 0 R\n", i, i);
        }

        resources += "  >>";
    }
    if(!used_fonts.empty()) {
        resources += "  /Font <<\n";
        for(const auto &i : used_fonts) {
            fmt::format_to(resource_appender, "    /Font{} {} 0 R\n", i, i);
        }
        resources += "  >>\n";
    }
    if(!used_colorspaces.empty()) {
        resources += "  /ColorSpace <<\n";
        for(const auto &i : used_colorspaces) {
            fmt::format_to(resource_appender, "    /CSpace{} {} 0 R\n", i, i);
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
        fmt::format_to(cmd_appender, "{} {} {} rg\n", c.r.v(), c.g.v(), c.b.v());
        break;
    }
    case PDF_DEVICE_GRAY: {
        DeviceGrayColor gray = cm->to_gray(c);
        fmt::format_to(cmd_appender, "{} g\n", gray.v.v());
        break;
    }
    case PDF_DEVICE_CMYK: {
        DeviceCMYKColor cmyk = cm->to_cmyk(c);
        fmt::format_to(
            cmd_appender, "{} {} {} {} k\n", cmyk.c.v(), cmyk.m.v(), cmyk.y.v(), cmyk.k.v());
        break;
    }
    default:
        throw std::runtime_error("Unreachable!");
    }
}

void PdfPage::set_separation_stroke_color(SeparationId id, LimitDouble value) {
    const auto idnum = g->separation_object_number(id);
    used_colorspaces.insert(idnum);
    fmt::format_to(cmd_appender,
                   R"(/CSpace{} CS
{} SCN
)",
                   idnum,
                   value.v());
}

void PdfPage::set_separation_nonstroke_color(SeparationId id, LimitDouble value) {
    const auto idnum = g->separation_object_number(id);
    used_colorspaces.insert(idnum);
    fmt::format_to(cmd_appender,
                   R"(/CSpace{} cs
{} scn
)",
                   idnum,
                   value.v());
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

void PdfPage::simple_text(
    const char *u8text, FontId font_id, double pointsize, double x, double y) {
    auto font_object = g->font_object_number(font_id);
    used_fonts.insert(font_object);
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
                   u8text);
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
