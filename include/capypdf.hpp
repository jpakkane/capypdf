// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#pragma once

// The functionality in this header is neither ABI nor API stable.
// If you need that, use the plain C header.

#include <capypdf.h>
#include <memory>
#include <optional>
#include <stdexcept>

#if defined(__cpp_exceptions)
#define CAPY_ERROR_HAPPENED(error_string) throw PdfException(error_string)
#else
#define CAPY_ERROR_HAPPENED(error_string)                                                          \
    fprintf(stderr, "CapyPDF error: %s\n", error_string);                                          \
    std::abort()
#endif

#define CAPY_CPP_CHECK(funccall)                                                                   \
    {                                                                                              \
        auto rc = funccall;                                                                        \
        if(rc != 0) {                                                                              \
            CAPY_ERROR_HAPPENED(capy_error_message(rc));                                           \
        }                                                                                          \
    }

namespace capypdf {

class PdfException : public std::runtime_error {
public:
    PdfException(const char *msg) : std::runtime_error(msg) {}
};

template<typename T>
concept ByteSequence = requires(T a) {
    a.data();
    a.size();
};

struct CapyCTypeDeleter {
    template<typename T> void operator()(T *cobj) {
        int32_t rc;
        if constexpr(std::is_same_v<T, CapyPDF_DocumentProperties>) {
            rc = capy_document_properties_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_PageProperties>) {
            rc = capy_page_properties_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_Text>) {
            rc = capy_text_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_TextSequence>) {
            rc = capy_text_sequence_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_Annotation>) {
            rc = capy_annotation_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_Color>) {
            rc = capy_color_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_Destination>) {
            rc = capy_destination_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_GraphicsState>) {
            rc = capy_graphics_state_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_DrawContext>) {
            rc = capy_dc_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_ImagePdfProperties>) {
            rc = capy_image_pdf_properties_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_OptionalContentGroup>) {
            rc = capy_optional_content_group_destroy(cobj);
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
        } else if constexpr(std::is_same_v<T, CapyPDF_SoftMask>) {
            rc = capy_soft_mask_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_Generator>) {
            rc = capy_generator_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_BDCTags>) {
            rc = capy_bdc_tags_destroy(cobj);
        } else if constexpr(std::is_same_v<T, CapyPDF_FontProperties>) {
            rc = capy_font_properties_destroy(cobj);
        } else {
            static_assert(std::is_same_v<T, CapyPDF_DocumentProperties>, "Unknown C object type.");
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

class Destination : public CapyC<CapyPDF_Destination> {
public:
    friend class Annotation;

    Destination() {
        CapyPDF_Destination *dest;
        CAPY_CPP_CHECK(capy_destination_new(&dest));
        _d.reset(dest);
    }

    void set_page_fit(int32_t page_num) {
        CAPY_CPP_CHECK(capy_destination_set_page_fit(*this, page_num));
    }

    void set_page_xyz(int32_t page_num,
                      std::optional<double> x,
                      std::optional<double> y,
                      std::optional<double> z) {
        CAPY_CPP_CHECK(capy_destination_set_page_xyz(
            *this, page_num, x ? &*x : nullptr, y ? &*y : nullptr, z ? &*z : nullptr));
    }
};

class Annotation : public CapyC<CapyPDF_Annotation> {
public:
    friend class Generator;

    Annotation() {
        CapyPDF_Annotation *annot;
        CAPY_CPP_CHECK(capy_link_annotation_new(&annot));
        _d.reset(annot);
    }

    void set_uri(const char *u8str, int32_t slen) {
        CAPY_CPP_CHECK(capy_annotation_set_uri(*this, u8str, slen));
    }
    template<ByteSequence T> void set_uri(const T &text) { set_uri(text.data(), text.size()); }
    void set_rectangle(double x1, double y1, double x2, double y2) {
        CAPY_CPP_CHECK(capy_annotation_set_rectangle(*this, x1, y1, x2, y2));
    }
    void set_flags(CapyPDF_Annotation_Flags flags) {
        CAPY_CPP_CHECK(capy_annotation_set_flags(*this, flags));
    }
    void set_destination(Destination &dest) {
        CAPY_CPP_CHECK(capy_annotation_set_destination(*this, dest));
    }
};

class BDCTags : public CapyC<CapyPDF_BDCTags> {
public:
    friend class DrawContext;

    BDCTags() {
        CapyPDF_BDCTags *tags;
        CAPY_CPP_CHECK(capy_bdc_tags_new(&tags));
        _d.reset(tags);
    }

    void add_tag(const char *key, int32_t keylen, const char *value, int32_t valuelen) {
        CAPY_CPP_CHECK(capy_bdc_tags_add_tag(*this, key, keylen, value, valuelen));
    }
};

class TransparencyGroupProperties : public CapyC<CapyPDF_TransparencyGroupProperties> {
public:
    friend class PageProperties;
    friend class DrawContext;

    TransparencyGroupProperties() {
        CapyPDF_TransparencyGroupProperties *tge;
        CAPY_CPP_CHECK(capy_transparency_group_properties_new(&tge));
        _d.reset(tge);
    }

    void set_CS(CapyPDF_Device_Colorspace cs) {
        CAPY_CPP_CHECK(capy_transparency_group_properties_set_CS(*this, cs));
    }

    void set_I(bool I) { CAPY_CPP_CHECK(capy_transparency_group_properties_set_I(*this, I)); }

    void set_K(bool K) { CAPY_CPP_CHECK(capy_transparency_group_properties_set_K(*this, K)); }
};

class OptionalContentGroup : public CapyC<CapyPDF_OptionalContentGroup> {
public:
    friend class Generator;

    OptionalContentGroup(const char *name) {
        CapyPDF_OptionalContentGroup *ocg;
        CAPY_CPP_CHECK(capy_optional_content_group_new(&ocg, name));
        _d.reset(ocg);
    }
};

class PageProperties : public CapyC<CapyPDF_PageProperties> {
public:
    friend class DocumentProperties;
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

class DocumentProperties : public CapyC<CapyPDF_DocumentProperties> {
public:
    friend class Generator;

    DocumentProperties() {
        CapyPDF_DocumentProperties *dp;
        CAPY_CPP_CHECK(capy_document_properties_new(&dp));
        _d.reset(dp);
    }

    void set_title(const char *title, int32_t strsize = -1) {
        CAPY_CPP_CHECK(capy_document_properties_set_title(*this, title, strsize));
    }
    template<ByteSequence T> void set_title(const T &title) {
        set_title(title.data(), title.size());
    }

    void set_author(const char *author, int32_t strsize = -1) {
        CAPY_CPP_CHECK(capy_document_properties_set_author(*this, author, strsize));
    }
    template<ByteSequence T> void set_author(const T &author) {
        set_author(author.data(), author.size());
    }

    void set_creator(const char *creator, int32_t strsize = -1) {
        CAPY_CPP_CHECK(capy_document_properties_set_creator(*this, creator, strsize));
    }
    template<ByteSequence T> void set_creator(const T &creator) {
        set_creator(creator.data(), creator.size());
    }

    void set_language(const char *lang, int32_t strsize = -1) {
        CAPY_CPP_CHECK(capy_document_properties_set_language(*this, lang, strsize));
    }
    template<ByteSequence T> void set_language(const T &language) {
        set_language(language.data(), language.size());
    }

    void set_device_profile(CapyPDF_Device_Colorspace cs, const char *path) {
        CAPY_CPP_CHECK(capy_document_properties_set_device_profile(*this, cs, path));
    }

    void set_colorspace(CapyPDF_Device_Colorspace cs) {
        CAPY_CPP_CHECK(capy_document_properties_set_colorspace(*this, cs));
    }

    void set_output_intent(const char *identifier) {
        CAPY_CPP_CHECK(capy_document_properties_set_output_intent(*this, identifier, -1));
    }

    void set_pdfx(CapyPDF_PDFX_Type xtype) {
        CAPY_CPP_CHECK(capy_document_properties_set_pdfx(*this, xtype));
    }

    void set_pdfa(CapyPDF_PDFA_Type atype) {
        CAPY_CPP_CHECK(capy_document_properties_set_pdfa(*this, atype));
    }

    void set_default_page_properties(PageProperties const &prop) {
        CAPY_CPP_CHECK(capy_document_properties_set_default_page_properties(*this, prop));
    }

    void set_tagged(bool is_tagged) {
        CAPY_CPP_CHECK(capy_document_properties_set_tagged(*this, is_tagged));
    }
};
static_assert(sizeof(DocumentProperties) == sizeof(void *));

class FontProperties : public CapyC<CapyPDF_FontProperties> {
public:
    friend class Generator;

    FontProperties() {
        CapyPDF_FontProperties *fp;
        CAPY_CPP_CHECK(capy_font_properties_new(&fp));
        _d.reset(fp);
    }

    void set_subfont(int32_t subfont) {
        CAPY_CPP_CHECK(capy_font_properties_set_subfont(*this, subfont));
    }
};

class Color : public CapyC<CapyPDF_Color> {
public:
    friend class DrawContext;
    friend class Text;
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

class TextSequence : public CapyC<CapyPDF_TextSequence> {
    friend class Text;

public:
    TextSequence() {
        CapyPDF_TextSequence *ts;
        CAPY_CPP_CHECK(capy_text_sequence_new(&ts));
        _d.reset(ts);
    }

    void append_codepoint(uint32_t codepoint) {
        CAPY_CPP_CHECK(capy_text_sequence_append_codepoint(*this, codepoint));
    }

    void append_kerning(int32_t kern) {
        CAPY_CPP_CHECK(capy_text_sequence_append_kerning(*this, kern));
    }

    void append_string(const char *u8str, int32_t slen) {
        CAPY_CPP_CHECK(capy_text_sequence_append_string(*this, u8str, slen));
    }
    template<ByteSequence T> void append_string(const T &text) {
        append_string(text.data(), text.size());
    }

    void append_actualtext_start(const char *actual_text) {
        CAPY_CPP_CHECK(capy_text_sequence_append_actualtext_start(*this, actual_text, -1));
    }

    void append_actualtext_end() {
        CAPY_CPP_CHECK(capy_text_sequence_append_actualtext_end(*this));
    }

    void append_raw_glyph(uint32_t glyph_id, uint32_t codepoint) {
        CAPY_CPP_CHECK(capy_text_sequence_append_raw_glyph(*this, glyph_id, codepoint));
    }

    void append_ligature_glyph(uint32_t glyph_id, const char *original_text) {
        CAPY_CPP_CHECK(
            capy_text_sequence_append_ligature_glyph(*this, glyph_id, original_text, -1));
    }
};

class Text : public CapyC<CapyPDF_Text> {
    friend class DrawContext;

public:
    Text() = delete;

    void cmd_Tj(const char *utf8_text, int32_t strsize = -1) {
        CAPY_CPP_CHECK(capy_text_cmd_Tj(*this, utf8_text, strsize));
    }
    template<ByteSequence T> void cmd_Tj(const T &text) { cmd_Tj(text.data(), text.size()); }

    void cmd_BDC(CapyPDF_StructureItemId sid) {
        CAPY_CPP_CHECK(capy_text_cmd_BDC_builtin(*this, sid));
    }

    void cmd_Tc(double spacing) { CAPY_CPP_CHECK(capy_text_cmd_Tc(*this, spacing)); }

    void cmd_Td(double x, double y) { CAPY_CPP_CHECK(capy_text_cmd_Td(*this, x, y)); }

    void cmd_TD(double x, double y) { CAPY_CPP_CHECK(capy_text_cmd_TD(*this, x, y)); }

    void cmd_Tf(CapyPDF_FontId font, double pointsize) {
        CAPY_CPP_CHECK(capy_text_cmd_Tf(*this, font, pointsize));
    }

    void cmd_TJ(TextSequence &sequence) { CAPY_CPP_CHECK(capy_text_cmd_TJ(*this, sequence)); }

    void cmd_TL(double leading) { CAPY_CPP_CHECK(capy_text_cmd_TL(*this, leading)); }

    void cmd_Tm(double a, double b, double c, double d, double e, double f) {
        CAPY_CPP_CHECK(capy_text_cmd_Tm(*this, a, b, c, d, e, f));
    }

    void cmd_Tr(CapyPDF_Text_Mode tmode) { CAPY_CPP_CHECK(capy_text_cmd_Tr(*this, tmode)); }

    void cmd_Tstar() { CAPY_CPP_CHECK(capy_text_cmd_Tstar(*this)); }

    void cmd_EMC() { CAPY_CPP_CHECK(capy_text_cmd_EMC(*this)); }

    void set_nonstroke(Color &color) { CAPY_CPP_CHECK(capy_text_set_nonstroke(*this, color)); }
    void set_stroke(Color &color) { CAPY_CPP_CHECK(capy_text_set_stroke(*this, color)); }
    void cmd_w(double value) { CAPY_CPP_CHECK(capy_text_cmd_w(*this, value)); }
    void cmd_M(double value) { CAPY_CPP_CHECK(capy_text_cmd_M(*this, value)); }
    void cmd_j(CapyPDF_Line_Join value) { CAPY_CPP_CHECK(capy_text_cmd_j(*this, value)); }
    void cmd_J(CapyPDF_Line_Cap value) { CAPY_CPP_CHECK(capy_text_cmd_J(*this, value)); }
    void cmd_d(double *values, int32_t num_values, double offset) {
        CAPY_CPP_CHECK(capy_text_cmd_d(*this, values, num_values, offset));
    }
    void cmd_gs(CapyPDF_GraphicsStateId gsid) { CAPY_CPP_CHECK(capy_text_cmd_gs(*this, gsid)); }

private:
    explicit Text(CapyPDF_Text *text) { _d.reset(text); }
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
    void set_SMask(CapyPDF_SoftMaskId value) {
        CAPY_CPP_CHECK(capy_graphics_state_set_SMask(*this, value));
    }
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

    Type2Shading(CapyPDF_Device_Colorspace cs,
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

    Type3Shading(CapyPDF_Device_Colorspace cs,
                 double *coords,
                 uint32_t num_coords,
                 CapyPDF_FunctionId func) {
        if(num_coords != 6) {
            CAPY_ERROR_HAPPENED("Coords array must hold exactly 6 doubles");
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

    Type4Shading(CapyPDF_Device_Colorspace cs, double minx, double miny, double maxx, double maxy) {
        CapyPDF_Shading *sd;
        CAPY_CPP_CHECK(capy_type4_shading_new(cs, minx, miny, maxx, maxy, &sd));
        _d.reset(sd);
    }
    void add_triangle(double *coords, uint32_t num_coords, Color *colors, uint32_t num_colors) {
        if(num_coords != 6) {
            CAPY_ERROR_HAPPENED("Coords must have exactly 6 doubles.");
        }
        if(num_colors != 3) {
            CAPY_ERROR_HAPPENED("Triangle patch must have exactly 3 colors");
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
            CAPY_ERROR_HAPPENED("Bad flag value");
        }
        if(num_coords != 2) {
            CAPY_ERROR_HAPPENED("Coords must have exactly 2 doubles");
        }
        CAPY_CPP_CHECK(capy_type4_shading_extend(*this, flag, coords, color));
    }
};

class Type6Shading : public CapyC<CapyPDF_Shading> {
public:
    friend class Generator;

    Type6Shading(CapyPDF_Device_Colorspace cs, double minx, double miny, double maxx, double maxy) {
        CapyPDF_Shading *sd;
        CAPY_CPP_CHECK(capy_type6_shading_new(cs, minx, miny, maxx, maxy, &sd));
        _d.reset(sd);
    }
    void add_patch(double *coords, uint32_t num_coords, Color *colors, uint32_t num_colors) {
        if(num_coords != 24) {
            CAPY_ERROR_HAPPENED("Coords must have exactly 24 doubles.");
        }
        if(num_colors != 4) {
            CAPY_ERROR_HAPPENED("Shading patch must have exactly 4 colors");
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
            CAPY_ERROR_HAPPENED("Bad flag value");
        }
        if(num_coords != 16) {
            CAPY_ERROR_HAPPENED("Coords must have exactly 2 doubles");
        }
        if(num_colors != 2) {
            CAPY_ERROR_HAPPENED("Shading extension must have exactly 2 colors");
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

    void annotate(CapyPDF_AnnotationId aid) { CAPY_CPP_CHECK(capy_dc_annotate(*this, aid)); }

    void cmd_b() { CAPY_CPP_CHECK(capy_dc_cmd_b(*this)); }
    void cmd_B() { CAPY_CPP_CHECK(capy_dc_cmd_B(*this)); }
    void cmd_bstar() { CAPY_CPP_CHECK(capy_dc_cmd_bstar(*this)); }
    void cmd_Bstar() { CAPY_CPP_CHECK(capy_dc_cmd_Bstar(*this)); }

    void cmd_BDC(CapyPDF_OptionalContentGroupId ocgid) {
        CAPY_CPP_CHECK(capy_dc_cmd_BDC_ocg(*this, ocgid));
    }
    void cmd_BDC(CapyPDF_StructureItemId sid, const BDCTags *tags = nullptr) {
        if(tags) {
            CAPY_CPP_CHECK(capy_dc_cmd_BDC_builtin(*this, sid, *tags));
        } else {
            CAPY_CPP_CHECK(capy_dc_cmd_BDC_builtin(*this, sid, nullptr));
        }
    }

    void cmd_BDC_testing(const char *name, int32_t namelen, BDCTags *tags) {
        if(tags) {
            CAPY_CPP_CHECK(capy_dc_cmd_BDC_testing(*this, name, namelen, *tags));
        } else {
            CAPY_CPP_CHECK(capy_dc_cmd_BDC_testing(*this, name, namelen, nullptr));
        }
    }

    void cmd_c(double x1, double y1, double x2, double y2, double x3, double y3) {
        CAPY_CPP_CHECK(capy_dc_cmd_c(*this, x1, y1, x2, y2, x3, y3));
    }
    void cmd_cm(double a, double b, double c, double d, double e, double f) {
        CAPY_CPP_CHECK(capy_dc_cmd_cm(*this, a, b, c, d, e, f));
    }
    void cmd_Do(CapyPDF_TransparencyGroupId tgid) {
        CAPY_CPP_CHECK(capy_dc_cmd_Do_trgroup(*this, tgid));
    }
    void cmd_Do(CapyPDF_ImageId iid) { CAPY_CPP_CHECK(capy_dc_cmd_Do_image(*this, iid)); }
    void cmd_EMC() { CAPY_CPP_CHECK(capy_dc_cmd_EMC(*this)); }
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
    void cmd_n() { CAPY_CPP_CHECK(capy_dc_cmd_n(*this)); }
    void cmd_M(double value) { CAPY_CPP_CHECK(capy_dc_cmd_M(*this, value)); }
    void cmd_j(CapyPDF_Line_Join value) { CAPY_CPP_CHECK(capy_dc_cmd_j(*this, value)); }
    void cmd_J(CapyPDF_Line_Cap value) { CAPY_CPP_CHECK(capy_dc_cmd_J(*this, value)); }
    void cmd_d(double *values, int32_t num_values, double offset) {
        CAPY_CPP_CHECK(capy_dc_cmd_d(*this, values, num_values, offset));
    }
    void cmd_gs(CapyPDF_GraphicsStateId gsid) { CAPY_CPP_CHECK(capy_dc_cmd_gs(*this, gsid)); }

    void cmd_s() { CAPY_CPP_CHECK(capy_dc_cmd_s(*this)); }
    void cmd_S() { CAPY_CPP_CHECK(capy_dc_cmd_S(*this)); }
    void cmd_W() { CAPY_CPP_CHECK(capy_dc_cmd_W(*this)); }
    void cmd_Wstar() { CAPY_CPP_CHECK(capy_dc_cmd_Wstar(*this)); }

    [[deprecated]] void draw_image(CapyPDF_ImageId iid) { return cmd_Do(iid); }

    void set_custom_page_properties(PageProperties const &props) {
        CAPY_CPP_CHECK(capy_dc_set_custom_page_properties(*this, props));
    }

    void set_transparency_group_properties(TransparencyGroupProperties &trprop) {
        CAPY_CPP_CHECK(capy_dc_set_transparency_group_properties(*this, trprop));
    }

    void set_group_matrix(double a, double b, double c, double d, double e, double f) {
        CAPY_CPP_CHECK(capy_dc_set_group_matrix(*this, a, b, c, d, e, f));
    }

    void render_text(const char *text,
                     int32_t strsize,
                     CapyPDF_FontId fid,
                     double point_size,
                     double x,
                     double y) {
        CAPY_CPP_CHECK(capy_dc_render_text(*this, text, strsize, fid, point_size, x, y));
    }
    template<ByteSequence T>
    void render_text(const T &buf, CapyPDF_FontId fid, double point_size, double x, double y) {
        render_text(buf.data(), buf.size(), fid, point_size, x, y);
    }

    Text text_new() {
        CapyPDF_Text *text;
        CAPY_CPP_CHECK(capy_dc_text_new(*this, &text));
        return Text(text);
    }

    void render_text_obj(Text &text) { CAPY_CPP_CHECK(capy_dc_render_text_obj(*this, text)); }

private:
    explicit DrawContext(CapyPDF_DrawContext *dc) { _d.reset(dc); }
};

class RasterImage : public CapyC<CapyPDF_RasterImage> {
    friend class Generator;
    friend class RasterImageBuilder;

public:
    RasterImage() = delete;

    std::pair<uint32_t, uint32_t> get_size() {
        uint32_t h, w = 0;
        CAPY_CPP_CHECK(capy_raster_image_get_size(*this, &w, &h));
        return {h, w};
    }

    CapyPDF_Image_Colorspace get_colorspace() {
        CapyPDF_Image_Colorspace out;
        CAPY_CPP_CHECK(capy_raster_image_get_colorspace(*this, &out));
        return out;
    }

    bool has_profile() {
        int32_t out = 0;
        CAPY_CPP_CHECK(capy_raster_image_has_profile(*this, &out));
        return out != 0;
    }

private:
    RasterImage(CapyPDF_RasterImage *ri) { _d.reset(ri); }
};

class ImagePdfProperties : public CapyC<CapyPDF_ImagePdfProperties> {
public:
    friend class Generator;

    ImagePdfProperties() {
        CapyPDF_ImagePdfProperties *props;
        CAPY_CPP_CHECK(capy_image_pdf_properties_new(&props));
        _d.reset(props);
    }

    void set_mask(bool as_mask) {
        CAPY_CPP_CHECK(capy_image_pdf_properties_set_mask(*this, as_mask));
    }
    void set_interpolate(CapyPDF_Image_Interpolation interp) {
        CAPY_CPP_CHECK(capy_image_pdf_properties_set_interpolate(*this, interp));
    }
    void set_conversion_intent(CapyPDF_Rendering_Intent intent) {
        CAPY_CPP_CHECK(capy_image_pdf_properties_set_conversion_intent(*this, intent));
    }
};

class RasterImageBuilder : public CapyC<CapyPDF_RasterImageBuilder> {
public:
    RasterImageBuilder() {
        CapyPDF_RasterImageBuilder *rib;
        CAPY_CPP_CHECK(capy_raster_image_builder_new(&rib));
        _d.reset(rib);
    }

    void set_size(uint32_t w, uint32_t h) {
        CAPY_CPP_CHECK(capy_raster_image_builder_set_size(*this, w, h));
    }
    void set_pixel_depth(uint32_t depth) {
        CAPY_CPP_CHECK(capy_raster_image_builder_set_pixel_depth(*this, depth));
    }
    void set_pixel_data(const char *buf, uint64_t bufsize) {
        CAPY_CPP_CHECK(capy_raster_image_builder_set_pixel_data(*this, buf, bufsize));
    }
    void set_alpha_depth(uint32_t depth) {
        CAPY_CPP_CHECK(capy_raster_image_builder_set_alpha_depth(*this, depth));
    }
    void set_alpha_data(const char *buf, uint64_t bufsize) {
        CAPY_CPP_CHECK(capy_raster_image_builder_set_alpha_data(*this, buf, bufsize));
    }
    void set_input_data_compression_format(CapyPDF_Compression compression) {
        CAPY_CPP_CHECK(
            capy_raster_image_builder_set_input_data_compression_format(*this, compression));
    }
    void set_colorspace(CapyPDF_Image_Colorspace colorspace) {
        CAPY_CPP_CHECK(capy_raster_image_builder_set_colorspace(*this, colorspace));
    }

    RasterImage build() {
        CapyPDF_RasterImage *im;
        CAPY_CPP_CHECK(capy_raster_image_builder_build(*this, &im));
        return RasterImage(im);
    }
};

class SoftMask : public CapyC<CapyPDF_SoftMask> {
public:
    friend class Generator;

    SoftMask(CapyPDF_Soft_Mask_Subtype type, CapyPDF_TransparencyGroupId tgid) {
        CapyPDF_SoftMask *sm;
        CAPY_CPP_CHECK(capy_soft_mask_new(type, tgid, &sm));
        _d.reset(sm);
    }
};

class Generator : public CapyC<CapyPDF_Generator> {
public:
    Generator(const char *filename, const DocumentProperties &md) {
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

    CapyPDF_AnnotationId add_annotation(Annotation &annot) {
        CapyPDF_AnnotationId aid;
        CAPY_CPP_CHECK(capy_generator_add_annotation(*this, annot, &aid));
        return aid;
    }

    void add_page_labeling(uint32_t start_page,
                           std::optional<CapyPDF_Page_Label_Number_Style> style,
                           const char *prefix,
                           int32_t strsize,
                           std::optional<uint32_t> page_num) {
        CAPY_CPP_CHECK(capy_generator_add_page_labeling(*this,
                                                        start_page,
                                                        style ? &*style : nullptr,
                                                        prefix,
                                                        strsize,
                                                        page_num ? &(*page_num) : nullptr));
    }
    template<ByteSequence T>
    void add_page_labeling(uint32_t start_page,
                           std::optional<CapyPDF_Page_Label_Number_Style> style,
                           const T &bseq,
                           std::optional<uint32_t> page_num) {
        add_page_labeling(start_page, style, bseq.data(), bseq.size(), page_num);
    }

    void add_page(DrawContext &dc){CAPY_CPP_CHECK(capy_generator_add_page(*this, dc))}

    CapyPDF_StructureItemId add_structure_item(const CapyPDF_Structure_Type stype,
                                               const CapyPDF_StructureItemId *parent,
                                               CapyPDF_StructItemExtraData *extra) {
        CapyPDF_StructureItemId sid;
        CAPY_CPP_CHECK(capy_generator_add_structure_item(*this, stype, parent, extra, &sid));
        return sid;
    }

    CapyPDF_FontId load_font(const char *fname) {
        CapyPDF_FontId fid;
        CAPY_CPP_CHECK(capy_generator_load_font(*this, fname, nullptr, &fid));
        return fid;
    }

    CapyPDF_FontId load_font(const char *fname, FontProperties &fprop) {
        CapyPDF_FontId fid;
        CAPY_CPP_CHECK(capy_generator_load_font(*this, fname, fprop, &fid));
        return fid;
    }

    CapyPDF_IccColorSpaceId load_icc_profile(const char *fname) {
        CapyPDF_IccColorSpaceId cpid;
        CAPY_CPP_CHECK(capy_generator_load_icc_profile(*this, fname, &cpid));
        return cpid;
    }

    CapyPDF_IccColorSpaceId
    add_icc_profile(const char *bytes, uint64_t bufsize, int32_t num_channels) {
        CapyPDF_IccColorSpaceId cpid;
        CAPY_CPP_CHECK(capy_generator_add_icc_profile(*this, bytes, bufsize, num_channels, &cpid));
        return cpid;
    }
    template<ByteSequence T>
    CapyPDF_IccColorSpaceId add_icc_profile(const T &buf, int32_t num_channels) {
        return add_icc_profile(buf.data(), buf.size(), num_channels);
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

    RasterImage load_image_from_memory(const char *buf, int64_t bufsize) {
        CapyPDF_RasterImage *im;
        CAPY_CPP_CHECK(capy_generator_load_image_from_memory(*this, buf, bufsize, &im));
        return RasterImage(im);
    }

    CapyPDF_ImageId add_image(RasterImage &image, ImagePdfProperties const &props) {
        CapyPDF_ImageId imid;
        CAPY_CPP_CHECK(capy_generator_add_image(*this, image, props, &imid));
        return imid;
    }

    CapyPDF_GraphicsStateId add_graphics_state(GraphicsState const &gstate) {
        CapyPDF_GraphicsStateId gsid;
        CAPY_CPP_CHECK(capy_generator_add_graphics_state(*this, gstate, &gsid));
        return gsid;
    }

    CapyPDF_OptionalContentGroupId add_optional_content_group(OptionalContentGroup const &ocg) {
        CapyPDF_OptionalContentGroupId ocgid;
        CAPY_CPP_CHECK(capy_generator_add_optional_content_group(*this, ocg, &ocgid));
        return ocgid;
    }

    CapyPDF_TransparencyGroupId add_transparency_group(DrawContext &dct) {
        CapyPDF_TransparencyGroupId tgid;
        CAPY_CPP_CHECK(capy_generator_add_transparency_group(*this, dct, &tgid));
        return tgid;
    }

    CapyPDF_SoftMaskId add_soft_mask(SoftMask &sm) {
        CapyPDF_SoftMaskId smid;
        CAPY_CPP_CHECK(capy_generator_add_soft_mask(*this, sm, &smid));
        return smid;
    }

    double text_width(const char *u8txt, int32_t strsize, CapyPDF_FontId font, double pointsize) {
        double result;
        CAPY_CPP_CHECK(capy_generator_text_width(*this, u8txt, strsize, font, pointsize, &result));
        return result;
    }
    template<ByteSequence T>
    double text_width(const T &str, CapyPDF_FontId font, double pointsize) {
        return text_width(str.data(), str.size(), font, pointsize);
    }
    void write() { CAPY_CPP_CHECK(capy_generator_write(*this)); }
};

} // namespace capypdf
