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
#include <filesystem>
using namespace A4PDF;

int main(int argc, char **argv) {
    PdfGenerationData opts;

    std::filesystem::path datadir = argc > 1 ? argv[1] : "../pdfgen/images";
    std::filesystem::path jpg = datadir / "simple.jpg";
    std::filesystem::path png_1bit_noalpha = datadir / "1bit_noalpha.png";
    std::filesystem::path png_1bit_alpha = datadir / "1bit_alpha.png";
    std::filesystem::path png_gray{datadir / "gray_alpha.png"};
    std::filesystem::path cmyk_tif{datadir / "cmyk_tiff.tif"};
    opts.mediabox.w = opts.mediabox.h = 200;
    opts.title = "PDF image test";
    opts.author = "Test Person";
    opts.output_colorspace = A4PDF_DEVICE_RGB;
    {
        GenPopper genpop("image_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        auto ctxguard = gen.guarded_page_context();
        auto &ctx = ctxguard.ctx;
        auto bg_img = gen.embed_jpg(jpg.c_str()).value();
        auto mono_img = gen.load_image(png_1bit_alpha.c_str()).value();
        auto gray_img = gen.load_image(png_gray.c_str()).value();
        auto cmyk_img = gen.load_image(cmyk_tif.c_str()).value();
        ctx.cmd_re(0, 0, 200, 200);
        ctx.set_nonstroke_color(DeviceRGBColor{0.9, 0.9, 0.9});
        ctx.cmd_f();
        {
            auto pop = ctx.push_gstate();
            ctx.translate(10, 10);
            ctx.scale(80, 80);
            ctx.draw_image(bg_img);
        }
        {
            auto pop = ctx.push_gstate();
            ctx.translate(0, 100);
            ctx.translate(10, 10);
            ctx.scale(80, 80);
            ctx.draw_image(mono_img);
        }
        {
            auto pop = ctx.push_gstate();
            ctx.translate(100, 100);
            ctx.translate(10, 10);
            ctx.scale(80, 80);
            ctx.draw_image(gray_img);
        }
        {
            auto pop = ctx.push_gstate();
            ctx.translate(100, 0);
            ctx.translate(10, 10);
            ctx.scale(80, 80);
            ctx.draw_image(cmyk_img);
        }
    }
    return 0;
}
