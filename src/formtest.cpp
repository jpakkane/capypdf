// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <generator.hpp>

using namespace capypdf::internal;

int draw_simple_form() {
    DocumentProperties opts;

    opts.default_page_properties.mediabox->x2 = opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("Form XObject test").value();
    opts.author = u8string::from_cstr("Test Person").value();
    opts.output_colorspace = CAPY_DEVICE_CS_RGB;
    {
        GenPopper genpop("form_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        CapyPDF_FormXObjectId check_offstate, check_onstate;
        CapyPDF_FormXObjectId push_offstate, push_onstate;
        CapyPDF_FormXObjectId radio_offstate, radio_onstate;
        // Check button widgets.
        {
            auto xobj_h = gen.guarded_form_xobject(PdfRectangle{0, 0, 10, 10});
            auto &xobj = xobj_h.ctx;
            xobj.cmd_BMC("/Tx");
            xobj.cmd_EMC();
            auto rv = gen.add_form_xobject(xobj);
            if(!rv) {
                fprintf(stderr, "%s\n", error_text(rv.error()));
                return 1;
            }
            check_offstate = *rv;
        }
        {
            auto xobj_h = gen.guarded_form_xobject(PdfRectangle{0, 0, 10, 10});
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
            check_onstate = *rv;
        }

        // Push button widgets.
        {
            auto xobj_h = gen.guarded_form_xobject(PdfRectangle{0, 0, 10, 50});
            auto &xobj = xobj_h.ctx;
            xobj.cmd_q();
            xobj.cmd_re(0, 0, 10, 50);
            xobj.cmd_g(0.2);
            xobj.cmd_f();
            xobj.cmd_Q();
            auto rv = gen.add_form_xobject(xobj);
            if(!rv) {
                fprintf(stderr, "%s\n", error_text(rv.error()));
                return 1;
            }
            push_onstate = *rv;
        }
        {
            auto xobj_h = gen.guarded_form_xobject(PdfRectangle{0, 0, 10, 50});
            auto &xobj = xobj_h.ctx;
            xobj.cmd_q();
            xobj.cmd_re(0, 0, 10, 50);
            xobj.cmd_g(0.8);
            xobj.cmd_f();
            xobj.cmd_Q();
            auto rv = gen.add_form_xobject(xobj);
            if(!rv) {
                fprintf(stderr, "%s\n", error_text(rv.error()));
                return 1;
            }
            push_offstate = *rv;
        }

        // Radio button widgets.
        {
            auto xobj_h = gen.guarded_form_xobject(PdfRectangle{0, 0, 10, 10});
            auto &xobj = xobj_h.ctx;
            xobj.cmd_q();
            xobj.cmd_re(0, 0, 10, 10);
            xobj.cmd_G(0);
            xobj.cmd_w(2.0);
            xobj.cmd_S();
            xobj.cmd_Q();
            auto rv = gen.add_form_xobject(xobj);
            if(!rv) {
                fprintf(stderr, "%s\n", error_text(rv.error()));
                return 1;
            }
            radio_offstate = *rv;
        }
        {
            auto xobj_h = gen.guarded_form_xobject(PdfRectangle{0, 0, 10, 10});
            auto &xobj = xobj_h.ctx;
            xobj.cmd_q();
            xobj.cmd_re(0, 0, 10, 10);
            xobj.cmd_G(0);
            xobj.cmd_w(2.0);
            xobj.cmd_S();
            xobj.cmd_re(2.5, 2.5, 5, 5);
            xobj.cmd_g(0);
            xobj.cmd_f();
            xobj.cmd_Q();
            auto rv = gen.add_form_xobject(xobj);
            if(!rv) {
                fprintf(stderr, "%s\n", error_text(rv.error()));
                return 1;
            }
            radio_onstate = *rv;
        }

        auto ctxguard = gen.guarded_page_context();
        auto &ctx = ctxguard.ctx;

        FormField check_field{
            .T = u8string::from_cstr("check1").value(),
            .sub = ButtonField{check_onstate, check_offstate, PdfName::from_cstr("/On").value()}};

        CapyPDF_FormFieldId check_field_id = gen.add_form_field(check_field).value();

        Annotation check_widget =
            Annotation{{}, WidgetAnnotation{check_field_id}, PdfRectangle{110, 180, 120, 190}};
        auto check_annot_id = gen.add_annotation(check_widget).value();
        {
            ctx.cmd_re(110, 180, 10, 10);
            ctx.cmd_S();

            ctx.render_pdfdoc_text_builtin("A checkbox", CAPY_FONT_HELVETICA, 12, 25, 180);
            auto rc = ctx.annotate(check_annot_id);
            if(!rc) {
                fprintf(stderr, "FAIL\n");
                return 1;
            }
        }

        std::vector<u8string> choices{
            u8string::from_cstr("Choice one").value(),
            u8string::from_cstr("Choice two").value(),
            u8string::from_cstr("Choice three").value(),
            u8string::from_cstr("Choice four").value(),
            u8string::from_cstr("Choice five").value(),
        };
        FormField choice_field{.T = u8string::from_cstr("choice1").value(),
                               .sub = ChoiceField{std::move(choices)}};
        auto choice_field_id = gen.add_form_field(choice_field).value();
        Annotation choice_widget =
            Annotation{{}, WidgetAnnotation{choice_field_id}, PdfRectangle{130, 150, 190, 170}};
        auto choice_annot_id = gen.add_annotation(choice_widget).value();
        {
            ctx.render_pdfdoc_text_builtin("A choice widget ->", CAPY_FONT_HELVETICA, 12, 25, 165);
            auto rc = ctx.annotate(choice_annot_id);
            if(!rc) {
                fprintf(stderr, "FAIL\n");
                return 1;
            }
        }

        FormField text_field{
            .T = u8string::from_cstr("text1").value(),
            .V = "The default text contents.",
            .sub = TextField{},
        };
        auto text_field_id = gen.add_form_field(text_field).value();
        Annotation text_widget =
            Annotation{{}, WidgetAnnotation{text_field_id}, PdfRectangle{20, 90, 110, 110}};
        auto text_annot_id = gen.add_annotation(text_widget).value();
        {
            ctx.render_pdfdoc_text_builtin("A text widget", CAPY_FONT_HELVETICA, 12, 25, 115);
            auto rc = ctx.annotate(text_annot_id);
            if(!rc) {
                fprintf(stderr, "FAIL\n");
                return 1;
            }
        }

        FormField push_field{
            .T = u8string::from_cstr("push1").value(),
            .Ff = CAPY_FFIELD_PUSHBUTTON,
            .sub = ButtonField{push_onstate, push_offstate},
        };

        CapyPDF_FormFieldId push_field_id = gen.add_form_field(push_field).value();

        Annotation push_widget =
            Annotation{{}, WidgetAnnotation{push_field_id}, PdfRectangle{20, 60, 70, 70}};
        auto push_annot_id = gen.add_annotation(push_widget).value();
        {
            ctx.render_pdfdoc_text_builtin("A push button", CAPY_FONT_HELVETICA, 12, 25, 75);
            auto rc = ctx.annotate(push_annot_id);
            if(!rc) {
                fprintf(stderr, "FAIL\n");
                return 1;
            }
        }

        FormField top_radio_field{
            .T = u8string::from_cstr("Radio choice").value(),
            .Ff = CAPY_FFIELD_RADIO,
            .V = "/state1",
            .sub = ButtonField{push_onstate, push_offstate},
        };
        auto top_radio_field_id = gen.add_form_field(top_radio_field).value();
        {
            ctx.render_pdfdoc_text_builtin("Radio buttons", CAPY_FONT_HELVETICA, 12, 25, 40);
            Annotation button1_a{
                {},
                WidgetAnnotation{top_radio_field_id,
                                 ButtonStateInfo{radio_onstate,
                                                 radio_offstate,
                                                 PdfName::from_cstr("/state1").value()}},
                PdfRectangle(20, 20, 30, 30)};
            auto b1_a_id = gen.add_annotation(button1_a).value();
            ctx.annotate(b1_a_id);

            Annotation button2_a{
                {},
                WidgetAnnotation{top_radio_field_id,
                                 ButtonStateInfo{radio_onstate,
                                                 radio_offstate,
                                                 PdfName::from_cstr("/state2").value()}},
                PdfRectangle(40, 20, 50, 30)};
            auto b2_a_id = gen.add_annotation(button2_a).value();
            ctx.annotate(b2_a_id);

            Annotation button3_a{
                {},
                WidgetAnnotation{top_radio_field_id,
                                 ButtonStateInfo{radio_onstate,
                                                 radio_offstate,
                                                 PdfName::from_cstr("/state3").value()}},
                PdfRectangle(60, 20, 70, 30)};
            auto b3_a_id = gen.add_annotation(button3_a).value();
            ctx.annotate(b3_a_id);
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
    DocumentProperties opts;

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
        tgid = gen.add_transparency_group(*groupctx).value();
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
    DocumentProperties opts;
    const char *icc_out = "/usr/share/color/icc/colord/FOGRA29L_uncoated.icc";

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
                TransparencyGroupProperties ex;
                ex.I = false;
                ex.K = true;
                groupctx->set_transparency_properties(ex);
                auto tgid = gen.add_transparency_group(*groupctx).value();
                auto rc = ctx.push_gstate();
                draw_gradient(ctx, shadeid, 80, 20);
                ctx.cmd_Do(tgid);
            }
            {
                PdfRectangle bbox{0, 0, 80, 80};
                std::unique_ptr<PdfDrawContext> groupctx{gen.new_transparency_group(bbox)};
                draw_circles(*groupctx, gsid);
                TransparencyGroupProperties ex;
                ex.I = true;
                ex.K = true;
                groupctx->set_transparency_properties(ex);
                auto tgid = gen.add_transparency_group(*groupctx).value();
                auto rc = ctx.push_gstate();
                draw_gradient(ctx, shadeid, 80, 110);
                ctx.cmd_Do(tgid);
            }
            {
                PdfRectangle bbox{0, 0, 80, 80};
                std::unique_ptr<PdfDrawContext> groupctx{gen.new_transparency_group(bbox)};
                draw_circles(*groupctx, gsid);
                TransparencyGroupProperties ex;
                ex.I = false;
                ex.K = false;
                groupctx->set_transparency_properties(ex);
                auto tgid = gen.add_transparency_group(*groupctx).value();
                auto rc = ctx.push_gstate();
                draw_gradient(ctx, shadeid, 180, 20);
                ctx.cmd_Do(tgid);
            }
            {
                PdfRectangle bbox{0, 0, 80, 80};
                std::unique_ptr<PdfDrawContext> groupctx{gen.new_transparency_group(bbox)};
                draw_circles(*groupctx, gsid);
                TransparencyGroupProperties ex;
                ex.I = true;
                ex.K = false;
                groupctx->set_transparency_properties(ex);
                auto tgid = gen.add_transparency_group(*groupctx).value();
                auto rc = ctx.push_gstate();
                draw_gradient(ctx, shadeid, 180, 110);
                ctx.cmd_Do(tgid);
            }
        }
    }
    return 0;
}

int main(int /*argc*/, char ** /*argv*/) {
    if(draw_simple_form() != 0) {
        return 1;
    }
    if(draw_transp_doc() != 0) {
        return 1;
    }
    return draw_group_doc();
}
