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
#include <cmath>
#include <span>

using namespace capypdf;

const std::array<const char *, 12> trnames{
    "Split",
    "Blinds",
    "Box",
    "Wipe",
    "Dissolve",
    "Glitter",
    "R",
    "Fly",
    "Push",
    "Cover",
    "Uncover",
    "Fade",
};

int main(int, char **) {
    PdfGenerationData opts;

    const int32_t w = 160;
    const int32_t h = 90;
    opts.mediabox.x2 = w;
    opts.mediabox.y2 = h;
    opts.title = "Presentation test";
    opts.author = "Joe Speaker";
    opts.output_colorspace = CAPYPDF_CS_DEVICE_RGB;
    {
        GenPopper genpop("presentation.pdf", opts);
        PdfGen &gen = *genpop.g;
        PageTransition transition;
        transition.duration = 1.0;
        transition.Dm = true;
        transition.M = false;
        transition.Di = 90;
        {
            auto ctxh = std::unique_ptr<PdfDrawContext>{gen.new_page_draw_context()};
            auto &ctx = *ctxh;
            ctx.cmd_rg(0, 0, 0);
            ctx.render_pdfdoc_text_builtin(
                "Transition styles", CAPY_FONT_HELVETICA_BOLD, 16, 10, 45);
            gen.add_page(ctx);
            for(size_t i = 0; i < trnames.size(); ++i) {
                transition.type = CAPYPDF_Transition_Type(i);
                if(i % 2) {
                    ctx.cmd_rg(0.9, 0, 0.0);
                } else {
                    ctx.cmd_rg(0, 0.7, 0.0);
                }
                ctx.cmd_re(0, 0, w, h);
                ctx.cmd_f();
                ctx.cmd_rg(0, 0, 0);
                ctx.render_pdfdoc_text_builtin(trnames.at(i), CAPY_FONT_HELVETICA_BOLD, 14, 30, 35);
                ctx.set_transition(transition);
                gen.add_page(ctx);
            }
        }
    }
    return 0;
}
