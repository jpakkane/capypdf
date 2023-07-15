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

int draw_simple_form() {
    PdfGenerationData opts;

    opts.mediabox.x2 = opts.mediabox.y2 = 200;
    opts.title = "Form XObject test";
    opts.author = "Test Person";
    opts.output_colorspace = CAPYPDF_CS_DEVICE_RGB;
    {
        GenPopper genpop("form_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        CapyPDF_FormXObjectId offstate, onstate;
        {
            PdfDrawContext xobj = gen.new_form_xobject(10, 10);
            xobj.cmd_BMC("/Tx");
            xobj.cmd_EMC();
            auto rv = gen.add_form_xobject(xobj);
            if(!rv) {
                fprintf(stderr, "%s\n", error_text(rv.error()));
                return 1;
            }
            offstate = *rv;
        }
        {
            PdfDrawContext xobj = gen.new_form_xobject(10, 10);
            xobj.cmd_BMC("/Tx");
            xobj.cmd_q();
            xobj.render_pdfdoc_text_builtin("X", CAPY_FONT_HELVETICA, 12, 0, 0);
            xobj.cmd_Q();
            xobj.cmd_EMC();
            auto rv = gen.add_form_xobject(xobj);
            if(!rv) {
                fprintf(stderr, "%s\n", error_text(rv.error()));
                return 1;
            }
            onstate = *rv;
        }

        auto checkbox_widget =
            gen.create_form_checkbox(PdfBox{10, 80, 20, 90}, onstate, offstate, "checkbox1")
                .value();
        {
            auto ctxguard = gen.guarded_page_context();
            auto &ctx = ctxguard.ctx;

            ctx.cmd_re(10, 80, 10, 10);
            ctx.cmd_S();

            ctx.render_pdfdoc_text_builtin("A checkbox", CAPY_FONT_HELVETICA, 12, 25, 80);
            auto rc = ctx.add_form_widget(checkbox_widget);
            if(rc != ErrorCode::NoError) {
                fprintf(stderr, "FAIL\n");
                return 1;
            }
        }
    }
    return 0;
}

void draw_gradient(PdfDrawContext &ctx, ShadingId shadeid, double x, double y) {
    ctx.translate(x, y);
    ctx.cmd_re(0, 0, 80, 80);
    ctx.cmd_Wstar();
    ctx.cmd_n();
    ctx.cmd_sh(shadeid);
}

int draw_group_doc() {
    // PDF 2.0 spec page 409.
    PdfGenerationData opts;
    const char *icc_out =
        "/home/jpakkane/Downloads/temp/Adobe ICC Profiles (end-user)/CMYK/UncoatedFOGRA29.icc";

    opts.mediabox.x2 = 300;
    opts.mediabox.y2 = 200;
    opts.title = "Transparency group test";
    opts.author = "Test Person";
    opts.output_colorspace = CAPYPDF_CS_DEVICE_CMYK;
    opts.prof.cmyk_profile_file = icc_out;
    {
        GenPopper genpop("group_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        FunctionType2 cmykfunc{{0.0, 1.0}, {0.0, 1.0, 0.0, 0.0}, {1.0, 0.0, 1.0, 0.0}, 1.0};
        auto funcid = gen.add_function(cmykfunc);

        ShadingType2 shade;
        shade.colorspace = CAPYPDF_CS_DEVICE_CMYK;
        shade.x0 = 0;
        shade.y0 = 40;
        shade.x1 = 80;
        shade.y1 = 40;
        shade.function = funcid;
        shade.extend0 = false;
        shade.extend1 = false;

        auto shadeid = gen.add_shading(shade);
        {
            auto ctxguard = gen.guarded_page_context();
            auto &ctx = ctxguard.ctx;
            ctx.render_pdfdoc_text_builtin("Isolated", CAPY_FONT_HELVETICA, 8, 5, 150);
            ctx.render_pdfdoc_text_builtin("Non-isolated", CAPY_FONT_HELVETICA, 8, 5, 50);
            ctx.render_pdfdoc_text_builtin("Knockout", CAPY_FONT_HELVETICA, 8, 100, 5);
            ctx.render_pdfdoc_text_builtin("Non-knockout", CAPY_FONT_HELVETICA, 8, 200, 5);
            {
                auto rc = ctx.push_gstate();
                draw_gradient(ctx, shadeid, 80, 20);
            }
            {
                auto rc = ctx.push_gstate();
                draw_gradient(ctx, shadeid, 80, 110);
            }
            {
                auto rc = ctx.push_gstate();
                draw_gradient(ctx, shadeid, 180, 20);
            }
            {
                auto rc = ctx.push_gstate();
                draw_gradient(ctx, shadeid, 180, 110);
            }
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if(draw_simple_form() != 0) {
        return 1;
    }
    return draw_group_doc();
}
