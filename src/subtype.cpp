// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <pdfgen.hpp>
#include <errorhandling.hpp>
#include <cmath>

using namespace capypdf;

int main(int argc, char **argv) {
    PdfGenerationData opts;

    opts.default_page_properties.mediabox->x2 = opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("Form XObject test").value();
    opts.author = u8string::from_cstr("Test Person").value();
    opts.output_colorspace = CAPY_CS_DEVICE_RGB;
    opts.subtype = CAPY_INTENT_SUBTYPE_PDFA;
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
            ctx.render_text(
                u8string::from_cstr("This is a PDF/A-3 document.").value(), font, 12, 20, 94);
        }
    }
    return 0;
}
