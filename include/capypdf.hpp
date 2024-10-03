// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#pragma once

// The functionality in this header is neither ABI nor API stable.
// If you need that, use the plain C header.

#include <capypdf.h>
#include <memory>

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

template<typename T, typename TDeleter> class CapyC {
public:
    T *get() { return d.get(); }
    const T *get() const { return d.get(); }

protected:
    std::unique_ptr<T, TDeleter> d;
};

// Why a struct and not a function pointer? Because then the unique_ptr
// would need to store a pointer to the function itself because it
// needs to work with all functions that satisfy the prototype.
struct OptionDeleter {
    void operator()(CapyPDF_Options *opt) {
        auto rc = capy_options_destroy(opt); // Can't really do anything if this fails.
        (void)rc;
    }
};

class PdfOptions : public CapyC<CapyPDF_Options, OptionDeleter> {
public:
    PdfOptions() {
        CapyPDF_Options *opt;
        CAPY_CPP_CHECK(capy_options_new(&opt));
        d.reset(opt);
    }
};
static_assert(sizeof(PdfOptions) == sizeof(void *));

struct PropsDeleter {
    void operator()(CapyPDF_PageProperties *opt) {
        auto rc = capy_page_properties_destroy(opt); // Can't really do anything if this fails.
        (void)rc;
    }
};

class PageProperties : public CapyC<CapyPDF_PageProperties, PropsDeleter> {
public:
    PageProperties() {
        CapyPDF_PageProperties *prop;
        CAPY_CPP_CHECK(capy_page_properties_new(&prop));
        d.reset(prop);
    }

    void set_pagebox(CapyPDF_Page_Box boxtype, double x1, double y1, double x2, double y2) {
        CAPY_CPP_CHECK(capy_page_properties_set_pagebox(d.get(), boxtype, x1, y1, x2, y2));
    }
};

struct TextSeqDeleter {
    void operator()(CapyPDF_TextSequence *ts) {
        auto rc = capy_text_sequence_destroy(ts);
        (void)rc;
    }
};

class TextSequence : public CapyC<CapyPDF_TextSequence, TextSeqDeleter> {
public:
    TextSequence() {
        CapyPDF_TextSequence *ts;
        CAPY_CPP_CHECK(capy_text_sequence_new(&ts));
        d.reset(ts);
    }

    void append_codepoint(uint32_t codepoint) {
        CAPY_CPP_CHECK(capy_text_sequence_append_codepoint(d.get(), codepoint));
    }
};

struct ColorDeleter {
    void operator()(CapyPDF_Color *c) {
        auto rc = capy_color_destroy(c);
        (void)rc;
    }
};

class Color : public CapyC<CapyPDF_Color, ColorDeleter> {
public:
    Color() {
        CapyPDF_Color *c;
        CAPY_CPP_CHECK(capy_color_new(&c));
        d.reset(c);
    }

    void set_rgb(double r, double g, double b) {
        CAPY_CPP_CHECK(capy_color_set_rgb(d.get(), r, g, b));
    }
};

struct GSDeleter {
    void operator()(CapyPDF_GraphicsState *gs) {
        auto rc = capy_graphics_state_destroy(gs);
        (void)rc;
    }
};

class GraphicsState : public CapyC<CapyPDF_GraphicsState, GSDeleter> {
public:
    GraphicsState() {
        CapyPDF_GraphicsState *gs;
        CAPY_CPP_CHECK(capy_graphics_state_new(&gs));
        d.reset(gs);
    }

    void set_CA(double value) { CAPY_CPP_CHECK(capy_graphics_state_set_CA(d.get(), value)); }
};

struct DCDeleter {
    void operator()(CapyPDF_DrawContext *dc) {
        auto rc = capy_dc_destroy(dc);
        (void)rc;
    }
};

class DrawContext : public CapyC<CapyPDF_DrawContext, DCDeleter> {
    friend class Generator;

public:
    void cmd_f() { CAPY_CPP_CHECK(capy_dc_cmd_f(d.get())); }
    void cmd_re(double x, double y, double w, double h) {
        CAPY_CPP_CHECK(capy_dc_cmd_re(d.get(), x, y, w, h));
    }
    void cmd_rg(double r, double g, double b) { CAPY_CPP_CHECK(capy_dc_cmd_rg(d.get(), r, g, b)); }

private:
    DrawContext(CapyPDF_DrawContext *dc) { d.reset(dc); }
};

struct RIDeleter {
    void operator()(CapyPDF_RasterImage *ri) {
        auto rc = capy_raster_image_destroy(ri);
        (void)rc;
    }
};

class RasterImage : public CapyC<CapyPDF_RasterImage, RIDeleter> {
    friend class Generator;
    friend class RasterImageBuilder;

public:
private:
    RasterImage(CapyPDF_RasterImage *ri) { d.reset(ri); }
};

struct RIBDeleter {
    void operator()(CapyPDF_RasterImageBuilder *rib) {
        auto rc = capy_raster_image_builder_destroy(rib);
        (void)rc;
    }
};

class RasterImageBuilder : public CapyC<CapyPDF_RasterImageBuilder, RIBDeleter> {
public:
    RasterImageBuilder() {
        CapyPDF_RasterImageBuilder *rib;
        CAPY_CPP_CHECK(capy_raster_image_builder_new(&rib));
        d.reset(rib);
    }
};

struct GenDeleter {
    void operator()(CapyPDF_Generator *gen) {
        auto rc = capy_generator_destroy(gen);
        (void)rc;
    }
};

class Generator : public CapyC<CapyPDF_Generator, GenDeleter> {
public:
    Generator(const char *filename, const PdfOptions &opt) {
        CapyPDF_Generator *gen;
        CAPY_CPP_CHECK(capy_generator_new(filename, opt.get(), &gen));
        d.reset(gen);
    }

    DrawContext new_page_context() {
        CapyPDF_DrawContext *dc;
        CAPY_CPP_CHECK(capy_page_draw_context_new(d.get(), &dc));
        return DrawContext(dc);
    }

    void add_page(DrawContext &dc){CAPY_CPP_CHECK(capy_generator_add_page(d.get(), dc.get()))}

    CapyPDF_FontId load_font(const char *fname) {
        CapyPDF_FontId fid;
        CAPY_CPP_CHECK(capy_generator_load_font(d.get(), fname, &fid));
        return fid;
    }

    RasterImage load_image(const char *fname) {
        CapyPDF_RasterImage *im;
        CAPY_CPP_CHECK(capy_generator_load_image(d.get(), fname, &im));
        return RasterImage(im);
    }

    void write() { CAPY_CPP_CHECK(capy_generator_write(d.get())); }
};

} // namespace capypdf
