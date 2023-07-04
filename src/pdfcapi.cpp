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

#include <capypdf.h>
#include <cstring>
#include <pdfgen.hpp>
#include <pdfdrawcontext.hpp>
#include <errorhandling.hpp>

using namespace capypdf;

CAPYPDF_EC capy_options_new(CapyPdF_Options **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPdF_Options *>(new PdfGenerationData());
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_EC capy_options_destroy(CapyPdF_Options *opt) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<PdfGenerationData *>(opt);
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_EC capy_options_set_title(CapyPdF_Options *opt, const char *utf8_title) CAPYPDF_NOEXCEPT {
    reinterpret_cast<PdfGenerationData *>(opt)->title = utf8_title;
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_options_set_author(CapyPdF_Options *opt,
                                                  const char *utf8_author) CAPYPDF_NOEXCEPT {
    reinterpret_cast<PdfGenerationData *>(opt)->author = utf8_author;
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_options_set_pagebox(CapyPdF_Options *opt,
                                                   CAPYPDF_Page_Box boxtype,
                                                   double x1,
                                                   double y1,
                                                   double x2,
                                                   double y2) CAPYPDF_NOEXCEPT {
    auto opts = reinterpret_cast<PdfGenerationData *>(opt);
    switch(boxtype) {
    case CAPY_BOX_MEDIA:
        opts->mediabox.x1 = x1;
        opts->mediabox.y1 = y1;
        opts->mediabox.x2 = x2;
        opts->mediabox.y2 = y2;
        break;
    case CAPY_BOX_CROP:
        opts->cropbox = PdfRectangle{x1, y1, x2, y2};
        break;
    case CAPY_BOX_BLEED:
        opts->bleedbox = PdfRectangle{x1, y1, x2, y2};
        break;
    case CAPY_BOX_TRIM:
        opts->trimbox = PdfRectangle{x1, y1, x2, y2};
        break;
    case CAPY_BOX_ART:
        opts->artbox = PdfRectangle{x1, y1, x2, y2};
        break;
    default:
        return (CAPYPDF_EC)ErrorCode::BadEnum;
    }

    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_options_set_colorspace(CapyPdF_Options *opt,
                                                      enum CapyPdF_Colorspace cs) CAPYPDF_NOEXCEPT {
    auto opts = reinterpret_cast<PdfGenerationData *>(opt);
    opts->output_colorspace = cs;
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_EC capy_generator_new(const char *filename,
                              const CapyPdF_Options *options,
                              CapyPdF_Generator **out_ptr) CAPYPDF_NOEXCEPT {
    CHECK_NULL(filename);
    CHECK_NULL(options);
    CHECK_NULL(out_ptr);
    auto opts = reinterpret_cast<const PdfGenerationData *>(options);
    auto genconstruct = PdfGen::construct(filename, *opts);
    if(!genconstruct) {
        return (CAPYPDF_EC)genconstruct.error();
    }
    *out_ptr = reinterpret_cast<CapyPdF_Generator *>(genconstruct.value().release());
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_EC capy_generator_add_page(CapyPdF_Generator *g,
                                   CapyPdF_DrawContext *dctx) CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto *ctx = reinterpret_cast<PdfDrawContext *>(dctx);

    auto rc = gen->add_page(*ctx);
    if(rc) {
        return (CAPYPDF_EC)ErrorCode::NoError;
    }
    return (CAPYPDF_EC)rc.error();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_generator_embed_jpg(CapyPdF_Generator *g,
                                                   const char *fname,
                                                   CapyPdF_ImageId *iid) CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto res = gen->embed_jpg(fname);
    if(res) {
        *iid = res.value();
        return (CAPYPDF_EC)ErrorCode::NoError;
    }
    return (CAPYPDF_EC)res.error();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_generator_load_font(CapyPdF_Generator *g,
                                                   const char *fname,
                                                   CapyPdF_FontId *fid) CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto rc = gen->load_font(fname);
    if(rc) {
        *fid = rc.value();
        return (CAPYPDF_EC)ErrorCode::NoError;
    }
    return (CAPYPDF_EC)rc.error();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_generator_load_image(CapyPdF_Generator *g,
                                                    const char *fname,
                                                    CapyPdF_ImageId *iid) CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto load_result = gen->load_image(fname);
    if(load_result) {
        *iid = load_result.value();
        return (CAPYPDF_EC)ErrorCode::NoError;
    }
    return (CAPYPDF_EC)load_result.error();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_generator_load_icc_profile(
    CapyPdF_Generator *g, const char *fname, CapyPdF_IccColorSpaceId *iid) CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto res = gen->load_icc_file(fname);
    if(res) {
        *iid = res.value();
        return (CAPYPDF_EC)ErrorCode::NoError;
    }
    return (CAPYPDF_EC)res.error();
}

CAPYPDF_EC capy_generator_write(CapyPdF_Generator *generator) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(generator);
    auto rc = g->write();
    if(rc) {
        return (CAPYPDF_EC)ErrorCode::NoError;
    }
    return (CAPYPDF_EC)rc.error();
}

CAPYPDF_EC capy_generator_destroy(CapyPdF_Generator *generator) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(generator);
    delete g;
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_generator_utf8_text_width(CapyPdF_Generator *generator,
                                                         const char *utf8_text,
                                                         CapyPdF_FontId font,
                                                         double pointsize,
                                                         double *width) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(generator);
    auto rc = g->utf8_text_width(utf8_text, font, pointsize);
    if(rc) {
        *width = rc.value();
        return (CAPYPDF_EC)ErrorCode::NoError;
    }
    return (CAPYPDF_EC)rc.error();
}

// Draw Context

CAPYPDF_EC capy_page_draw_context_new(CapyPdF_Generator *g,
                                      CapyPdF_DrawContext **out_ptr) CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    *out_ptr = reinterpret_cast<CapyPdF_DrawContext *>(gen->new_page_draw_context());
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_b(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_b();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_B(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_B();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_bstar(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_bstar();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_Bstar(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_Bstar();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_c(CapyPdF_DrawContext *ctx,
                                        double x1,
                                        double y1,
                                        double x2,
                                        double y2,
                                        double x3,
                                        double y3) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_c(x1, y1, x2, y2, x3, y3);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_cm(CapyPdF_DrawContext *ctx,
                                         double m1,
                                         double m2,
                                         double m3,
                                         double m4,
                                         double m5,
                                         double m6) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_cm(m1, m2, m3, m4, m5, m6);
}

CAPYPDF_EC capy_dc_cmd_f(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_f();
}

CAPYPDF_EC capy_dc_cmd_fstar(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_fstar();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_G(CapyPdF_DrawContext *ctx, double gray) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_G(gray);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_g(CapyPdF_DrawContext *ctx, double gray) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_g(gray);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_h(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_h();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_i(CapyPdF_DrawContext *ctx,
                                        double flatness) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_i(flatness);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_j(CapyPdF_DrawContext *ctx,
                                        CAPYPDF_Line_Join join_style) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_j(join_style);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_J(CapyPdF_DrawContext *ctx,
                                        CAPYPDF_Line_Cap cap_style) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_J(cap_style);
}

CAPYPDF_PUBLIC CAPYPDF_EC
capy_dc_cmd_k(CapyPdF_DrawContext *ctx, double c, double m, double y, double k) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)dc->cmd_k(c, m, y, k);
}

CAPYPDF_PUBLIC CAPYPDF_EC
capy_dc_cmd_K(CapyPdF_DrawContext *ctx, double c, double m, double y, double k) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)dc->cmd_K(c, m, y, k);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_l(CapyPdF_DrawContext *ctx,
                                        double x,
                                        double y) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_l(x, y);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_m(CapyPdF_DrawContext *ctx,
                                        double x,
                                        double y) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_m(x, y);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_M(CapyPdF_DrawContext *ctx,
                                        double miterlimit) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_M(miterlimit);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_n(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_n();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_q(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_q();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_Q(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_Q();
}

CAPYPDF_EC
capy_dc_cmd_re(CapyPdF_DrawContext *ctx, double x, double y, double w, double h) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_re(x, y, w, h);
}

CAPYPDF_EC
capy_dc_cmd_RG(CapyPdF_DrawContext *ctx, double r, double g, double b) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    DeviceRGBColor rgb{r, g, b};
    c->set_stroke_color(rgb);
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_EC
capy_dc_cmd_rg(CapyPdF_DrawContext *ctx, double r, double g, double b) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    DeviceRGBColor rgb{r, g, b};
    c->set_nonstroke_color(rgb);
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_ri(CapyPdF_DrawContext *ctx,
                                         CapyPdF_Rendering_Intent ri) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_ri(ri);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_s(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_s();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_S(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_S();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_v(
    CapyPdF_DrawContext *ctx, double x2, double y2, double x3, double y3) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_v(x2, y2, x3, y3);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_w(CapyPdF_DrawContext *ctx,
                                        double line_width) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_w(line_width);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_W(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_W();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_Wstar(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_Wstar();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_y(
    CapyPdF_DrawContext *ctx, double x1, double y1, double x3, double y3) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->cmd_y(x1, y1, x3, y3);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_set_icc_stroke(CapyPdF_DrawContext *ctx,
                                                 CapyPdF_IccColorSpaceId icc_id,
                                                 double *values,
                                                 int32_t num_values) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->set_stroke_color(icc_id, values, num_values);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_set_icc_nonstroke(CapyPdF_DrawContext *ctx,
                                                    CapyPdF_IccColorSpaceId icc_id,
                                                    double *values,
                                                    int32_t num_values) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->set_nonstroke_color(icc_id, values, num_values);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_draw_image(CapyPdF_DrawContext *ctx,
                                             CapyPdF_ImageId iid) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)c->draw_image(iid);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_render_utf8_text(CapyPdF_DrawContext *ctx,
                                                   const char *text,
                                                   CapyPdF_FontId fid,
                                                   double point_size,
                                                   double x,
                                                   double y) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    c->render_utf8_text(text, fid, point_size, x, y);
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_render_text_obj(CapyPdF_DrawContext *ctx,
                                                  CapyPdF_Text *text) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    auto t = reinterpret_cast<PdfText *>(text);
    return (CAPYPDF_EC)c->render_text(*t);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_set_page_transition(
    CapyPdF_DrawContext *dc, CAPYPDF_Transition *transition) CAPYPDF_NOEXCEPT {
    auto ctx = reinterpret_cast<PdfDrawContext *>(dc);
    auto t = reinterpret_cast<Transition *>(transition);
    auto rc = ctx->set_transition(*t);
    return (CAPYPDF_EC)(rc ? ErrorCode::NoError : rc.error());
}

CAPYPDF_EC capy_dc_destroy(CapyPdF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<PdfDrawContext *>(ctx);
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_new(CapyPdF_Text **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPdF_Text *>(new capypdf::PdfText());
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_render_utf8_text(CapyPdF_Text *text,
                                                     const char *utf8_text) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return (CAPYPDF_EC)t->render_text(std::string_view(utf8_text, strlen(utf8_text)));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_nonstroke_color(CapyPdF_Text *text,
                                                    const CapyPdF_Color *color) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    const auto *c = reinterpret_cast<const Color *>(color);
    return (CAPYPDF_EC)t->nonstroke_color(*c);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_cmd_Tc(CapyPdF_Text *text, double spacing) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return (CAPYPDF_EC)t->cmd_Tc(spacing);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_cmd_Td(CapyPdF_Text *text,
                                           double x,
                                           double y) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return (CAPYPDF_EC)t->cmd_Td(x, y);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_cmd_Tf(CapyPdF_Text *text,
                                           CapyPdF_FontId font,
                                           double pointsize) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return (CAPYPDF_EC)t->cmd_Tf(font, pointsize);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_cmd_TL(CapyPdF_Text *text, double leading) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return (CAPYPDF_EC)t->cmd_TL(leading);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_cmd_Tstar(CapyPdF_Text *text) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return (CAPYPDF_EC)t->cmd_Tstar();
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_destroy(CapyPdF_Text *text) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<PdfText *>(text);
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_color_new(CapyPdF_Color **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPdF_Color *>(new capypdf::Color(DeviceRGBColor{0, 0, 0}));
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_color_destroy(CapyPdF_Color *color) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<capypdf::Color *>(color);
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_color_set_rgb(CapyPdF_Color *c, double r, double g, double b)
    CAPYPDF_NOEXCEPT {
    *reinterpret_cast<capypdf::Color *>(c) = DeviceRGBColor{r, g, b};
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_color_set_gray(CapyPdF_Color *c, double v) CAPYPDF_NOEXCEPT {
    *reinterpret_cast<capypdf::Color *>(c) = DeviceGrayColor{v};
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_transition_new(CAPYPDF_Transition **out_ptr,
                                              CAPYPDF_Transition_Type type,
                                              double duration) CAPYPDF_NOEXCEPT {
    auto pt = new Transition{};
    pt->type = type;
    pt->duration = duration;
    *out_ptr = reinterpret_cast<CAPYPDF_Transition *>(pt);
    return (CAPYPDF_EC)ErrorCode::NoError;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_transition_destroy(CAPYPDF_Transition *transition) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<Transition *>(transition);
    return (CAPYPDF_EC)ErrorCode::NoError;
}

const char *capy_error_message(CAPYPDF_EC error_code) CAPYPDF_NOEXCEPT {
    return error_text((ErrorCode)error_code);
}
