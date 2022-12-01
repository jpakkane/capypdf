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

PdfPage::PdfPage(PdfGen *g) : g(g) {
    resources = R"(<<
  /ExtGState
  <<
    /a0
    <<
      /CA 1
      /ca 1
    >>
  >>
>>
)";
}

PdfPage::~PdfPage() {
    std::string buf;
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

void PdfPage::rectangle(double x, double y, double w, double h) {
    fmt::format_to(std::back_inserter(commands), "{} {} {} {} re\n", x, y, w, h);
}

void PdfPage::fill() { commands += "f\n"; }

void PdfPage::stroke() { commands += "S\n"; }
