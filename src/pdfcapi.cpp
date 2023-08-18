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

#define RETNOERR return (CAPYPDF_EC)ErrorCode::NoError

using namespace capypdf;

namespace {

CAPYPDF_EC conv_err(ErrorCode ec) { return (CAPYPDF_EC)ec; }

template<typename T> CAPYPDF_EC conv_err(const rvoe<T> &rc) {
    return (CAPYPDF_EC)(rc ? ErrorCode::NoError : rc.error());
}

} // namespace

CAPYPDF_EC capy_options_new(CapyPDF_Options **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_Options *>(new PdfGenerationData());
    RETNOERR;
}

CAPYPDF_EC capy_options_destroy(CapyPDF_Options *opt) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<PdfGenerationData *>(opt);
    RETNOERR;
}

CAPYPDF_EC capy_options_set_title(CapyPDF_Options *opt, const char *utf8_title) CAPYPDF_NOEXCEPT {
    auto rc = u8string::from_cstr(utf8_title);
    if(rc) {
        reinterpret_cast<PdfGenerationData *>(opt)->title = std::move(rc.value());
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_options_set_author(CapyPDF_Options *opt,
                                                  const char *utf8_author) CAPYPDF_NOEXCEPT {
    auto rc = u8string::from_cstr(utf8_author);
    if(rc) {
        reinterpret_cast<PdfGenerationData *>(opt)->author = std::move(rc.value());
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_options_set_creator(CapyPDF_Options *opt,
                                                   const char *utf8_creator) CAPYPDF_NOEXCEPT {
    auto rc = u8string::from_cstr(utf8_creator);
    if(rc) {
        reinterpret_cast<PdfGenerationData *>(opt)->creator = std::move(rc.value());
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_page_properties_new(CapyPDF_PageProperties **out_ptr)
    CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_PageProperties *>(new PageProperties);
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_page_properties_destroy(CapyPDF_PageProperties *prop)
    CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<PageProperties *>(prop);
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_page_properties_set_pagebox(CapyPDF_PageProperties *prop,
                                                           CAPYPDF_Page_Box boxtype,
                                                           double x1,
                                                           double y1,
                                                           double x2,
                                                           double y2) CAPYPDF_NOEXCEPT {
    auto props = reinterpret_cast<PageProperties *>(prop);
    switch(boxtype) {
    case CAPY_BOX_MEDIA:
        props->mediabox = PdfRectangle{x1, y1, x2, y2};
        break;
    case CAPY_BOX_CROP:
        props->cropbox = PdfRectangle{x1, y1, x2, y2};
        break;
    case CAPY_BOX_BLEED:
        props->bleedbox = PdfRectangle{x1, y1, x2, y2};
        break;
    case CAPY_BOX_TRIM:
        props->trimbox = PdfRectangle{x1, y1, x2, y2};
        break;
    case CAPY_BOX_ART:
        props->artbox = PdfRectangle{x1, y1, x2, y2};
        break;
    default:
        return (CAPYPDF_EC)ErrorCode::BadEnum;
    }

    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_options_set_device_profile(
    CapyPDF_Options *opt, enum CapyPDF_Colorspace cs, const char *profile_path) CAPYPDF_NOEXCEPT {
    auto opts = reinterpret_cast<PdfGenerationData *>(opt);
    switch(cs) {
    case CAPYPDF_CS_DEVICE_RGB:
        opts->prof.rgb_profile_file = profile_path;
        break;
    case CAPYPDF_CS_DEVICE_GRAY:
        opts->prof.gray_profile_file = profile_path;
        break;
    case CAPYPDF_CS_DEVICE_CMYK:
        opts->prof.cmyk_profile_file = profile_path;
        break;
    }
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_options_set_colorspace(CapyPDF_Options *opt,
                                                      enum CapyPDF_Colorspace cs) CAPYPDF_NOEXCEPT {
    auto opts = reinterpret_cast<PdfGenerationData *>(opt);
    opts->output_colorspace = cs;
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_options_set_output_intent(CapyPDF_Options *opt,
                                                         enum CAPYPDF_Intent_Subtype stype,
                                                         const char *identifier) CAPYPDF_NOEXCEPT {
    CHECK_NULL(identifier);
    auto opts = reinterpret_cast<PdfGenerationData *>(opt);
    opts->subtype = stype;
    opts->intent_condition_identifier = identifier;
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_options_set_default_page_properties(
    CapyPDF_Options *opt, const CapyPDF_PageProperties *prop) CAPYPDF_NOEXCEPT {
    auto opts = reinterpret_cast<PdfGenerationData *>(opt);
    auto props = reinterpret_cast<const PageProperties *>(prop);
    if(!props->mediabox) {
        return conv_err(ErrorCode::MissingMediabox);
    }
    opts->default_page_properties = *props;
    RETNOERR;
}

CAPYPDF_EC capy_generator_new(const char *filename,
                              const CapyPDF_Options *options,
                              CapyPDF_Generator **out_ptr) CAPYPDF_NOEXCEPT {
    CHECK_NULL(filename);
    CHECK_NULL(options);
    CHECK_NULL(out_ptr);
    auto opts = reinterpret_cast<const PdfGenerationData *>(options);
    auto rc = PdfGen::construct(filename, *opts);
    if(rc) {
        *out_ptr = reinterpret_cast<CapyPDF_Generator *>(rc.value().release());
    }
    return conv_err(rc);
}

CAPYPDF_EC capy_generator_add_page(CapyPDF_Generator *g,
                                   CapyPDF_DrawContext *dctx) CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto *ctx = reinterpret_cast<PdfDrawContext *>(dctx);

    auto rc = gen->add_page(*ctx);
    return conv_err(rc);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_generator_embed_jpg(CapyPDF_Generator *g,
                                                   const char *fname,
                                                   CapyPDF_ImageId *iid) CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto rc = gen->embed_jpg(fname);
    return conv_err(rc);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_generator_load_font(CapyPDF_Generator *g,
                                                   const char *fname,
                                                   CapyPDF_FontId *fid) CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto rc = gen->load_font(fname);
    return conv_err(rc);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_generator_load_image(CapyPDF_Generator *g,
                                                    const char *fname,
                                                    CapyPDF_ImageId *iid) CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto rc = gen->load_image(fname);
    if(rc) {
        *iid = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_generator_add_graphics_state(CapyPDF_Generator *g,
                                                            const CapyPDF_GraphicsState *state,
                                                            CapyPDF_GraphicsStateId *gsid)
    CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto *s = reinterpret_cast<const GraphicsState *>(state);
    auto rc = gen->add_graphics_state(*s);
    if(rc) {
        *gsid = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_generator_load_icc_profile(
    CapyPDF_Generator *g, const char *fname, CapyPDF_IccColorSpaceId *iid) CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto rc = gen->load_icc_file(fname);
    if(rc) {
        *iid = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_EC capy_generator_write(CapyPDF_Generator *generator) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(generator);
    auto rc = g->write();
    return conv_err(rc);
}

CAPYPDF_PUBLIC CAPYPDF_EC
capy_generator_add_optional_content_group(CapyPDF_Generator *generator,
                                          const CapyPDF_OptionalContentGroup *ocg,
                                          CapyPDF_OptionalContentGroupId *ocgid) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(generator);
    const auto *group = reinterpret_cast<const OptionalContentGroup *>(ocg);
    auto rc = g->add_optional_content_group(*group);
    if(rc) {
        *ocgid = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_EC capy_generator_destroy(CapyPDF_Generator *generator) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(generator);
    delete g;
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_generator_text_width(CapyPDF_Generator *generator,
                                                    const char *utf8_text,
                                                    CapyPDF_FontId font,
                                                    double pointsize,
                                                    double *width) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(generator);
    auto u8t = u8string::from_cstr(utf8_text);
    if(!u8t) {
        return conv_err(u8t);
    }
    auto rc = g->utf8_text_width(u8t.value(), font, pointsize);
    if(rc) {
        *width = rc.value();
    }
    return conv_err(rc);
}

// Draw Context

CAPYPDF_EC
capy_page_draw_context_new(CapyPDF_Generator *g, CapyPDF_DrawContext **out_ptr) CAPYPDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    *out_ptr = reinterpret_cast<CapyPDF_DrawContext *>(gen->new_page_draw_context());
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_b(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_b());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_B(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_B());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_bstar(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_bstar());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_Bstar(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_Bstar());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_BDC_ocg(
    CapyPDF_DrawContext *ctx, CapyPDF_OptionalContentGroupId ocgid) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_BDC(ocgid));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_c(CapyPDF_DrawContext *ctx,
                                        double x1,
                                        double y1,
                                        double x2,
                                        double y2,
                                        double x3,
                                        double y3) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_c(x1, y1, x2, y2, x3, y3));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_cm(CapyPDF_DrawContext *ctx,
                                         double m1,
                                         double m2,
                                         double m3,
                                         double m4,
                                         double m5,
                                         double m6) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_cm(m1, m2, m3, m4, m5, m6));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_EMC(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_EMC());
}

CAPYPDF_EC capy_dc_cmd_f(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_f());
}

CAPYPDF_EC capy_dc_cmd_fstar(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_fstar());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_G(CapyPDF_DrawContext *ctx, double gray) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_G(gray));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_g(CapyPDF_DrawContext *ctx, double gray) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_g(gray));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_gs(CapyPDF_DrawContext *ctx,
                                         CapyPDF_GraphicsStateId gsid) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_gs(gsid));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_h(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_h());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_i(CapyPDF_DrawContext *ctx,
                                        double flatness) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_i(flatness));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_j(CapyPDF_DrawContext *ctx,
                                        CAPYPDF_Line_Join join_style) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_j(join_style));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_J(CapyPDF_DrawContext *ctx,
                                        CAPYPDF_Line_Cap cap_style) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_J(cap_style));
}

CAPYPDF_PUBLIC CAPYPDF_EC
capy_dc_cmd_k(CapyPDF_DrawContext *ctx, double c, double m, double y, double k) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_k(c, m, y, k));
}

CAPYPDF_PUBLIC CAPYPDF_EC
capy_dc_cmd_K(CapyPDF_DrawContext *ctx, double c, double m, double y, double k) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_K(c, m, y, k));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_l(CapyPDF_DrawContext *ctx,
                                        double x,
                                        double y) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_l(x, y));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_m(CapyPDF_DrawContext *ctx,
                                        double x,
                                        double y) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_m(x, y));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_M(CapyPDF_DrawContext *ctx,
                                        double miterlimit) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_M(miterlimit));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_n(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_n());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_q(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_q());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_Q(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_Q());
}

CAPYPDF_EC
capy_dc_cmd_re(CapyPDF_DrawContext *ctx, double x, double y, double w, double h) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_re(x, y, w, h));
}

CAPYPDF_EC
capy_dc_cmd_RG(CapyPDF_DrawContext *ctx, double r, double g, double b) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_RG(r, g, b));
}

CAPYPDF_EC
capy_dc_cmd_rg(CapyPDF_DrawContext *ctx, double r, double g, double b) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_rg(r, g, b));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_ri(CapyPDF_DrawContext *ctx,
                                         CapyPDF_Rendering_Intent ri) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_ri(ri));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_s(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_s());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_S(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_S());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_v(
    CapyPDF_DrawContext *ctx, double x2, double y2, double x3, double y3) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_v(x2, y2, x3, y3));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_w(CapyPDF_DrawContext *ctx,
                                        double line_width) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_w(line_width));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_W(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_W());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_Wstar(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_Wstar());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_cmd_y(
    CapyPDF_DrawContext *ctx, double x1, double y1, double x3, double y3) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_y(x1, y1, x3, y3));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_set_stroke(CapyPDF_DrawContext *ctx,
                                             CapyPDF_Color *c) CAPYPDF_NOEXCEPT {
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto *color = reinterpret_cast<Color *>(c);
    return conv_err(dc->set_stroke_color(*color));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_set_nonstroke(CapyPDF_DrawContext *ctx,
                                                CapyPDF_Color *c) CAPYPDF_NOEXCEPT {
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto *color = reinterpret_cast<Color *>(c);
    return conv_err(dc->set_nonstroke_color(*color));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_draw_image(CapyPDF_DrawContext *ctx,
                                             CapyPDF_ImageId iid) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->draw_image(iid));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_render_text(CapyPDF_DrawContext *ctx,
                                              const char *text,
                                              CapyPDF_FontId fid,
                                              double point_size,
                                              double x,
                                              double y) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    auto utxt = u8string::from_cstr(text);
    if(!utxt) {
        return conv_err(utxt);
    }
    return conv_err(c->render_text(utxt.value(), fid, point_size, x, y));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_render_text_obj(CapyPDF_DrawContext *ctx,
                                                  CapyPDF_Text *text) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    auto t = reinterpret_cast<PdfText *>(text);
    return conv_err(c->render_text(*t));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_set_page_transition(
    CapyPDF_DrawContext *dc, CapyPDF_Transition *transition) CAPYPDF_NOEXCEPT {
    auto ctx = reinterpret_cast<PdfDrawContext *>(dc);
    auto t = reinterpret_cast<Transition *>(transition);
    auto rc = ctx->set_transition(*t);
    return conv_err(rc);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_set_custom_page_properties(
    CapyPDF_DrawContext *dc, const CapyPDF_PageProperties *custom_properties) {
    CHECK_NULL(custom_properties);
    auto *ctx = reinterpret_cast<PdfDrawContext *>(dc);
    auto *cprop = reinterpret_cast<const PageProperties *>(custom_properties);
    return conv_err(ctx->set_custom_page_properties(*cprop));
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC
capy_dc_add_simple_navigation(CapyPDF_DrawContext *dc,
                              const CapyPDF_OptionalContentGroupId *ocgarray,
                              int32_t array_size,
                              const CapyPDF_Transition *tr) CAPYPDF_NOEXCEPT {
    auto ctx = reinterpret_cast<PdfDrawContext *>(dc);
    std::optional<Transition> transition;
    if(tr) {
        transition = *reinterpret_cast<const Transition *>(tr);
    }
    std::span<const CapyPDF_OptionalContentGroupId> ocgspan(ocgarray, ocgarray + array_size);
    auto rc = ctx->add_simple_navigation(ocgspan, transition);
    return conv_err(rc);
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_dc_text_new(CapyPDF_DrawContext *dc,
                                           CapyPDF_Text **out_ptr) CAPYPDF_NOEXCEPT {
    CHECK_NULL(dc);
    *out_ptr = reinterpret_cast<CapyPDF_Text *>(
        new capypdf::PdfText(reinterpret_cast<PdfDrawContext *>(dc)));
    RETNOERR;
}

CAPYPDF_EC capy_dc_destroy(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<PdfDrawContext *>(ctx);
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_render_text(CapyPDF_Text *text,
                                                const char *utf8_text) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    auto txt = u8string::from_cstr(utf8_text);
    if(!txt) {
        return conv_err(txt);
    }
    return conv_err(t->render_text(txt.value()));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_nonstroke_color(CapyPDF_Text *text,
                                                    const CapyPDF_Color *color) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    const auto *c = reinterpret_cast<const Color *>(color);
    return conv_err(t->nonstroke_color(*c));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_stroke_color(CapyPDF_Text *text,
                                                 const CapyPDF_Color *color) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    const auto *c = reinterpret_cast<const Color *>(color);
    return conv_err(t->stroke_color(*c));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_cmd_Tc(CapyPDF_Text *text, double spacing) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tc(spacing));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_cmd_Td(CapyPDF_Text *text,
                                           double x,
                                           double y) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Td(x, y));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_cmd_Tf(CapyPDF_Text *text,
                                           CapyPDF_FontId font,
                                           double pointsize) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tf(font, pointsize));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_cmd_TL(CapyPDF_Text *text, double leading) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_TL(leading));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_cmd_Tr(CapyPDF_Text *text,
                                           CapyPDF_Text_Mode tmode) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tr(tmode));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_cmd_Tw(CapyPDF_Text *text, double spacing) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tw(spacing));
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_cmd_Tstar(CapyPDF_Text *text) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tstar());
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_text_destroy(CapyPDF_Text *text) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<PdfText *>(text);
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_color_new(CapyPDF_Color **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_Color *>(new capypdf::Color(DeviceRGBColor{0, 0, 0}));
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_color_destroy(CapyPDF_Color *color) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<capypdf::Color *>(color);
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_color_set_rgb(CapyPDF_Color *c, double r, double g, double b)
    CAPYPDF_NOEXCEPT {
    *reinterpret_cast<capypdf::Color *>(c) = DeviceRGBColor{r, g, b};
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_color_set_gray(CapyPDF_Color *c, double v) CAPYPDF_NOEXCEPT {
    *reinterpret_cast<capypdf::Color *>(c) = DeviceGrayColor{v};
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC
capy_color_set_cmyk(CapyPDF_Color *color, double c, double m, double y, double k) CAPYPDF_NOEXCEPT {
    *reinterpret_cast<capypdf::Color *>(color) = DeviceCMYKColor{c, m, y, k};
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_color_set_icc(CapyPDF_Color *color,
                                             CapyPDF_IccColorSpaceId icc_id,
                                             double *values,
                                             int32_t num_values) CAPYPDF_NOEXCEPT {
    ICCColor icc;
    icc.id = icc_id;
    icc.values.assign(values, values + num_values);
    *reinterpret_cast<capypdf::Color *>(color) = std::move(icc);
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_transition_new(CapyPDF_Transition **out_ptr,
                                              CAPYPDF_Transition_Type type,
                                              double duration) CAPYPDF_NOEXCEPT {
    auto pt = new Transition{};
    pt->type = type;
    pt->duration = duration;
    *out_ptr = reinterpret_cast<CapyPDF_Transition *>(pt);
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_transition_destroy(CapyPDF_Transition *transition) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<Transition *>(transition);
    RETNOERR;
}

// Optional Content groups

CAPYPDF_PUBLIC CAPYPDF_EC capy_optional_content_group_new(CapyPDF_OptionalContentGroup **out_ptr,
                                                          const char *name) CAPYPDF_NOEXCEPT {
    // FIXME check for ASCIIness (or even more strict?)
    auto *ocg = new OptionalContentGroup();
    ocg->name = name;
    *out_ptr = reinterpret_cast<CapyPDF_OptionalContentGroup *>(ocg);
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_optional_content_group_destroy(CapyPDF_OptionalContentGroup *ocg)
    CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<OptionalContentGroup *>(ocg);
    RETNOERR;
}

// Graphics state

CAPYPDF_PUBLIC CAPYPDF_EC capy_graphics_state_new(CapyPDF_GraphicsState **out_ptr)
    CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_GraphicsState *>(new GraphicsState);
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_graphics_state_set_CA(CapyPDF_GraphicsState *state,
                                                     double value) CAPYPDF_NOEXCEPT {
    auto *s = reinterpret_cast<GraphicsState *>(state);
    s->CA = LimitDouble{value};
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_graphics_state_set_ca(CapyPDF_GraphicsState *state,
                                                     double value) CAPYPDF_NOEXCEPT {
    auto *s = reinterpret_cast<GraphicsState *>(state);
    s->ca = LimitDouble{value};
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_graphics_state_set_BM(CapyPDF_GraphicsState *state,
                                                     CAPYPDF_Blend_Mode blendmode) CAPYPDF_NOEXCEPT {
    auto *s = reinterpret_cast<GraphicsState *>(state);
    s->BM = blendmode;
    RETNOERR;
}

CAPYPDF_PUBLIC CAPYPDF_EC capy_graphics_state_destroy(CapyPDF_GraphicsState *state)
    CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<GraphicsState *>(state);
    RETNOERR;
}

// Error handling.

const char *capy_error_message(CAPYPDF_EC error_code) CAPYPDF_NOEXCEPT {
    return error_text((ErrorCode)error_code);
}
