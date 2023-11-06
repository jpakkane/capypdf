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

using namespace capypdf;

int main(int argc, char **argv) {
    if(argc != 3) {
        printf("%s <bg file> <fg file>\n", argv[0]);
        return 1;
    }
    PdfGenerationData opts;
    opts.output_colorspace = CAPY_CS_DEVICE_RGB;
    opts.default_page_properties.mediabox->x1 = opts.default_page_properties.mediabox->y1 = 0;
    opts.default_page_properties.mediabox->x2 = 300;
    opts.default_page_properties.mediabox->y2 = 300;
    GenPopper genpop("gstate.pdf", opts);
    PdfGen &gen = *genpop.g;
    auto ctxguard = gen.guarded_page_context();
    auto &ctx = ctxguard.ctx;
    GraphicsState gs;
    gs.BM = CAPY_BM_MULTIPLY;
    gs.RI = CAPY_RI_PERCEPTUAL;
    auto bg_img = gen.load_image(argv[1]).value();
    auto fg_img = gen.load_image(argv[2]).value();
    ctx.cmd_q();
    ctx.scale(opts.default_page_properties.mediabox->x2, opts.default_page_properties.mediabox->y2);
    ctx.draw_image(bg_img);
    ctx.cmd_Q();
    // There are 16 blend modes.
    const int imsize = 40;
    CapyPDF_Blend_Mode bm = CAPY_BM_NORMAL;
    for(int j = 3; j >= 0; --j) {
        for(int i = 0; i < 4; ++i) {
            GraphicsState gs;
            gs.BM = bm;
            auto sid = gen.add_graphics_state(gs).value();
            ctx.cmd_q();
            ctx.cmd_gs(sid);
            ctx.translate((i + 0.5) * 1.5 * imsize, (j + 0.5) * 1.5 * imsize);
            ctx.scale(imsize, imsize);
            ctx.draw_image(fg_img);
            ctx.cmd_Q();
            ctx.cmd_q();
            ctx.translate((i + 0.5) * 1.5 * imsize, (j + 0.3) * 1.5 * imsize);
            ctx.render_pdfdoc_text_builtin(gstate_names.at(bm), CAPY_FONT_HELVETICA, 8, 0, 0);
            ctx.cmd_Q();
            bm = (CapyPDF_Blend_Mode)((int)bm + 1);
        }
    }
    return 0;
}
