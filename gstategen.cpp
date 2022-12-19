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

int main(int argc, char **argv) {
    PdfGenerationData opts;
    opts.page_size.h = 200;
    opts.page_size.w = 200;
    opts.mediabox.x = opts.mediabox.y = 0;
    opts.mediabox.w = opts.page_size.w;
    opts.mediabox.h = opts.page_size.h;
    PdfGen gen("gstate.pdf", opts);
    auto ctx = gen.new_page();
    GraphicsState gs;
    gs.blend_mode = BM_MULTIPLY;
    gs.intent = RI_PERCEPTUAL;
    ctx.add_graphics_state("blub", gs);
    ctx.cmd_gs("blub");
    return 0;
}
