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
#include <errorhandling.hpp>
#include <cmath>
#include <span>

using namespace A4PDF;

int main(int argc, char **argv) {
    PdfGenerationData opts;

    opts.mediabox.w = opts.mediabox.h = 200;
    opts.title = "Form XObject test";
    opts.author = "Test Person";
    opts.output_colorspace = A4PDF_CS_DEVICE_RGB;
    opts.subtype = IntentSubtype::SUBTYPE_PDFA;
    opts.intent_condition_identifier = "sRGB IEC61966-2.1";
    opts.prof.rgb_profile_file = "/usr/share/color/icc/ghostscript/srgb.icc";
    {
        GenPopper genpop("apdf_test.pdf", opts);
        PdfGen &gen = *genpop.g;

        {
            auto ctxguard = gen.guarded_page_context();
            auto &ctx = ctxguard.ctx;

            auto font =
                gen.load_font("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf")
                    .value();
            ctx.render_utf8_text("This is a PDF/A-3 document.", font, 12, 20, 94);
        }
    }
    return 0;
}
