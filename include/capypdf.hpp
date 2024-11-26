// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#pragma once

// The functionality in this header is neither ABI nor API stable.
// If you need that, use the plain C header.

#include <capypdf.h>
#include <memory>
#include <stdexcept>

#define CAPY_CPP_CHECK(funccall)                                                                   \
    {                                                                                              \
        auto rc = funccall;                                                                        \
        if(rc != 0) {                                                                              \
            throw PdfException(capy_error_message(rc));                                            \
        }                                                                                          \
    }

namespace capypdf {

class PdfException : public std::runtime_error {
public:
    PdfException(const char *msg) : std::runtime_error(msg) {}
};

struct CapyCTypeDeleter {
    template<typename T> void operator()(T *cobj) {
        int32_t rc;
        if constexpr(std::is_same_v<T, CapyPDF_DocumentMetadata>) {
            rc = capy_doc_md_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_PageProperties>) {
            rc = capy_page_properties_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_TextSequence>) {
            rc = capy_text_sequence_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_Color>) {
            rc = capy_color_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_GraphicsState>) {
            rc = capy_graphics_state_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_DrawContext>) {
            rc = capy_dc_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_RasterImage>) {
            rc = capy_raster_image_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_RasterImageBuilder>) {
            rc = capy_raster_image_builder_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_TransparencyGroupProperties>) {
            rc = capy_transparency_group_properties_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_Function>) {
            rc = capy_function_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_Shading>) {
            rc = capy_shading_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_ShadingPattern>) {
            rc = capy_shading_pattern_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_Generator>) {
            rc = capy_generator_destroy(cobj);
        } else {
            static_assert(std::is_same_v<T, CapyPDF_DocumentMetadata>, "Unknown C object type.");
        }
        (void)rc; // Not much we can do about this because destructors should not fail or throw.
    }
};

template<typename T> class CapyC {
protected:
    operator T *() { return _d.get(); }
    operator const T *() const { return _d.get(); }

    std::unique_ptr<T, CapyCTypeDeleter> _d;
};

class DocumentMetadata : public CapyC<CapyPDF_DocumentMetadata> {
public:
    friend class Generator;

    DocumentMetadata() {
        CapyPDF_DocumentMetadata *md;
        CAPY_CPP_CHECK(capy_doc_md_new(&md));
        _d.reset(md);
    }
};
static_assert(sizeof(DocumentMetadata) == sizeof(void *));

class TransparencyGroupProperties : public CapyC<CapyPDF_TransparencyGroupProperties> {
public:
    friend class PageProperties;
    friend class DrawContext;

    TransparencyGroupProperties() {
        CapyPDF_TransparencyGroupProperties *tge;
        CAPY_CPP_CHECK(capy_transparency_group_properties_new(&tge));
        _d.reset(tge);
    }

    void set_CS(CapyPDF_DeviceColorspace cs) {
        CAPY_CPP_CHECK(capy_transparency_group_properties_set_CS(*this, cs));
    }

    void set_I(bool I) { CAPY_CPP_CHECK(capy_transparency_group_properties_set_I(*this, I)); }

    void set_K(bool K) { CAPY_CPP_CHECK(capy_transparency_group_properties_set_K(*this, K)); }
};

class PageProperties : public CapyC<CapyPDF_PageProperties> {
public:
    friend class DrawContext;

    PageProperties() {
        CapyPDF_PageProperties *prop;
        CAPY_CPP_CHECK(capy_page_properties_new(&prop));
        _d.reset(prop);
    }

    void set_pagebox(CapyPDF_Page_Box boxtype, double x1, double y1, double x2, double y2) {
        CAPY_CPP_CHECK(capy_page_properties_set_pagebox(*this, boxtype, x1, y1, x2, y2));
    }

    void set_transparency_group_properties(TransparencyGroupProperties &trprop) {
        CAPY_CPP_CHECK(capy_page_properties_set_transparency_group_properties(*this, trprop));
    }
};

class TextSequence : public CapyC<CapyPDF_TextSequence> {
public:
    TextSequence() {
        CapyPDF_TextSequence *ts;
        CAPY_CPP_CHECK(capy_text_sequence_new(&ts));
        _d.reset(ts);
    }

    void append_codepoint(uint32_t codepoint) {
        CAPY_CPP_CHECK(capy_text_sequence_append_codepoint(*this, codepoint));
    }
};

class Color : public CapyC<CapyPDF_Color> {
public:
    friend class DrawContext;
    friend class Type2Function;
    friend class Type4Shading;
    friend class Type6Shading;

    Color() {
        CapyPDF_Color *c;
        CAPY_CPP_CHECK(capy_color_new(&c));
        _d.reset(c);
    }

    void set_rgb(double r, double g, double b) {
        CAPY_CPP_CHECK(capy_color_set_rgb(*this, r, g, b));
    }
    void set_cmyk(double c, double m, double y, double k) {
        CAPY_CPP_CHECK(capy_color_set_cmyk(*this, c, m, y, k));
    }
    void set_gray(double g) { CAPY_CPP_CHECK(capy_color_set_gray(*this, g)); }
    void set_icc(CapyPDF_IccColorSpaceId icc_id, double *values, int32_t num_values) {
        CAPY_CPP_CHECK(capy_color_set_icc(*this, icc_id, values, num_values));
    }
    void set_pattern(CapyPDF_PatternId pattern_id) {
        CAPY_CPP_CHECK(capy_color_set_pattern(*this, pattern_id));
    }
};

class GraphicsState : public CapyC<CapyPDF_GraphicsState> {
public:
    friend class Generator;

    GraphicsState() {
        CapyPDF_GraphicsState *gs;
        CAPY_CPP_CHECK(capy_graphics_state_new(&gs));
        _d.reset(gs);
    }

    void set_BM(CapyPDF_Blend_Mode value) {
        CAPY_CPP_CHECK(capy_graphics_state_set_BM(*this, value));
    }
    void set_ca(double value) { CAPY_CPP_CHECK(capy_graphics_state_set_ca(*this, value)); }
    void set_CA(double value) { CAPY_CPP_CHECK(capy_graphics_state_set_CA(*this, value)); }
    void set_op(int32_t value) { CAPY_CPP_CHECK(capy_graphics_state_set_op(*this, value)); }
    void set_OP(int32_t value) { CAPY_CPP_CHECK(capy_graphics_state_set_OP(*this, value)); }
    void set_OPM(int32_t value) { CAPY_CPP_CHECK(capy_graphics_state_set_OPM(*this, value)); }
    void set_TK(int32_t value) { CAPY_CPP_CHECK(capy_graphics_state_set_TK(*this, value)); }
};

class Type2Function : public CapyC<CapyPDF_Function> {
public:
    friend class Generator;

    Type2Function(double *domain, int32_t num_domain, Color &c1, Color &c2, double n = 1.0) {
        CapyPDF_Function *fn;
        CAPY_CPP_CHECK(capy_type2_function_new(domain, num_domain, c1, c2, n, &fn))
        _d.reset(fn);
    }
};

class Type3Function : public CapyC<CapyPDF_Function> {
public:
    friend class Generator;

    Type3Function(double *domain,
                  int32_t num_domain,
                  CapyPDF_FunctionId *functions,
                  int32_t num_functions,
                  double *bounds,
                  int32_t num_bounds,
                  double *encode,
                  int32_t num_encode) {
        CapyPDF_Function *fn;
        CAPY_CPP_CHECK(capy_type3_function_new(domain,
                                               num_domain,
                                               functions,
                                               num_functions,
                                               bounds,
                                               num_bounds,
                                               encode,
                                               num_encode,
                                               &fn))
        _d.reset(fn);
    }
};

class Type2Shading : public CapyC<CapyPDF_Shading> {
public:
    friend class Generator;

    Type2Shading(CapyPDF_DeviceColorspace cs,
                 double x0,
                 double y0,
                 double x1,
                 double y1,
                 CapyPDF_FunctionId func) {
        CapyPDF_Shading *sd;
        CAPY_CPP_CHECK(capy_type2_shading_new(cs, x0, y0, x1, y1, func, &sd));
        _d.reset(sd);
    }

    void set_extend(bool starting, bool ending) {
        CAPY_CPP_CHECK(capy_shading_set_extend(*this, starting, ending));
    }
    void set_domain(double starting, double ending) {
        CAPY_CPP_CHECK(capy_shading_set_domain(*this, starting, ending));
    }
};

class Type3Shading : public CapyC<CapyPDF_Shading> {
public:
    friend class Generator;

    Type3Shading(CapyPDF_DeviceColorspace cs,
                 double *coords,
                 uint32_t num_coords,
                 CapyPDF_FunctionId func) {
        if(num_coords != 6) {
            throw PdfException("Coords array must hold exactly 6 doubles");
        }
        CapyPDF_Shading *sd;
        CAPY_CPP_CHECK(capy_type3_shading_new(cs, coords, func, &sd));
        _d.reset(sd);
    }
    void set_extend(bool starting, bool ending) {
        CAPY_CPP_CHECK(capy_shading_set_extend(*this, starting, ending));
    }
    void set_domain(double starting, double ending) {
        CAPY_CPP_CHECK(capy_shading_set_domain(*this, starting, ending));
    }
};

class Type4Shading : public CapyC<CapyPDF_Shading> {
public:
    friend class Generator;

    Type4Shading(CapyPDF_DeviceColorspace cs, double minx, double miny, double maxx, double maxy) {
        CapyPDF_Shading *sd;
        CAPY_CPP_CHECK(capy_type4_shading_new(cs, minx, miny, maxx, maxy, &sd));
        _d.reset(sd);
    }
    void add_triangle(double *coords, uint32_t num_coords, Color *colors, uint32_t num_colors) {
        if(num_coords != 6) {
            throw PdfException("Coords must have exactly 6 doubles.");
        }
        if(num_colors != 3) {
            throw PdfException("Triangle patch must have exactly 3 colors");
        }
        CapyPDF_Color *c_colors[3];
        for(uint32_t i = 0; i < 3; i++) {
            c_colors[i] = colors[i];
        }
        CAPY_CPP_CHECK(
            capy_type4_shading_add_triangle(*this, coords, (const CapyPDF_Color **)c_colors));
    }
    void extend(uint32_t flag, double *coords, uint32_t num_coords, Color &color) {
        if(flag != 1 and flag != 2) {
            throw PdfException("Bad flag value");
        }
        if(num_coords != 2) {
            throw PdfException("Coords must have exactly 2 doubles");
        }
        CAPY_CPP_CHECK(capy_type4_shading_extend(*this, flag, coords, color));
    }
};

class Type6Shading : public CapyC<CapyPDF_Shading> {
public:
    friend class Generator;

    Type6Shading(CapyPDF_DeviceColorspace cs, double minx, double miny, double maxx, double maxy) {
        CapyPDF_Shading *sd;
        CAPY_CPP_CHECK(capy_type6_shading_new(cs, minx, miny, maxx, maxy, &sd));
        _d.reset(sd);
    }
    void add_patch(double *coords, uint32_t num_coords, Color *colors, uint32_t num_colors) {
        if(num_coords != 24) {
            throw PdfException("Coords must have exactly 24 doubles.");
        }
        if(num_colors != 4) {
            throw PdfException("Shading patch must have exactly 4 colors");
        }
        CapyPDF_Color *c_colors[4];
        for(uint32_t i = 0; i < 4; i++) {
            c_colors[i] = colors[i];
        }
        CAPY_CPP_CHECK(
            capy_type6_shading_add_patch(*this, coords, (const CapyPDF_Color **)c_colors));
    }
    void
    extend(uint32_t flag, double *coords, uint32_t num_coords, Color *colors, uint32_t num_colors) {
        if(flag != 1 and flag != 2) {
            throw PdfException("Bad flag value");
        }
        if(num_coords != 16) {
            throw PdfException("Coords must have exactly 2 doubles");
        }
        if(num_colors != 2) {
            throw PdfException("Shading extension must have exactly 2 colors");
        }
        CapyPDF_Color *c_colors[2];
        for(uint32_t i = 0; i < 2; i++) {
            c_colors[i] = colors[i];
        }
        CAPY_CPP_CHECK(
            capy_type6_shading_extend(*this, flag, coords, (const CapyPDF_Color **)c_colors));
    }
};

class ShadingPattern : public CapyC<CapyPDF_ShadingPattern> {
    friend class Generator;

public:
    ShadingPattern(CapyPDF_ShadingId shid) {
        CapyPDF_ShadingPattern *sp;
        CAPY_CPP_CHECK(capy_shading_pattern_new(shid, &sp));
        _d.reset(sp);
    }

    void set_matrix(double a, double b, double c, double d, double e, double f) {
        CAPY_CPP_CHECK(capy_shading_pattern_set_matrix(*this, a, b, c, d, e, f));
    }
};

class DrawContext : public CapyC<CapyPDF_DrawContext> {
    friend class Generator;

public:
    DrawContext() = delete;
    void cmd_b() { CAPY_CPP_CHECK(capy_dc_cmd_b(*this)); }
    void cmd_B() { CAPY_CPP_CHECK(capy_dc_cmd_B(*this)); }
    void cmd_bstar() { CAPY_CPP_CHECK(capy_dc_cmd_bstar(*this)); }
    void cmd_Bstar() { CAPY_CPP_CHECK(capy_dc_cmd_Bstar(*this)); }

    void cmd_c(double x1, double y1, double x2, double y2, double x3, double y3) {
        CAPY_CPP_CHECK(capy_dc_cmd_c(*this, x1, y1, x2, y2, x3, y3));
    }
    void cmd_cm(double a, double b, double c, double d, double e, double f) {
        CAPY_CPP_CHECK(capy_dc_cmd_cm(*this, a, b, c, d, e, f));
    }
    void cmd_f() { CAPY_CPP_CHECK(capy_dc_cmd_f(*this)); }
    void cmd_fstar() { CAPY_CPP_CHECK(capy_dc_cmd_fstar(*this)); }

    void cmd_g(double g) { CAPY_CPP_CHECK(capy_dc_cmd_g(*this, g)); }
    void cmd_G(double g) { CAPY_CPP_CHECK(capy_dc_cmd_G(*this, g)); }
    void cmd_h() { CAPY_CPP_CHECK(capy_dc_cmd_h(*this)); }
    void cmd_k(double c, double m, double y, double k) {
        CAPY_CPP_CHECK(capy_dc_cmd_k(*this, c, m, y, k));
    }
    void cmd_K(double c, double m, double y, double k) {
        CAPY_CPP_CHECK(capy_dc_cmd_K(*this, c, m, y, k));
    }
    void cmd_m(double x, double y) { CAPY_CPP_CHECK(capy_dc_cmd_m(*this, x, y)); }
    void cmd_l(double x, double y) { CAPY_CPP_CHECK(capy_dc_cmd_l(*this, x, y)); }

    void cmd_q() { CAPY_CPP_CHECK(capy_dc_cmd_q(*this)); }
    void cmd_Q() { CAPY_CPP_CHECK(capy_dc_cmd_Q(*this)); }

    void cmd_re(double x, double y, double w, double h) {
        CAPY_CPP_CHECK(capy_dc_cmd_re(*this, x, y, w, h));
    }
    void cmd_rg(double r, double g, double b) { CAPY_CPP_CHECK(capy_dc_cmd_rg(*this, r, g, b)); }
    void cmd_RG(double r, double g, double b) { CAPY_CPP_CHECK(capy_dc_cmd_RG(*this, r, g, b)); }

    void set_nonstroke(Color &color) { CAPY_CPP_CHECK(capy_dc_set_nonstroke(*this, color)); }
    void set_stroke(Color &color) { CAPY_CPP_CHECK(capy_dc_set_stroke(*this, color)); }
    void cmd_w(double value) { CAPY_CPP_CHECK(capy_dc_cmd_w(*this, value)); }
    void cmd_M(double value) { CAPY_CPP_CHECK(capy_dc_cmd_M(*this, value)); }
    void cmd_j(CapyPDF_Line_Join value) { CAPY_CPP_CHECK(capy_dc_cmd_j(*this, value)); }
    void cmd_J(CapyPDF_Line_Cap value) { CAPY_CPP_CHECK(capy_dc_cmd_J(*this, value)); }
    void cmd_d(double *values, int32_t num_values, double offset) {
        CAPY_CPP_CHECK(capy_dc_cmd_d(*this, values, num_values, offset));
    }
    void cmd_Do(CapyPDF_TransparencyGroupId tgid) { CAPY_CPP_CHECK(capy_dc_cmd_Do(*this, tgid)); }
    void cmd_gs(CapyPDF_GraphicsStateId gsid) { CAPY_CPP_CHECK(capy_dc_cmd_gs(*this, gsid)); }

    void cmd_s() { CAPY_CPP_CHECK(capy_dc_cmd_s(*this)); }
    void cmd_S() { CAPY_CPP_CHECK(capy_dc_cmd_S(*this)); }
    void cmd_W() { CAPY_CPP_CHECK(capy_dc_cmd_W(*this)); }
    void cmd_Wstar() { CAPY_CPP_CHECK(capy_dc_cmd_Wstar(*this)); }

    void set_custom_page_properties(PageProperties const &props) {
        CAPY_CPP_CHECK(capy_dc_set_custom_page_properties(*this, props));
    }

    void set_transparency_group_properties(TransparencyGroupProperties &trprop) {
        CAPY_CPP_CHECK(capy_dc_set_transparency_group_properties(*this, trprop));
    }

    void set_group_matrix(double a, double b, double c, double d, double e, double f) {
        CAPY_CPP_CHECK(capy_dc_set_group_matrix(*this, a, b, c, d, e, f));
    }

private:
    explicit DrawContext(CapyPDF_DrawContext *dc) { _d.reset(dc); }
};

class RasterImage : public CapyC<CapyPDF_RasterImage> {
    friend class Generator;
    friend class RasterImageBuilder;

public:
    RasterImage() = delete;

private:
    RasterImage(CapyPDF_RasterImage *ri) { _d.reset(ri); }
};

class RasterImageBuilder : public CapyC<CapyPDF_RasterImageBuilder> {
public:
    RasterImageBuilder() {
        CapyPDF_RasterImageBuilder *rib;
        CAPY_CPP_CHECK(capy_raster_image_builder_new(&rib));
        _d.reset(rib);
    }
};

class Generator : public CapyC<CapyPDF_Generator> {
public:
    Generator(const char *filename, const DocumentMetadata &md) {
        CapyPDF_Generator *gen;
        CAPY_CPP_CHECK(capy_generator_new(filename, md, &gen));
        _d.reset(gen);
    }

    DrawContext new_page_context() {
        CapyPDF_DrawContext *dc;
        CAPY_CPP_CHECK(capy_page_draw_context_new(*this, &dc));
        return DrawContext(dc);
    }

    DrawContext
    new_transparency_group_context(double left, double bottom, double right, double top) {
        CapyPDF_DrawContext *dc;
        CAPY_CPP_CHECK(capy_transparency_group_new(*this, left, bottom, right, top, &dc));
        return DrawContext(dc);
    }

    DrawContext new_tiling_pattern_context(double l, double b, double r, double t) {
        CapyPDF_DrawContext *dc;
        CAPY_CPP_CHECK(capy_tiling_pattern_context_new(*this, &dc, l, b, r, t));
        return DrawContext(dc);
    }

    void add_page(DrawContext &dc){CAPY_CPP_CHECK(capy_generator_add_page(*this, dc))}

    CapyPDF_FontId load_font(const char *fname) {
        CapyPDF_FontId fid;
        CAPY_CPP_CHECK(capy_generator_load_font(*this, fname, &fid));
        return fid;
    }

    CapyPDF_IccColorSpaceId load_icc_profile(const char *fname) {
        CapyPDF_IccColorSpaceId cpid;
        CAPY_CPP_CHECK(capy_generator_load_icc_profile(*this, fname, &cpid));
        return cpid;
    }

    CapyPDF_IccColorSpaceId add_icc_profile(std::string_view bytes, int32_t num_channels) {
        CapyPDF_IccColorSpaceId cpid;
        CAPY_CPP_CHECK(
            capy_generator_add_icc_profile(*this, bytes.data(), bytes.size(), num_channels, &cpid));
        return cpid;
    }

    CapyPDF_FunctionId add_function(Type2Function &fn) {
        CapyPDF_FunctionId fid;
        CAPY_CPP_CHECK(capy_generator_add_function(*this, fn, &fid));
        return fid;
    }

    CapyPDF_FunctionId add_function(Type3Function &fn) {
        CapyPDF_FunctionId fid;
        CAPY_CPP_CHECK(capy_generator_add_function(*this, fn, &fid));
        return fid;
    }

    CapyPDF_ShadingId add_shading(Type2Shading &sh) {
        CapyPDF_ShadingId sid;
        CAPY_CPP_CHECK(capy_generator_add_shading(*this, sh, &sid));
        return sid;
    }

    CapyPDF_ShadingId add_shading(Type3Shading &sh) {
        CapyPDF_ShadingId sid;
        CAPY_CPP_CHECK(capy_generator_add_shading(*this, sh, &sid));
        return sid;
    }

    CapyPDF_ShadingId add_shading(Type4Shading &sh) {
        CapyPDF_ShadingId sid;
        CAPY_CPP_CHECK(capy_generator_add_shading(*this, sh, &sid));
        return sid;
    }

    CapyPDF_ShadingId add_shading(Type6Shading &sh) {
        CapyPDF_ShadingId sid;
        CAPY_CPP_CHECK(capy_generator_add_shading(*this, sh, &sid));
        return sid;
    }

    CapyPDF_PatternId add_shading_pattern(ShadingPattern &sp) {
        CapyPDF_PatternId pid;
        CAPY_CPP_CHECK(capy_generator_add_shading_pattern(*this, sp, &pid));
        return pid;
    }

    CapyPDF_PatternId add_tiling_pattern(DrawContext &ctx) {
        CapyPDF_PatternId pid;
        CAPY_CPP_CHECK(capy_generator_add_tiling_pattern(*this, ctx, &pid));
        return pid;
    }

    RasterImage load_image(const char *fname) {
        CapyPDF_RasterImage *im;
        CAPY_CPP_CHECK(capy_generator_load_image(*this, fname, &im));
        return RasterImage(im);
    }

    CapyPDF_GraphicsStateId add_graphics_state(GraphicsState const &gstate) {
        CapyPDF_GraphicsStateId gsid;
        CAPY_CPP_CHECK(capy_generator_add_graphics_state(*this, gstate, &gsid));
        return gsid;
    }

    CapyPDF_TransparencyGroupId add_transparency_group(DrawContext &dct) {
        CapyPDF_TransparencyGroupId tgid;
        CAPY_CPP_CHECK(capy_generator_add_transparency_group(*this, dct, &tgid));
        return tgid;
    }

    void write() { CAPY_CPP_CHECK(capy_generator_write(*this)); }
};

} // namespace capypdf
