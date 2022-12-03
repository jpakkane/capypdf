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
    opts.page_size = Area::a4();
    opts.mediabox.x = opts.mediabox.y = 0;
    opts.mediabox.w = opts.page_size.w;
    opts.mediabox.h = opts.page_size.h;

    opts.title = "PDF experiment";
    opts.author = "Peter David Foster, esq";
    opts.output_colorspace = PDF_DEVICE_GRAY;

    try {
        PdfGen gen("test.pdf", opts);
        {
            auto ctx = gen.new_page();
            ctx.set_line_width(2.0);
            ctx.set_stroke_color(DeviceRGBColor{0.0, 0.3, 1.0});
            ctx.rectangle(300, 100, 200, 100);
            ctx.stroke();
            ctx.simple_text("This is text in Times New Roman.",
                            gen.get_builtin_font_id(FONT_TIMES_ROMAN),
                            12,
                            100,
                            500);
            ctx.simple_text("This is text in Helvetica.",
                            gen.get_builtin_font_id(FONT_HELVETICA),
                            12,
                            100,
                            480);
            ctx.simple_text(
                "This is text in Courier.", gen.get_builtin_font_id(FONT_COURIER), 12, 100, 460);
        }
        {
            auto ctx = gen.new_page();
            ctx.rectangle(100, 300, 200, 100);
            ctx.set_nonstroke_color(DeviceRGBColor{1.0, 0.1, 0.2});
            ctx.fill();
            if(argc > 1) {
                auto image_id = gen.load_image(argv[1]);
                auto image_size = gen.get_image_info(image_id);
                ctx.save();
                ctx.concatenate_matrix(image_size.w / 5, 0, 0, image_size.h / 5, 110, 310);
                ctx.draw_image(image_id);
                ctx.restore();
            }
        }
    } catch(const std::exception &e) {
        printf("ERROR: %s\n", e.what());
        return 1;
    }

    return 0;
}
