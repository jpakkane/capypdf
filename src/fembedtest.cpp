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

    opts.mediabox.w = opts.mediabox.h = 200;
    opts.title = "File embedding test";
    opts.author = "Test Person";
    {
        GenPopper genpop("fembed_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        auto efid = gen.embed_file("embed.txt").value();
        auto annoid =
            gen.create_annotation(PdfBox{35, 95, 45, 105}, "", FileAttachmentAnnotation{efid})
                .value();
        {
            auto ctxguard = gen.guarded_page_context();
            auto &ctx = ctxguard.ctx;

            ctx.render_pdfdoc_text_builtin(
                "<- an embedded file.", A4PDF_FONT_HELVETICA, 12, 50, 100);
            ctx.annotate(annoid);
        }
    }

    return 0;
}
