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
#include <fmt/core.h>
#include <array>

const std::array<const char *, 16> gstate_names{"NORMAL",
                                                "MULTIPLY",
                                                "SCREEN",
                                                "OVERLAY",
                                                "DARKEN",
                                                "LIGHTEN",
                                                "COLORDODGE",
                                                "COLORBURN",
                                                "HARDLIGHT",
                                                "SOFTLIGHT",
                                                "DIFFERENCE",
                                                "EXCLUSION",
                                                "HUE",
                                                "SATURATION",
                                                "COLOR",
                                                "LUMINOSITY"};

using namespace A4PDF;

int main(int argc, char **argv) {
    if(argc != 3) {
        printf("%s <bg file> <fg file>\n", argv[0]);
        return 1;
    }
    PdfGenerationData opts;
    opts.output_colorspace = A4PDF_DEVICE_RGB;
    opts.mediabox.x = opts.mediabox.y = 0;
    opts.mediabox.w = 300;
    opts.mediabox.h = 300;
    GenPopper genpop("gstate.pdf", opts);
    PdfGen &gen = *genpop.g;
    auto ctxguard = gen.guarded_page_context();
    auto &ctx = ctxguard.ctx;
    GraphicsState gs;
    gs.blend_mode = A4PDF_BM_MULTIPLY;
    gs.intent = A4PDF_RI_PERCEPTUAL;
    auto bg_img = gen.load_image(argv[1]).value();
    auto fg_img = gen.load_image(argv[2]).value();
    ctx.cmd_q();
    ctx.scale(opts.mediabox.w, opts.mediabox.h);
    ctx.draw_image(bg_img);
    ctx.cmd_Q();
    // There are 16 blend modes.
    const int imsize = 40;
    A4PDF_Blend_Mode bm = A4PDF_BM_NORMAL;
    for(int j = 3; j >= 0; --j) {
        for(int i = 0; i < 4; ++i) {
            GraphicsState gs;
            gs.blend_mode = bm;
            auto gs_name = fmt::format("bm{}", (int)bm);
            auto sid = gen.add_graphics_state(gs);
            ctx.cmd_q();
            ctx.cmd_gs(sid);
            ctx.translate((i + 0.5) * 1.5 * imsize, (j + 0.5) * 1.5 * imsize);
            ctx.scale(imsize, imsize);
            ctx.draw_image(fg_img);
            ctx.cmd_Q();
            ctx.cmd_q();
            ctx.translate((i + 0.5) * 1.5 * imsize, (j + 0.3) * 1.5 * imsize);
            ctx.render_pdfdoc_text_builtin(gstate_names.at(bm), A4PDF_FONT_HELVETICA, 8, 0, 0);
            ctx.cmd_Q();
            bm = (A4PDF_Blend_Mode)((int)bm + 1);
        }
    }
    return 0;
}
