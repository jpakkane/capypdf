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
#include <span>

using namespace A4PDF;

int main(int argc, char **argv) {
    PdfGenerationData opts;

    opts.mediabox.w = opts.mediabox.h = 200;
    opts.title = "Form XObject test";
    opts.author = "Test Person";
    opts.output_colorspace = A4PDF_DEVICE_RGB;
    {
        GenPopper genpop("form_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        A4PDF_FormXObjectId offstate, onstate;
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
            xobj.render_pdfdoc_text_builtin("X", A4PDF_FONT_HELVETICA, 12, 0, 0);
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

            ctx.render_pdfdoc_text_builtin("A checkbox", A4PDF_FONT_HELVETICA, 12, 25, 80);
            auto rc = ctx.add_form_widget(checkbox_widget);
            if(rc != ErrorCode::NoError) {
                fprintf(stderr, "FAIL\n");
                return 1;
            }
        }
    }
    return 0;
}
