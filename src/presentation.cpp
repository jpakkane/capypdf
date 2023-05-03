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

using namespace A4PDF;

int main(int argc, char **argv) {
    PdfGenerationData opts;

    const int32_t w = 160;
    const int32_t h = 90;
    opts.mediabox.w = w;
    opts.mediabox.h = h;
    opts.title = "Presentation test";
    opts.author = "Joe Speaker";
    opts.output_colorspace = A4PDF_DEVICE_RGB;
    {
        GenPopper genpop("presentation.pdf", opts);
        PdfGen &gen = *genpop.g;
        PageTransition transition;
        transition.duration = 1.0;
        {
            auto ctxh = std::unique_ptr<PdfDrawContext>{gen.new_page_draw_context()};
            auto &ctx = *ctxh;
            ctx.cmd_rg(1, 0, 0);
            ctx.cmd_re(0, 0, w, h);
            ctx.cmd_f();
            ctx.cmd_rg(0, 0, 0);
            ctx.render_pdfdoc_text_builtin("Title", A4PDF_FONT_HELVETICA_BOLD, 16, 60, 45);
            gen.add_page(ctx);
            transition.type = TransitionType::Blinds;
            ctx.cmd_rg(0, 1, 0);
            ctx.cmd_re(0, 0, w, h);
            ctx.cmd_f();
            ctx.cmd_rg(0, 0, 0);
            ctx.render_pdfdoc_text_builtin("Heading", A4PDF_FONT_HELVETICA_BOLD, 14, 30, 75);
            gen.add_page(ctx, transition);
        }
    }
    return 0;
}
