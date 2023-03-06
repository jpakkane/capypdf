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

#include <pdfgen.hpp>
#include <cmath>

using namespace A4PDF;

int main(int argc, char **argv) {
    PdfGenerationData opts;
    opts.output_colorspace = A4PDF_DEVICE_GRAY;
    const char *fontfile;
    if(argc > 1) {
        fontfile = argv[1];
    } else {
        fontfile = "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf";
    }
    /*
    opts.mediabox.x = opts.mediabox.y = 0;
    opts.mediabox.w = 200;
    opts.mediabox.h = 200;
    */
    opts.title = "Over 255 letters";
    GenPopper genpop("fonttest.pdf", opts);
    PdfGen &gen = genpop.g;
    auto fid = gen.load_font(fontfile);
    auto ctxguard = gen.guarded_page_context();
    auto &ctx = ctxguard.ctx;
    ctx.render_utf8_text("ABCDEFGHIJKLMNOPQRSTUVWXYZ", fid, 12, 10, 800);
    ctx.render_utf8_text("abcdefghijklmnopqrstuvwxyz", fid, 12, 10, 780);
    ctx.render_utf8_text("0123456789!\"#¤%&/()=+?-.,;:'*~", fid, 12, 10, 760);
    ctx.render_utf8_text("бгджзиклмнптфцч", fid, 12, 10, 740);
    // ctx.render_utf8_text("ΓΔΖΗΛΞΠΣΥΦΧΨΩ", fid, 12, 10, 720);
    /*
    std::vector<PdfGlyph> glyphs;
    const int num_glyphs = 26;
    for(int i = 0; i < num_glyphs; ++i) {
        const double x = 100 + 40 * sin(2 * M_PI * double(i) / num_glyphs);
        const double y = 50 + 40 * cos(2 * M_PI * double(i) / num_glyphs);
        glyphs.emplace_back(PdfGlyph{uint32_t('a' + i), x, y});
    }
    ctx.render_glyphs(glyphs, fid, 10);
    */
    /*
    gen.new_page();
    for(int page_num = 0; page_num < 2; ++page_num) {
        for(int i = 0; i < 16; ++i) {
            for(int j = 0; j < 16; ++j) {
                char buf[10];
                const uint32_t cur_char = 256 * page_num + 16 * i + j;
                snprintf(buf, 10, "0x%04X", cur_char);
                ctx.render_utf8_text(buf, fid, 8, 10 + 45 * i, opts.page_size.h - (10 + 10 * j));
                ctx.render_raw_glyph(
                    (uint32_t)cur_char, fid, 8, 10 + 30 + 45 * i, opts.page_size.h - (10 + 10 * j));
            }
        }
    }
    */
    return 0;
}
