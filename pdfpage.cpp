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
#include <fmt/core.h>

PdfPage::PdfPage(PdfGen *g) : g(g) {}

PdfPage::~PdfPage() {
    std::string buf;
    build_resource_dict();
    fmt::format_to(std::back_inserter(buf),
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
    resources = R"(<<
  /XObject <<
)";
    for(const auto &i : used_images) {
        fmt::format_to(std::back_inserter(resources), "    /Image{} {} 0 R\n", i, i);
    }

    resources += R"(  >>
>>
)";
}

void PdfPage::save() { commands += "q\n"; }

void PdfPage::restore() { commands += "Q\n"; }

void PdfPage::rectangle(double x, double y, double w, double h) {
    fmt::format_to(std::back_inserter(commands), "{} {} {} {} re\n", x, y, w, h);
}

void PdfPage::fill() { commands += "f\n"; }

void PdfPage::stroke() { commands += "S\n"; }

void PdfPage::set_line_width(double w) {
    fmt::format_to(std::back_inserter(commands), "{} w\n", w);
}

void PdfPage::set_stroke_color_rgb(double r, double g, double b) {
    fmt::format_to(std::back_inserter(commands), "{} {} {} RG\n", r, g, b);
}

void PdfPage::set_nonstroke_color_rgb(double r, double g, double b) {
    fmt::format_to(std::back_inserter(commands), "{} {} {} rg\n", r, g, b);
}

void PdfPage::draw_image(int32_t obj_num) {
    used_images.insert(obj_num);
    fmt::format_to(std::back_inserter(commands), "/Image{} Do\n", obj_num);
}

void PdfPage::set_matrix(double m1, double m2, double m3, double m4, double m5, double m6) {
    fmt::format_to(std::back_inserter(commands), "{} {} {} {} {} {} cm\n", m1, m2, m3, m4, m5, m6);
}
