// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <pdfgen.hpp>

#include <vector>
#include <string>
#include <variant>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cmath>

using namespace capypdf;

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
    opts.default_page_properties.mediabox = PdfRectangle{0, 0, 200, 30};
    GenPopper genpop("fonttester.pdf", opts);
    PdfGen &gen = *genpop.g;

    auto ctxguard = gen.guarded_page_context();
    auto &ctx = ctxguard.ctx;

    auto textfont = gen.load_font(fontfile).value();
    if(!ctx.render_text(u8string::from_cstr(text).value(), textfont, 12, 10, 10)) {
        printf("FAIL.\n");
    }
    return 0;
}
