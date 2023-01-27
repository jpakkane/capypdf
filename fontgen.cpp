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

int main(int argc, char **argv) {
    PdfGenerationData opts;
    opts.page_size.h = 200;
    opts.page_size.w = 800;
    opts.output_colorspace = PDF_DEVICE_GRAY;
    const char *fontfile;
    if(argc > 1) {
        fontfile = argv[1];
    } else {
        fontfile = "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf";
    }
    opts.mediabox.x = opts.mediabox.y = 0;
    opts.mediabox.w = opts.page_size.w;
    opts.mediabox.h = opts.page_size.h;
    opts.title = "Font layout test";
    PdfGen gen("fonttest.pdf", opts);
    auto fid = gen.load_font(fontfile);
    {
        auto ctx = gen.new_page();
        ctx.render_utf8_text("Av, Tv, kerning yo", fid, 12, 50, 50);
    }
    for(int page_num = 0; page_num < 2; ++page_num) {
        auto ctx = gen.new_page();
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
    return 0;
}
