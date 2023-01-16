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
    auto ctx = gen.new_page();
    auto fid = gen.load_font(fontfile);
    for(int i = 0; i < 16; ++i) {
        for(int j = 0; j < 16; ++j) {
            char buf[10];
            const int cur_char = 16 * i + j;
            snprintf(buf, 10, "0x%X", cur_char);
            ctx.render_utf8_text(buf, fid, 8, 10 + 45 * i, opts.page_size.h - (10 + 10 * j));
            buf[0] = (char)cur_char;
            buf[1] = '\0';
            ctx.render_raw_glyph(
                (uint16_t)cur_char, fid, 8, 10 + 30 + 45 * i, opts.page_size.h - (10 + 10 * j));
        }
    }
    return 0;
}
