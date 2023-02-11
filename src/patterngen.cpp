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

int main() {
    PdfGenerationData opts;
    opts.mediabox.w = opts.mediabox.h = 200;
    opts.title = "PDF pattern test";
    opts.author = "Test Person";
    opts.output_colorspace = A4PDF_DEVICE_RGB;
    {
        PdfGen gen("pattern_test.pdf", opts);
        auto &ctx = gen.page_context();
        {
            auto pop = ctx.push_gstate();
            ctx.translate(0, 100);
            FunctionType2 rgbfunc{{0.0, 1.0}, {0.0, 1.0, 0.0}, {1.0, 0.0, 1.0}, 1.0};
            auto funcid = gen.add_function(rgbfunc);

            ShadingType2 shade;
            shade.colorspace = A4PDF_DEVICE_RGB;
            shade.x0 = 10;
            shade.y0 = 50;
            shade.x1 = 90;
            shade.y1 = 50;
            shade.function = funcid;
            shade.extend0 = false;
            shade.extend1 = false;

            auto shadeid = gen.add_shading(shade);
            ctx.cmd_re(10, 10, 80, 80);
            // Shadings fill the entire clipping area. It does not
            // stick to what is inside the "current path".
            ctx.cmd_Wstar();
            ctx.cmd_n();
            ctx.cmd_sh(shadeid);
        }
        {
            auto pop = ctx.push_gstate();
            ctx.translate(100, 100);
            FunctionType2 rgbfunc{{0.0, 1.0}, {1.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, 1.0};
            auto funcid = gen.add_function(rgbfunc);
            ShadingType3 shade;
            shade.colorspace = A4PDF_DEVICE_RGB;
            shade.x0 = 50;
            shade.y0 = 50;
            shade.r0 = 40;
            shade.x1 = 40;
            shade.y1 = 30;
            shade.r1 = 10;
            shade.function = funcid;
            shade.extend0 = false;
            shade.extend1 = true;

            auto shadeid = gen.add_shading(shade);
            ctx.cmd_sh(shadeid);
        }
    }
    return 0;
}
