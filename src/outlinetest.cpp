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

int main(int, char **) {
    PdfGenerationData opts;

    opts.mediabox.x2 = opts.mediabox.y2 = 200;
    opts.title = u8string::from_cstr("Outline test").value();
    opts.author = u8string::from_cstr("Test Person").value();
    opts.output_colorspace = CAPYPDF_CS_DEVICE_RGB;
    {
        GenPopper genpop("outline_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        std::unique_ptr<PdfDrawContext> g{gen.new_page_draw_context()};
        g->cmd_re(10, 10, 10, 10);
        g->cmd_f();
        auto page_id = gen.add_page(*g).value();
        auto t1 = gen.add_outline("First toplevel", page_id, {});
        auto t2 = gen.add_outline("Second toplevel", page_id, {});
        (void)t2;
        gen.add_outline("Third toplevel", page_id, {});
        auto t1c1 = gen.add_outline("Top1 child1", page_id, t1);
        (void)t1c1;
        auto t1c2 = gen.add_outline("Top1 child2", page_id, t1);
        gen.add_outline("Top1 child2 child1", page_id, t1c2);
        gen.add_outline("Fourth toplevel", page_id, {});
    }
    return 0;
}
