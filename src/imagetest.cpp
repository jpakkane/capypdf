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

int main(int argc, char **argv) {
    PdfGenerationData opts;

    const char *image = argc > 1 ? argv[1] : "../pdfgen/images/simple.jpg";
    opts.mediabox.w = opts.mediabox.h = 200;
    opts.title = "PDF path test";
    opts.author = "Test Person";
    opts.output_colorspace = A4PDF_DEVICE_RGB;
    {
        PdfGen gen("image_test.pdf", opts);
        auto &ctx = gen.page_context();
        auto bg_img = gen.embed_jpg(image);
        ctx.translate(10, 10);
        ctx.scale(80, 80);
        ctx.draw_image(bg_img);
    }
    return 0;
}
