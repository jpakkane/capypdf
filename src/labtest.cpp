// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#define _USE_MATH_DEFINES
#include <pdfgen.hpp>
#include <cmath>

using namespace capypdf;

int main(int, char **) {
    PdfGenerationData opts;

    opts.default_page_properties.mediabox->x2 = opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("L*a*b* test").value();
    opts.author = u8string::from_cstr("Test Person").value();
    opts.output_colorspace = CAPY_CS_DEVICE_RGB;
    {
        GenPopper genpop("lab_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        auto ctxguard = gen.guarded_page_context();
        auto &ctx = ctxguard.ctx;
        const LabColorSpace lab = LabColorSpace::cielab_1976_D65();
        auto labid = gen.add_lab_colorspace(lab);
        const double ball_size = 10;
        const double radius = 40;
        const double num_balls = 16;
        double max_ab = 127;
        for(int i = 0; i < num_balls; ++i) {
            auto pop = ctx.push_gstate();
            LabColor lc;
            const double angle = 2 * M_PI * i / num_balls;
            lc.id = labid;
            lc.l = 50;
            lc.a = max_ab * cos(angle);
            lc.b = max_ab * sin(angle);
            ctx.set_nonstroke_color(lc);
            ctx.translate(cos(angle) * radius, sin(angle) * radius);
            ctx.translate(50, 50);
            ctx.scale(ball_size, ball_size);
            ctx.draw_unit_circle();
            ctx.cmd_f();
        }
    }
    return 0;
}
