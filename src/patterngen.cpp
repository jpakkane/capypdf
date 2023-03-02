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
        GenPopper genpop("pattern_test.pdf", opts);
        PdfGen &gen = genpop.g;
        auto ctxguard = gen.guarded_page_context();
        auto &ctx = ctxguard.ctx;
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
            FunctionType2 rgbfunc{{0.0, 1.0}, {1.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, 0.7};
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
        {
            auto pop = ctx.push_gstate();
            auto pattern = gen.new_color_pattern_builder(10, 10);
            auto &pctx = pattern.pctx;
            pctx.set_nonstroke_color(DeviceRGBColor{0.9, 0.8, 0.8});
            pctx.cmd_re(0, 0, 10, 10);
            pctx.cmd_f();
            pctx.set_nonstroke_color(DeviceRGBColor{0.9, 0.1, 0.1});
            pctx.cmd_re(0, 2.5, 2.5, 5);
            pctx.cmd_f();
            pctx.cmd_re(5, 0, 2.5, 2.5);
            pctx.cmd_f();
            pctx.cmd_re(5, 7.5, 2.5, 2.5);
            pctx.cmd_f();
            auto patternid = gen.add_pattern(pattern);

            ctx.cmd_re(10, 10, 80, 80);
            ctx.set_nonstroke_color(patternid);
            ctx.set_stroke_color(DeviceRGBColor{0, 0, 0});
            ctx.cmd_j(A4PDF_Round_Join);
            ctx.cmd_w(1.5);
            ctx.cmd_B();
        }
        {
            auto pop = ctx.push_gstate();
            auto pattern = gen.new_color_pattern_builder(3, 3);
            auto &pctx = pattern.pctx;

            pctx.render_ascii_text_builtin("g", A4PDF_FONT_TIMES_ROMAN, 3, 0, 2);
            auto patternid = gen.add_pattern(pattern);

            ctx.translate(100, 10);
            ctx.set_nonstroke_color(patternid);
            ctx.render_ascii_text_builtin("C", A4PDF_FONT_TIMES_ROMAN, 120, 0, 5);
        }
    }
    return 0;
}
