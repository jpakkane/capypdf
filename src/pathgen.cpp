/*
 * Copyright 2023 Jussi Pakkanen
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

using namespace A4PDF;

static void draw_intersect_shape(PdfPageBuilder &ctx) {
    ctx.cmd_m(50, 90);
    ctx.cmd_l(80, 10);
    ctx.cmd_l(10, 60);
    ctx.cmd_l(90, 60);
    ctx.cmd_l(20, 10);
    ctx.cmd_h();
}

int main(int, char **) {
    PdfGenerationData opts;

    opts.mediabox.w = opts.mediabox.h = 200;
    opts.title = "PDF path test";
    opts.author = "Test Person";
    opts.output_colorspace = A4PDF_DEVICE_RGB;

    {
        PdfGen gen("path_test.pdf", opts);
        auto &ctx = gen.page_context();
        ctx.cmd_w(5);
        ctx.cmd_m(10, 10);
        ctx.cmd_c(80, 10, 20, 90, 90, 90);
        ctx.cmd_S();

        ctx.cmd_q();
        ctx.translate(100, 0);
        ctx.set_stroke_color(DeviceRGBColor{1.0, 0.0, 0.0});
        ctx.set_nonstroke_color(DeviceRGBColor{0.9, 0.4, 0.7});
        ctx.cmd_m(50, 90);
        ctx.cmd_l(10, 10);
        ctx.cmd_l(90, 10);
        ctx.cmd_h();
        ctx.cmd_B();
        ctx.cmd_Q();

        ctx.cmd_q();
        ctx.translate(0, 100);
        draw_intersect_shape(ctx);
        ctx.cmd_w(3);
        ctx.set_nonstroke_color(DeviceRGBColor{0, 1, 0});
        ctx.set_stroke_color(DeviceRGBColor{0.5, 0.1, 0.5});
        ctx.cmd_B();
        ctx.cmd_Q();

        ctx.cmd_q();
        ctx.translate(100, 100);
        ctx.cmd_w(2);
        ctx.set_nonstroke_color(DeviceRGBColor{0, 1, 0});
        ctx.set_stroke_color(DeviceRGBColor{0.5, 0.1, 0.5});
        draw_intersect_shape(ctx);
        ctx.cmd_Bstar();
        ctx.cmd_Q();
    }
    return 0;
}
