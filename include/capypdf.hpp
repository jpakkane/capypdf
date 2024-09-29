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
