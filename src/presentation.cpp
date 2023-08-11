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

const std::array<const char *, 12> trnames{
    "Split",
    "Blinds",
    "Box",
    "Wipe",
    "Dissolve",
    "Glitter",
    "R",
    "Fly",
    "Push",
    "Cover",
    "Uncover",
    "Fade",
};

void create_presentation() {
    PdfGenerationData opts;

    const int32_t w = 160;
    const int32_t h = 90;
    opts.default_page_properties.mediabox->x2 = w;
    opts.default_page_properties.mediabox->y2 = h;
    opts.title = u8string::from_cstr("Presentation test").value();
    opts.author = u8string::from_cstr("Joe Speaker").value();
    opts.output_colorspace = CAPYPDF_CS_DEVICE_RGB;
    {
        GenPopper genpop("presentation.pdf", opts);
        PdfGen &gen = *genpop.g;
        Transition transition;
        transition.duration = 1.0;
        transition.Dm = true;
        transition.M = false;
        transition.Di = 90;
        {
            auto ctxh = std::unique_ptr<PdfDrawContext>{gen.new_page_draw_context()};
            auto &ctx = *ctxh;
            ctx.cmd_rg(0, 0, 0);
            ctx.render_pdfdoc_text_builtin(
                "Transition styles", CAPY_FONT_HELVETICA_BOLD, 16, 10, 45);
            gen.add_page(ctx);
            for(size_t i = 0; i < trnames.size(); ++i) {
                transition.type = CAPYPDF_Transition_Type(i);
                if(i % 2) {
                    ctx.cmd_rg(0.9, 0, 0.0);
                } else {
                    ctx.cmd_rg(0, 0.7, 0.0);
                }
                ctx.cmd_re(0, 0, w, h);
                ctx.cmd_f();
                ctx.cmd_rg(0, 0, 0);
                ctx.render_pdfdoc_text_builtin(trnames.at(i), CAPY_FONT_HELVETICA_BOLD, 14, 30, 35);
                ctx.set_transition(transition);
                gen.add_page(ctx);
            }
        }
    }
}

void create_subpage() {
    PdfGenerationData opts;
    const int32_t w = 160;
    const int32_t h = 90;
    opts.default_page_properties.mediabox->x2 = w;
    opts.default_page_properties.mediabox->y2 = h;
    opts.title = u8string::from_cstr("Subpage navigation").value();
    opts.author = u8string::from_cstr("Joe Speaker").value();
    opts.output_colorspace = CAPYPDF_CS_DEVICE_RGB;
    {
        GenPopper genpop("subpage.pdf", opts);
        PdfGen &gen = *genpop.g;
        {
            auto ctxguard = gen.guarded_page_context();
            auto &ctx = ctxguard.ctx;
            ctx.render_pdfdoc_text_builtin("This is page 1", CAPY_FONT_HELVETICA, 14, 20, 40);
        }
        {
            auto ctxguard = gen.guarded_page_context();
            auto &ctx = ctxguard.ctx;
            OptionalContentGroup group;
            Transition tr{CAPY_TR_DISSOLVE, 1.0, {}, {}, {}, {}, {}};
            group.name = "bullet1";
            std::vector<CapyPDF_OptionalContentGroupId> ocgs;
            auto g1 = gen.add_optional_content_group(group).value();
            ocgs.emplace_back(g1);
            group.name = "bullet2";
            auto g2 = gen.add_optional_content_group(group).value();
            ocgs.emplace_back(g2);
            ctx.render_pdfdoc_text_builtin("Heading", CAPY_FONT_HELVETICA, 14, 50, 70);
            ctx.cmd_BDC(g1);
            ctx.render_pdfdoc_text_builtin("Bullet 1", CAPY_FONT_HELVETICA, 12, 20, 50);
            ctx.cmd_EMC();
            ctx.cmd_BDC(g2);
            ctx.render_pdfdoc_text_builtin("Bullet 2", CAPY_FONT_HELVETICA, 12, 20, 30);
            ctx.cmd_EMC();
            ctx.add_simple_navigation(ocgs, tr);
        }
        {
            auto ctxguard = gen.guarded_page_context();
            auto &ctx = ctxguard.ctx;
            ctx.render_pdfdoc_text_builtin("This is page 3", CAPY_FONT_HELVETICA, 14, 20, 40);
        }
    }
}

int main(int, char **) {
    create_presentation();
    create_subpage();
    return 0;
}
