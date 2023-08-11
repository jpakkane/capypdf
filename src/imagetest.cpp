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
using namespace capypdf;

int basictest(int argc, char **argv) {
    PdfGenerationData opts;

    std::filesystem::path datadir = argc > 1 ? argv[1] : "../pdfgen/images";
    std::filesystem::path jpg = datadir / "simple.jpg";
    std::filesystem::path png_1bit_noalpha = datadir / "1bit_noalpha.png";
    std::filesystem::path png_1bit_alpha = datadir / "1bit_alpha.png";
    std::filesystem::path png_gray{datadir / "gray_alpha.png"};
    std::filesystem::path cmyk_tif{datadir / "cmyk_tiff.tif"};
    opts.default_page_properties.mediabox->x2 = opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("PDF image test").value();
    opts.author = u8string::from_cstr("Test Person").value();
    opts.output_colorspace = CAPYPDF_CS_DEVICE_RGB;
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

void masktest(int argc, char **argv) {
    std::filesystem::path datadir = argc > 1 ? argv[1] : "../pdfgen/images";
    const char *icc_out =
        "/home/jpakkane/Downloads/temp/Adobe ICC Profiles (end-user)/CMYK/UncoatedFOGRA29.icc";
    PdfGenerationData opts;
    std::filesystem::path lines = datadir / "comic-lines.png";
    std::filesystem::path richblack = datadir / "comic-richblack.png";
    std::filesystem::path colors = datadir / "comic-colors.png";
    opts.default_page_properties.mediabox->x2 = opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("PDF image masking test").value();
    opts.author = u8string::from_cstr("Test Person").value();
    opts.output_colorspace = CAPYPDF_CS_DEVICE_CMYK;
    opts.prof.cmyk_profile_file = icc_out;
    {
        GenPopper genpop("imagemask_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        auto ctxguard = gen.guarded_page_context();
        auto &ctx = ctxguard.ctx;
        auto stencil_img = gen.load_mask_image(lines.c_str()).value();
        GraphicsState gstate;
        gstate.OP = true;
        gstate.op = true;
        gstate.OPM = 1;
        auto sid = gen.add_graphics_state(gstate);
        {
            auto q = ctx.push_gstate();
            ctx.cmd_k(0, 1, 0, 0);
            ctx.cmd_re(10, 130, 40, 10);
            ctx.cmd_f();
            ctx.cmd_k(0.5, 0, 0.5, 0);
            ctx.cmd_re(15, 135, 10, 10);
            ctx.cmd_f();
            ctx.cmd_gs(sid);
            ctx.cmd_re(35, 135, 10, 10);
            ctx.cmd_f();
        }
        {
            auto q = ctx.push_gstate();
            ctx.cmd_k(0.3, 1, 0.2, 0);
            ctx.translate(10, 10);
            ctx.scale(72, 72);
            ctx.draw_image(stencil_img);
        }

        auto line_img = gen.load_mask_image(lines.c_str()).value();
        auto richblack_img = gen.load_mask_image(richblack.c_str()).value();
        auto color_img = gen.load_image(colors.c_str()).value();
        ctx.cmd_k(0.2, 0.2, 0.2, 0);
        {
            auto q = ctx.push_gstate();
            ctx.translate(110, 10);
            ctx.scale(72, 72);
            ctx.draw_image(color_img);
            ctx.draw_image(richblack_img);
            ctx.cmd_gs(sid);
            ctx.cmd_k(0, 0, 0, 1);
            ctx.draw_image(line_img);
        }
    }
}

int main(int argc, char **argv) {
    basictest(argc, argv);
    masktest(argc, argv);
    return 0;
}
