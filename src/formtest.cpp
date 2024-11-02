// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <generator.hpp>
#include <cmath>

using namespace capypdf::internal;

int draw_simple_form() {
    DocumentMetadata opts;

    opts.default_page_properties.mediabox->x2 = opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("Form XObject test").value();
    opts.author = u8string::from_cstr("Test Person").value();
    opts.output_colorspace = CAPY_DEVICE_CS_RGB;
    {
        GenPopper genpop("form_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        CapyPDF_FormXObjectId offstate, onstate;
        {
            auto xobj_h = gen.guarded_form_xobject(10, 10);
            auto &xobj = xobj_h.ctx;
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
            auto xobj_h = gen.guarded_form_xobject(10, 10);
            auto &xobj = xobj_h.ctx;
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
            if(!rc) {
                fprintf(stderr, "FAIL\n");
                return 1;
            }
        }
    }
    return 0;
}

void draw_gradient(PdfDrawContext &ctx, CapyPDF_ShadingId shadeid, double x, double y) {
    ctx.translate(x, y);
    ctx.cmd_re(0, 0, 80, 80);
    ctx.cmd_Wstar();
    ctx.cmd_n();
    ctx.cmd_sh(shadeid);
}

void draw_circles(PdfDrawContext &ctx, CapyPDF_GraphicsStateId gsid) {
    ctx.cmd_gs(gsid);
    ctx.cmd_k(0, 0, 0, 0.15);
    {
        auto g = ctx.push_gstate();
        ctx.translate(30, 30);
        ctx.scale(40, 40);
        ctx.draw_unit_circle();
        ctx.cmd_f();
    }
    {
        auto g = ctx.push_gstate();
        ctx.translate(50, 30);
        ctx.scale(40, 40);
        ctx.draw_unit_circle();
        ctx.cmd_f();
    }
    {
        auto g = ctx.push_gstate();
        ctx.translate(30, 50);
        ctx.scale(40, 40);
        ctx.draw_unit_circle();
        ctx.cmd_f();
    }
    {
        auto g = ctx.push_gstate();
        ctx.translate(50, 50);
        ctx.scale(40, 40);
        ctx.draw_unit_circle();
        ctx.cmd_f();
    }
}

int draw_group_doc() {
    // PDF 2.0 spec page 409.
    DocumentMetadata opts;

    opts.default_page_properties.mediabox->x2 = 200;
    opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("Transparency group test").value();
    opts.author = u8string::from_cstr("Test Person").value();
    opts.output_colorspace = CAPY_DEVICE_CS_RGB;
    GenPopper genpop("tr_test.pdf", opts);
    PdfGen &gen = *genpop.g;
    CapyPDF_TransparencyGroupId tgid;
    GraphicsState gs;
    gs.ca = 0.5;
    // Note: does not need to set CA, as layer composition operations treat everything as
    // "non-stroke".
    auto gsid = gen.add_graphics_state(gs).value();
    {
        PdfRectangle bbox{0, 0, 100, 100};
        std::unique_ptr<PdfDrawContext> groupctx{gen.new_transparency_group(bbox)};
        groupctx->cmd_w(10);
        groupctx->cmd_rg(0.9, 0.1, 0.1);
        groupctx->cmd_RG(0.1, 0.9, 0.2);
        groupctx->cmd_re(0, 0, 100, 100);
        groupctx->cmd_b();
        TransparencyGroupExtra ex;
        //        ex.I = false;
        //        ex.K = true;
        tgid = gen.add_transparency_group(*groupctx, &ex).value();
    }
    {
        auto ctxguard = gen.guarded_page_context();
        auto &ctx = ctxguard.ctx;
        ctx.cmd_g(0.5);
        ctx.cmd_re(0, 0, 200, 100);
        ctx.cmd_f();
        auto st = ctx.push_gstate();
        ctx.translate(50, 50);
        ctx.cmd_gs(gsid);
        ctx.cmd_Do(tgid);
    }
    return 0;
}

int draw_transp_doc() {
    // PDF 2.0 spec page 409.
    DocumentMetadata opts;
    const char *icc_out =
        "/home/jpakkane/Downloads/temp/Adobe ICC Profiles (end-user)/CMYK/UncoatedFOGRA29.icc";

    opts.default_page_properties.mediabox->x2 = 300;
    opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("Transparency group test").value();
    opts.author = u8string::from_cstr("Test Person").value();
    opts.output_colorspace = CAPY_DEVICE_CS_CMYK;
    opts.prof.cmyk_profile_file = icc_out;
    {
        GenPopper genpop("group_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        FunctionType2 cmykfunc{{0.0, 1.0},
                               DeviceCMYKColor{0.0, 1.0, 0.0, 0.0},
                               DeviceCMYKColor{1.0, 0.0, 1.0, 0.0},
                               1.0};
        auto funcid = gen.add_function(cmykfunc).value();

        ShadingType2 shade;
        shade.colorspace = CAPY_DEVICE_CS_CMYK;
        shade.x0 = 0;
        shade.y0 = 40;
        shade.x1 = 80;
        shade.y1 = 40;
        shade.function = funcid;
        shade.extend0 = false;
        shade.extend1 = false;

        GraphicsState gs;
        gs.BM = CAPY_BM_MULTIPLY;
        auto gsid = gen.add_graphics_state(gs).value();

        auto shadeid = gen.add_shading(shade).value();
        {
            auto ctxguard = gen.guarded_page_context();
            auto &ctx = ctxguard.ctx;
            ctx.render_pdfdoc_text_builtin("Isolated", CAPY_FONT_HELVETICA, 8, 5, 150);
            ctx.render_pdfdoc_text_builtin("Non-isolated", CAPY_FONT_HELVETICA, 8, 5, 50);
            ctx.render_pdfdoc_text_builtin("Knockout", CAPY_FONT_HELVETICA, 8, 100, 5);
            ctx.render_pdfdoc_text_builtin("Non-knockout", CAPY_FONT_HELVETICA, 8, 200, 5);
            {
                PdfRectangle bbox{0, 0, 80, 80};
                std::unique_ptr<PdfDrawContext> groupctx{gen.new_transparency_group(bbox)};
                draw_circles(*groupctx, gsid);
                TransparencyGroupExtra ex;
                ex.I = false;
                ex.K = true;
                auto tgid = gen.add_transparency_group(*groupctx, &ex).value();
                auto rc = ctx.push_gstate();
                draw_gradient(ctx, shadeid, 80, 20);
                ctx.cmd_Do(tgid);
            }
            {
                PdfRectangle bbox{0, 0, 80, 80};
                std::unique_ptr<PdfDrawContext> groupctx{gen.new_transparency_group(bbox)};
                draw_circles(*groupctx, gsid);
                TransparencyGroupExtra ex;
                ex.I = true;
                ex.K = true;
                auto tgid = gen.add_transparency_group(*groupctx, &ex).value();
                auto rc = ctx.push_gstate();
                draw_gradient(ctx, shadeid, 80, 110);
                ctx.cmd_Do(tgid);
            }
            {
                PdfRectangle bbox{0, 0, 80, 80};
                std::unique_ptr<PdfDrawContext> groupctx{gen.new_transparency_group(bbox)};
                draw_circles(*groupctx, gsid);
                TransparencyGroupExtra ex;
                ex.I = false;
                ex.K = false;
                auto tgid = gen.add_transparency_group(*groupctx, &ex).value();
                auto rc = ctx.push_gstate();
                draw_gradient(ctx, shadeid, 180, 20);
                ctx.cmd_Do(tgid);
            }
            {
                PdfRectangle bbox{0, 0, 80, 80};
                std::unique_ptr<PdfDrawContext> groupctx{gen.new_transparency_group(bbox)};
                draw_circles(*groupctx, gsid);
                TransparencyGroupExtra ex;
                ex.I = true;
                ex.K = false;
                auto tgid = gen.add_transparency_group(*groupctx, &ex).value();
                auto rc = ctx.push_gstate();
                draw_gradient(ctx, shadeid, 180, 110);
                ctx.cmd_Do(tgid);
            }
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if(draw_simple_form() != 0) {
        return 1;
    }
    if(draw_transp_doc() != 0) {
        return 1;
    }
    return draw_group_doc();
}
