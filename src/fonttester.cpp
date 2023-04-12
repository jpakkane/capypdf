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

#include <pdfgen.hpp>

#include <vector>
#include <string>
#include <variant>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cmath>

using namespace A4PDF;

int main(int argc, char **argv) {
    const char *fontfile;
    const char *text;
    if(argc > 1) {
        fontfile = argv[1];
    } else {
        fontfile = "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf";
    }
    if(argc > 2) {
        text = argv[2];
    } else {
        text = "MiW.";
    }

    PdfGenerationData opts;
    opts.mediabox = PdfBox{0, 0, 200, 30};
    GenPopper genpop("fonttester.pdf", opts);
    PdfGen &gen = *genpop.g;

    auto ctxguard = gen.guarded_page_context();
    auto &ctx = ctxguard.ctx;

    auto textfont = gen.load_font(fontfile).value();
    ctx.render_utf8_text(text, textfont, 12, 10, 10);
    return 0;
}
