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

class PdfOptions {
public:
    PdfOptions() {
        CapyPDF_Options *opt;
        CAPY_CPP_CHECK(capy_options_new(&opt));
        d.reset(opt);
    }

    CapyPDF_Options *get() { return d.get(); }
    const CapyPDF_Options *get() const { return d.get(); }

private:
    // Why a struct and not a function pointer? Because then the unique_ptr
    // would need to store a pointer to the function itself because it
    // needs to work with all functions that satisfy the prototype.
    struct OptionDeleter {
        void operator()(CapyPDF_Options *opt) {
            auto rc = capy_options_destroy(opt); // Can't really do anything if this fails.
            (void)rc;
        }
    };

    std::unique_ptr<CapyPDF_Options, OptionDeleter> d;
};
static_assert(sizeof(PdfOptions) == sizeof(void *));

class PageProperties {
public:
    PageProperties() {
        CapyPDF_PageProperties *prop;
        CAPY_CPP_CHECK(capy_page_properties_new(&prop));
        d.reset(prop);
    }

    void set_pagebox(CapyPDF_Page_Box boxtype, double x1, double y1, double x2, double y2) {
        CAPY_CPP_CHECK(capy_page_properties_set_pagebox(d.get(), boxtype, x1, y1, x2, y2));
    }

    CapyPDF_PageProperties *get() { return d.get(); }
    const CapyPDF_PageProperties *get() const { return d.get(); }

private:
    struct PropsDeleter {
        void operator()(CapyPDF_PageProperties *opt) {
            auto rc = capy_page_properties_destroy(opt); // Can't really do anything if this fails.
            (void)rc;
        }
    };

    std::unique_ptr<CapyPDF_PageProperties, PropsDeleter> d;
};

class TextSequence {
public:
    TextSequence() {
        CapyPDF_TextSequence *ts;
        CAPY_CPP_CHECK(capy_text_sequence_new(&ts));
        d.reset(ts);
    }

    void append_codepoint(uint32_t codepoint) {
        CAPY_CPP_CHECK(capy_text_sequence_append_codepoint(d.get(), codepoint));
    }

    CapyPDF_TextSequence *get() { return d.get(); }
    const CapyPDF_TextSequence *get() const { return d.get(); }

private:
    struct TextSeqDeleter {
        void operator()(CapyPDF_TextSequence *ts) {
            auto rc = capy_text_sequence_destroy(ts);
            (void)rc;
        }
    };

    std::unique_ptr<CapyPDF_TextSequence, TextSeqDeleter> d;
};

class Color {
public:
    Color() {
        CapyPDF_Color *c;
        CAPY_CPP_CHECK(capy_color_new(&c));
        d.reset(c);
    }

    void set_rgb(double r, double g, double b) {
        CAPY_CPP_CHECK(capy_color_set_rgb(d.get(), r, g, b));
    }

    CapyPDF_Color *get() { return d.get(); }
    const CapyPDF_Color *get() const { return d.get(); }

private:
    struct ColorDeleter {
        void operator()(CapyPDF_Color *c) {
            auto rc = capy_color_destroy(c);
            (void)rc;
        }
    };

    std::unique_ptr<CapyPDF_Color, ColorDeleter> d;
};

class GraphicsState {
public:
    GraphicsState() {
        CapyPDF_GraphicsState *gs;
        CAPY_CPP_CHECK(capy_graphics_state_new(&gs));
        d.reset(gs);
    }

    void set_CA(double value) { CAPY_CPP_CHECK(capy_graphics_state_set_CA(d.get(), value)); }

    CapyPDF_GraphicsState *get() { return d.get(); }
    const CapyPDF_GraphicsState *get() const { return d.get(); }

private:
    struct GSDeleter {
        void operator()(CapyPDF_GraphicsState *gs) {
            auto rc = capy_graphics_state_destroy(gs);
            (void)rc;
        }
    };

    std::unique_ptr<CapyPDF_GraphicsState, GSDeleter> d;
};

class DrawContext {
    friend class Generator;

public:
    void cmd_f() { CAPY_CPP_CHECK(capy_dc_cmd_f(d.get())); }
    void cmd_re(double x, double y, double w, double h) {
        CAPY_CPP_CHECK(capy_dc_cmd_re(d.get(), x, y, w, h));
    }
    void cmd_rg(double r, double g, double b) { CAPY_CPP_CHECK(capy_dc_cmd_rg(d.get(), r, g, b)); }

    CapyPDF_DrawContext *get() { return d.get(); }
    const CapyPDF_DrawContext *get() const { return d.get(); }

private:
    DrawContext(CapyPDF_DrawContext *dc) : d{dc} {}

    struct DCDeleter {
        void operator()(CapyPDF_DrawContext *dc) {
            auto rc = capy_dc_destroy(dc);
            (void)rc;
        }
    };

    std::unique_ptr<CapyPDF_DrawContext, DCDeleter> d;
};

class Generator {
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

    void add_page(DrawContext &dc) { CAPY_CPP_CHECK(capy_generator_add_page(d.get(), dc.get())) }

    void write() { CAPY_CPP_CHECK(capy_generator_write(d.get())); }

    CapyPDF_Generator *get() { return d.get(); }
    const CapyPDF_Generator *get() const { return d.get(); }

private:
    struct GenDeleter {
        void operator()(CapyPDF_Generator *gen) {
            auto rc = capy_generator_destroy(gen);
            (void)rc;
        }
    };

    std::unique_ptr<CapyPDF_Generator, GenDeleter> d;
};

} // namespace capypdf
