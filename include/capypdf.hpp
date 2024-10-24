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
        } else if constexpr(std::is_same_v<T, CapyPDF_Generator>) {
            rc = capy_generator_destroy(cobj);
        } else {
            static_assert(std::is_same_v<T, CapyPDF_DocumentMetadata>, "Unknown C object type.");
        }
        (void)rc; // Not much we can do about this because destructors should not fail or throw.
    }
};

template<typename T> class CapyC {
public:
    T *get() { return _d.get(); }
    const T *get() const { return _d.get(); }

    operator T*() { return _d.get(); }
    operator const T*() const { return _d.get(); }

protected:
    std::unique_ptr<T, CapyCTypeDeleter> _d;
};

class DocumentMetadata : public CapyC<CapyPDF_DocumentMetadata> {
public:
    DocumentMetadata() {
        CapyPDF_DocumentMetadata *md;
        CAPY_CPP_CHECK(capy_doc_md_new(&md));
        _d.reset(md);
    }
};
static_assert(sizeof(DocumentMetadata) == sizeof(void *));

class PageProperties : public CapyC<CapyPDF_PageProperties> {
public:
    PageProperties() {
        CapyPDF_PageProperties *prop;
        CAPY_CPP_CHECK(capy_page_properties_new(&prop));
        _d.reset(prop);
    }

    void set_pagebox(CapyPDF_Page_Box boxtype, double x1, double y1, double x2, double y2) {
        CAPY_CPP_CHECK(capy_page_properties_set_pagebox(*this, boxtype, x1, y1, x2, y2));
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
    void set_icc(CapyPDF_IccColorSpaceId icc_id, double *values, int32_t num_values)
    {
        CAPY_CPP_CHECK(capy_color_set_icc(*this, icc_id, values, num_values));
    }
};

class GraphicsState : public CapyC<CapyPDF_GraphicsState> {
public:
    GraphicsState() {
        CapyPDF_GraphicsState *gs;
        CAPY_CPP_CHECK(capy_graphics_state_new(&gs));
        _d.reset(gs);
    }

    void set_BM(CapyPDF_Blend_Mode value) { CAPY_CPP_CHECK(capy_graphics_state_set_BM(*this, value)); }
    void set_CA(double value) { CAPY_CPP_CHECK(capy_graphics_state_set_CA(*this, value)); }
    void set_ca(double value) { CAPY_CPP_CHECK(capy_graphics_state_set_ca(*this, value)); }
};

class DrawContext : public CapyC<CapyPDF_DrawContext> {
    friend class Generator;

public:
    void cmd_b() { CAPY_CPP_CHECK(capy_dc_cmd_b(*this)); }
    void cmd_B() { CAPY_CPP_CHECK(capy_dc_cmd_B(*this)); }
    void cmd_bstar() { CAPY_CPP_CHECK(capy_dc_cmd_bstar(*this)); }
    void cmd_Bstar() { CAPY_CPP_CHECK(capy_dc_cmd_Bstar(*this)); }

    void cmd_c(double x1, double y1, double x2, double y2, double x3, double y3) { CAPY_CPP_CHECK(capy_dc_cmd_c(*this, x1, y1, x2, y2, x3, y3)); }
    void cmd_cm(double a, double b, double c, double d, double e, double f) { CAPY_CPP_CHECK(capy_dc_cmd_cm(*this, a, b, c, d, e, f)); }
    void cmd_f() { CAPY_CPP_CHECK(capy_dc_cmd_f(*this)); }
    void cmd_fstar() { CAPY_CPP_CHECK(capy_dc_cmd_fstar(*this)); }

    void cmd_h() { CAPY_CPP_CHECK(capy_dc_cmd_h(*this)); }
    void cmd_m(double x, double y) { CAPY_CPP_CHECK(capy_dc_cmd_m(*this, x, y)); }
    void cmd_l(double x, double y) { CAPY_CPP_CHECK(capy_dc_cmd_l(*this, x, y)); }

    void cmd_q() { CAPY_CPP_CHECK(capy_dc_cmd_q(*this)); }
    void cmd_Q() { CAPY_CPP_CHECK(capy_dc_cmd_Q(*this)); }

    void cmd_re(double x, double y, double w, double h) {
        CAPY_CPP_CHECK(capy_dc_cmd_re(*this, x, y, w, h));
    }
    void cmd_rg(double r, double g, double b) { CAPY_CPP_CHECK(capy_dc_cmd_rg(*this, r, g, b)); }

    void set_nonstroke(Color &color) { CAPY_CPP_CHECK(capy_dc_set_nonstroke(*this, color)); }
    void set_stroke(Color &color) { CAPY_CPP_CHECK(capy_dc_set_stroke(*this, color)); }
    void cmd_w(double value) { CAPY_CPP_CHECK(capy_dc_cmd_w(*this, value)); }
    void cmd_M(double value) { CAPY_CPP_CHECK(capy_dc_cmd_M(*this, value)); }
    void cmd_j(CapyPDF_Line_Join value) { CAPY_CPP_CHECK(capy_dc_cmd_j(*this, value)); }
    void cmd_J(CapyPDF_Line_Cap value) { CAPY_CPP_CHECK(capy_dc_cmd_J(*this, value)); }
    void cmd_d(double *values, int32_t num_values, double offset) { CAPY_CPP_CHECK(capy_dc_cmd_d(*this, values, num_values, offset)); }
    void cmd_gs(CapyPDF_GraphicsStateId gsid) { CAPY_CPP_CHECK(capy_dc_cmd_gs(*this, gsid)); }

    void cmd_s() { CAPY_CPP_CHECK(capy_dc_cmd_s(*this)); }
    void cmd_S() { CAPY_CPP_CHECK(capy_dc_cmd_S(*this)); }
    void cmd_W() { CAPY_CPP_CHECK(capy_dc_cmd_W(*this)); }
    void cmd_Wstar() { CAPY_CPP_CHECK(capy_dc_cmd_Wstar(*this)); }

    void set_custom_page_properties(PageProperties const &props) {
        CAPY_CPP_CHECK(capy_dc_set_custom_page_properties(*this, props));
    }
private:
    explicit DrawContext(CapyPDF_DrawContext *dc) { _d.reset(dc); }
};

class RasterImage : public CapyC<CapyPDF_RasterImage> {
    friend class Generator;
    friend class RasterImageBuilder;

public:
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

    RasterImage load_image(const char *fname) {
        CapyPDF_RasterImage *im;
        CAPY_CPP_CHECK(capy_generator_load_image(*this, fname, &im));
        return RasterImage(im);
    }

    void write() { CAPY_CPP_CHECK(capy_generator_write(*this)); }
};

} // namespace capypdf
