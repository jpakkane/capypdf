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

#include <a4pdf.h>
#include <cstring>
#include <pdfgen.hpp>
#include <pdfdrawcontext.hpp>
#include <errorhandling.hpp>

using namespace A4PDF;

A4PDF_EC a4pdf_options_new(A4PDF_Options **out_ptr) A4PDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<A4PDF_Options *>(new PdfGenerationData());
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_EC a4pdf_options_destroy(A4PDF_Options *opt) A4PDF_NOEXCEPT {
    delete reinterpret_cast<PdfGenerationData *>(opt);
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_EC a4pdf_options_set_title(A4PDF_Options *opt, const char *utf8_title) A4PDF_NOEXCEPT {
    reinterpret_cast<PdfGenerationData *>(opt)->title = utf8_title;
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_PUBLIC A4PDF_EC a4pdf_options_set_author(A4PDF_Options *opt,
                                               const char *utf8_author) A4PDF_NOEXCEPT {
    reinterpret_cast<PdfGenerationData *>(opt)->author = utf8_author;
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_PUBLIC A4PDF_EC a4pdf_options_set_mediabox(
    A4PDF_Options *opt, double x, double y, double w, double h) A4PDF_NOEXCEPT {
    auto opts = reinterpret_cast<PdfGenerationData *>(opt);
    opts->mediabox.x = x;
    opts->mediabox.y = y;
    opts->mediabox.w = w;
    opts->mediabox.h = h;
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_PUBLIC A4PDF_EC a4pdf_options_set_colorspace(A4PDF_Options *opt,
                                                   enum A4PDF_Colorspace cs) A4PDF_NOEXCEPT {
    auto opts = reinterpret_cast<PdfGenerationData *>(opt);
    opts->output_colorspace = cs;
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_EC a4pdf_generator_new(const char *filename,
                             const A4PDF_Options *options,
                             A4PDF_Generator **out_ptr) A4PDF_NOEXCEPT {
    CHECK_NULL(filename);
    CHECK_NULL(options);
    CHECK_NULL(out_ptr);
    auto opts = reinterpret_cast<const PdfGenerationData *>(options);
    auto genconstruct = PdfGen::construct(filename, *opts);
    if(!genconstruct) {
        return (A4PDF_EC)genconstruct.error();
    }
    *out_ptr = reinterpret_cast<A4PDF_Generator *>(genconstruct.value().release());
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_EC a4pdf_generator_add_page(A4PDF_Generator *g, A4PDF_DrawContext *dctx) A4PDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto *ctx = reinterpret_cast<PdfDrawContext *>(dctx);

    auto rc = gen->add_page(*ctx);
    if(rc) {
        return (A4PDF_EC)ErrorCode::NoError;
    }
    return (A4PDF_EC)rc.error();
}

A4PDF_PUBLIC A4PDF_EC a4pdf_generator_embed_jpg(A4PDF_Generator *g,
                                                const char *fname,
                                                A4PDF_ImageId *iid) A4PDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto res = gen->embed_jpg(fname);
    if(res) {
        *iid = res.value();
        return (A4PDF_EC)ErrorCode::NoError;
    }
    return (A4PDF_EC)res.error();
}

A4PDF_PUBLIC A4PDF_EC a4pdf_generator_load_font(A4PDF_Generator *g,
                                                const char *fname,
                                                A4PDF_FontId *fid) A4PDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto rc = gen->load_font(fname);
    if(rc) {
        *fid = rc.value();
        return (A4PDF_EC)ErrorCode::NoError;
    }
    return (A4PDF_EC)rc.error();
}

A4PDF_PUBLIC A4PDF_EC a4pdf_generator_load_image(A4PDF_Generator *g,
                                                 const char *fname,
                                                 A4PDF_ImageId *iid) A4PDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto load_result = gen->load_image(fname);
    if(load_result) {
        *iid = load_result.value();
        return (A4PDF_EC)ErrorCode::NoError;
    }
    return (A4PDF_EC)load_result.error();
}

A4PDF_PUBLIC A4PDF_EC a4pdf_generator_load_icc_profile(A4PDF_Generator *g,
                                                       const char *fname,
                                                       A4PDF_IccColorSpaceId *iid) A4PDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    auto res = gen->load_icc_file(fname);
    if(res) {
        *iid = res.value();
        return (A4PDF_EC)ErrorCode::NoError;
    }
    return (A4PDF_EC)res.error();
}

A4PDF_EC a4pdf_generator_write(A4PDF_Generator *generator) A4PDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(generator);
    return (A4PDF_EC)g->write();
}

A4PDF_EC a4pdf_generator_destroy(A4PDF_Generator *generator) A4PDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(generator);
    auto rc = (A4PDF_EC)ErrorCode::NoError;
    delete g;
    return rc;
}

A4PDF_PUBLIC A4PDF_EC a4pdf_generator_utf8_text_width(A4PDF_Generator *generator,
                                                      const char *utf8_text,
                                                      A4PDF_FontId font,
                                                      double pointsize,
                                                      double *width) A4PDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(generator);
    auto rc = g->utf8_text_width(utf8_text, font, pointsize);
    if(rc) {
        *width = rc.value();
        return (A4PDF_EC)ErrorCode::NoError;
    }
    return (A4PDF_EC)rc.error();
}

// Draw Context

A4PDF_EC a4pdf_page_draw_context_new(A4PDF_Generator *g,
                                     A4PDF_DrawContext **out_ptr) A4PDF_NOEXCEPT {
    auto *gen = reinterpret_cast<PdfGen *>(g);
    *out_ptr = reinterpret_cast<A4PDF_DrawContext *>(gen->new_page_draw_context());
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_B(A4PDF_DrawContext *ctx) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_B();
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_Bstar(A4PDF_DrawContext *ctx) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_Bstar();
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_c(A4PDF_DrawContext *ctx,
                                     double x1,
                                     double y1,
                                     double x2,
                                     double y2,
                                     double x3,
                                     double y3) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_c(x1, y1, x2, y2, x3, y3);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_cm(A4PDF_DrawContext *ctx,
                                      double m1,
                                      double m2,
                                      double m3,
                                      double m4,
                                      double m5,
                                      double m6) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_cm(m1, m2, m3, m4, m5, m6);
}

A4PDF_EC a4pdf_dc_cmd_f(A4PDF_DrawContext *ctx) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_f();
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_S(A4PDF_DrawContext *ctx) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_S();
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_h(A4PDF_DrawContext *ctx) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_h();
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_j(A4PDF_DrawContext *ctx,
                                     A4PDF_Line_Join join_style) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_j(join_style);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_J(A4PDF_DrawContext *ctx,
                                     A4PDF_Line_Cap cap_style) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_J(cap_style);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_k(A4PDF_DrawContext *ctx, double c, double m, double y, double k)
    A4PDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)dc->cmd_k(c, m, y, k);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_K(A4PDF_DrawContext *ctx, double c, double m, double y, double k)
    A4PDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)dc->cmd_K(c, m, y, k);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_l(A4PDF_DrawContext *ctx, double x, double y) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_l(x, y);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_m(A4PDF_DrawContext *ctx, double x, double y) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_m(x, y);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_q(A4PDF_DrawContext *ctx) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_q();
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_Q(A4PDF_DrawContext *ctx) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_Q();
}

A4PDF_EC
a4pdf_dc_cmd_re(A4PDF_DrawContext *ctx, double x, double y, double w, double h) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_re(x, y, w, h);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_cmd_w(A4PDF_DrawContext *ctx, double line_width) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->cmd_w(line_width);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_set_icc_stroke(A4PDF_DrawContext *ctx,
                                              A4PDF_IccColorSpaceId icc_id,
                                              double *values,
                                              int32_t num_values) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->set_stroke_color(icc_id, values, num_values);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_set_icc_nonstroke(A4PDF_DrawContext *ctx,
                                                 A4PDF_IccColorSpaceId icc_id,
                                                 double *values,
                                                 int32_t num_values) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->set_nonstroke_color(icc_id, values, num_values);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_draw_image(A4PDF_DrawContext *ctx,
                                          A4PDF_ImageId iid) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)c->draw_image(iid);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_render_utf8_text(A4PDF_DrawContext *ctx,
                                                const char *text,
                                                A4PDF_FontId fid,
                                                double point_size,
                                                double x,
                                                double y) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    c->render_utf8_text(text, fid, point_size, x, y);
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_EC
a4pdf_dc_cmd_RG(A4PDF_DrawContext *ctx, double r, double g, double b) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    DeviceRGBColor rgb{r, g, b};
    c->set_stroke_color(rgb);
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_EC
a4pdf_dc_cmd_rg(A4PDF_DrawContext *ctx, double r, double g, double b) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    DeviceRGBColor rgb{r, g, b};
    c->set_nonstroke_color(rgb);
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_PUBLIC A4PDF_EC a4pdf_dc_render_text_obj(A4PDF_DrawContext *ctx,
                                               A4PDF_Text *text) A4PDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    auto t = reinterpret_cast<PdfText *>(text);
    return (A4PDF_EC)c->render_text(*t);
}

A4PDF_EC a4pdf_dc_destroy(A4PDF_DrawContext *ctx) A4PDF_NOEXCEPT {
    delete reinterpret_cast<PdfDrawContext *>(ctx);
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_PUBLIC A4PDF_EC a4pdf_text_new(A4PDF_Text **out_ptr) A4PDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<A4PDF_Text *>(new A4PDF::PdfText());
    return (A4PDF_EC)ErrorCode::NoError;
}

A4PDF_PUBLIC A4PDF_EC a4pdf_text_render_utf8_text(A4PDF_Text *text,
                                                  const char *utf8_text) A4PDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return (A4PDF_EC)t->render_text(std::string_view(utf8_text, strlen(utf8_text)));
}

A4PDF_PUBLIC A4PDF_EC a4pdf_text_cmd_Tc(A4PDF_Text *text, double spacing) A4PDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return (A4PDF_EC)t->cmd_Tc(spacing);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_text_cmd_Td(A4PDF_Text *text, double x, double y) A4PDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return (A4PDF_EC)t->cmd_Td(x, y);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_text_cmd_Tf(A4PDF_Text *text,
                                        A4PDF_FontId font,
                                        double pointsize) A4PDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return (A4PDF_EC)t->cmd_Tf(font, pointsize);
}

A4PDF_PUBLIC A4PDF_EC a4pdf_text_destroy(A4PDF_Text *text) A4PDF_NOEXCEPT {
    delete reinterpret_cast<PdfText *>(text);
    return (A4PDF_EC)ErrorCode::NoError;
}

const char *a4pdf_error_message(A4PDF_EC error_code) A4PDF_NOEXCEPT {
    return error_text((ErrorCode)error_code);
}
