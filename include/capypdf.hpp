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
    T *get() { return d.get(); }
    const T *get() const { return d.get(); }

    operator T *() { return d.get(); }
    operator const T *() const { return d.get(); }

protected:
    std::unique_ptr<T, CapyCTypeDeleter> d;
};

// Why a struct and not a function pointer? Because then the unique_ptr
// would need to store a pointer to the function itself because it
// needs to work with all functions that satisfy the prototype.

class DocumentMetadata : public CapyC<CapyPDF_DocumentMetadata> {
public:
    DocumentMetadata() {
        CapyPDF_DocumentMetadata *md;
        CAPY_CPP_CHECK(capy_doc_md_new(&md));
        d.reset(md);
    }
};
static_assert(sizeof(DocumentMetadata) == sizeof(void *));

class PageProperties : public CapyC<CapyPDF_PageProperties> {
public:
    PageProperties() {
        CapyPDF_PageProperties *prop;
        CAPY_CPP_CHECK(capy_page_properties_new(&prop));
        d.reset(prop);
    }

    void set_pagebox(CapyPDF_Page_Box boxtype, double x1, double y1, double x2, double y2) {
        CAPY_CPP_CHECK(capy_page_properties_set_pagebox(*this, boxtype, x1, y1, x2, y2));
    }
};

struct TextSeqDeleter {
    void operator()(CapyPDF_TextSequence *ts) {
        auto rc = capy_text_sequence_destroy(ts);
        (void)rc;
    }
};

class TextSequence : public CapyC<CapyPDF_TextSequence> {
public:
    TextSequence() {
        CapyPDF_TextSequence *ts;
        CAPY_CPP_CHECK(capy_text_sequence_new(&ts));
        d.reset(ts);
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
        d.reset(c);
    }

    void set_rgb(double r, double g, double b) {
        CAPY_CPP_CHECK(capy_color_set_rgb(*this, r, g, b));
    }
};

class GraphicsState : public CapyC<CapyPDF_GraphicsState> {
public:
    GraphicsState() {
        CapyPDF_GraphicsState *gs;
        CAPY_CPP_CHECK(capy_graphics_state_new(&gs));
        d.reset(gs);
    }

    void set_CA(double value) { CAPY_CPP_CHECK(capy_graphics_state_set_CA(*this, value)); }
};

class DrawContext : public CapyC<CapyPDF_DrawContext> {
    friend class Generator;

public:
    void cmd_f() { CAPY_CPP_CHECK(capy_dc_cmd_f(*this)); }
    void cmd_re(double x, double y, double w, double h) {
        CAPY_CPP_CHECK(capy_dc_cmd_re(*this, x, y, w, h));
    }
    void cmd_rg(double r, double g, double b) { CAPY_CPP_CHECK(capy_dc_cmd_rg(*this, r, g, b)); }

private:
    DrawContext(CapyPDF_DrawContext *dc) { d.reset(dc); }
};

class RasterImage : public CapyC<CapyPDF_RasterImage> {
    friend class Generator;
    friend class RasterImageBuilder;

public:
private:
    RasterImage(CapyPDF_RasterImage *ri) { d.reset(ri); }
};

class RasterImageBuilder : public CapyC<CapyPDF_RasterImageBuilder> {
public:
    RasterImageBuilder() {
        CapyPDF_RasterImageBuilder *rib;
        CAPY_CPP_CHECK(capy_raster_image_builder_new(&rib));
        d.reset(rib);
    }
};

class Generator : public CapyC<CapyPDF_Generator> {
public:
    Generator(const char *filename, const DocumentMetadata &md) {
        CapyPDF_Generator *gen;
        CAPY_CPP_CHECK(capy_generator_new(filename, md, &gen));
        d.reset(gen);
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

    RasterImage load_image(const char *fname) {
        CapyPDF_RasterImage *im;
        CAPY_CPP_CHECK(capy_generator_load_image(*this, fname, &im));
        return RasterImage(im);
    }

    void write() { CAPY_CPP_CHECK(capy_generator_write(*this)); }
};

} // namespace capypdf
