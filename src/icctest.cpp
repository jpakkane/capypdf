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

using namespace capypdf;

int main(int argc, char **argv) {
    PdfGenerationData opts;

    std::filesystem::path icc_file{argc == 1 ? "/usr/share/color/icc/colord/AdobeRGB1998.icc"
                                             : argv[2]};
    opts.mediabox.x2 = opts.mediabox.y2 = 200;
    opts.title = "ICC test";
    opts.author = "Test Person";
    opts.output_colorspace = CAPYPDF_CS_DEVICE_RGB;
    {
        GenPopper genpop("icc_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        auto ctxguard = gen.guarded_page_context();
        auto &ctx = ctxguard.ctx;
        auto icc_id = gen.load_icc_file(icc_file).value();

        ICCColor blueish{icc_id, {0.1, 0.2, 0.9}};
        ICCColor reddish{icc_id, {0.8, 0.3, 0.1}};
        ctx.set_stroke_color(blueish);
        ctx.set_nonstroke_color(reddish);

        ctx.cmd_w(5);
        ctx.cmd_re(40, 40, 120, 120);
        ctx.cmd_B();
    }
    return 0;
}
