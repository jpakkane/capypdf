// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <pdfgen.hpp>
#include <cmath>

using namespace capypdf;

int main(int argc, char **argv) {
    PdfGenerationData opts;

    opts.default_page_properties.mediabox->x2 = opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("Acroform  test").value();
    opts.author = u8string::from_cstr("Test Person").value();
    opts.output_colorspace = CAPY_DEVICE_CS_RGB;
    {
        GenPopper genpop("foxbj_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        CapyPDF_FormXObjectId xid;
        {
            auto xobj_h = gen.guarded_form_xobject(10, 10);
            auto &xobj = xobj_h.ctx;
            xobj.cmd_w(1);
            xobj.cmd_re(0, 0, 10, 10);
            xobj.cmd_S();
            xobj.cmd_w(2);
            xobj.cmd_m(2, 2);
            xobj.cmd_l(8, 8);
            xobj.cmd_m(2, 8);
            xobj.cmd_l(8, 2);
            xobj.cmd_S();
            auto rv = gen.add_form_xobject(xobj);
            if(!rv) {
                fprintf(stderr, "%s\n", error_text(rv.error()));
                return 1;
            }
            xid = *rv;
        }

        auto ctxguard = gen.guarded_page_context();
        auto &ctx = ctxguard.ctx;
        {
            auto g = ctx.push_gstate();
            ctx.translate(10, 95);
            ctx.cmd_Do(xid);
        }
        {
            auto g = ctx.push_gstate();
            ctx.translate(180, 95);
            ctx.cmd_Do(xid);
        }
        {
            auto g = ctx.push_gstate();
            ctx.translate(95, 10);
            ctx.cmd_Do(xid);
        }
        {
            auto g = ctx.push_gstate();
            ctx.translate(95, 180);
            ctx.cmd_Do(xid);
        }
    }
    return 0;
}
