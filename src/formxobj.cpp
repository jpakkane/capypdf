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
    opts.title = "Acroform  test";
    opts.author = "Test Person";
    opts.output_colorspace = A4PDF_CS_DEVICE_RGB;
    {
        GenPopper genpop("foxbj_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        A4PDF_FormXObjectId xid;
        {
            PdfDrawContext xobj = gen.new_form_xobject(10, 10);
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
