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

using namespace A4PDF;

int main(int, char **) {
    PdfGenerationData opts;

    opts.mediabox.w = opts.mediabox.h = 200;
    opts.title = "Outline test";
    opts.author = "Test Person";
    opts.output_colorspace = A4PDF_DEVICE_RGB;
    {
        PdfGen gen("outline_test.pdf", opts);
        std::unique_ptr<PdfDrawContext> g{gen.new_page_draw_context()};
        g->cmd_re(10, 10, 10, 10);
        g->cmd_f();
        auto page_id = gen.add_page(*g);
        gen.add_outline("First entry", page_id, {});
    }
    return 0;
}
