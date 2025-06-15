// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <capypdf.h>
#include <string.h>
#include <generator.hpp>
#include <drawcontext.hpp>
#include <errorhandling.hpp>

#define RETNOERR return conv_err(ErrorCode::NoError)

#define CHECK_NULL(x)                                                                              \
    if(x == nullptr) {                                                                             \
        return conv_err(ErrorCode::ArgIsNull);                                                     \
    }

#define CHECK_BOOLEAN(b)                                                                           \
    if(b < 0 || b > 1) {                                                                           \
        return conv_err(ErrorCode::BadBoolean);                                                    \
    }

using namespace capypdf::internal;

namespace {

[[nodiscard]] CapyPDF_EC conv_err(ErrorCode ec) { return (CapyPDF_EC)ec; }

template<typename T> [[nodiscard]] CapyPDF_EC conv_err(const rvoe<T> &rc) {
    return (CapyPDF_EC)(rc ? ErrorCode::NoError : rc.error());
}

rvoe<asciistring> validate_ascii(const char *buf, int32_t strsize) {
    if(!buf) {
        RETERR(ArgIsNull);
    }
    if(strsize < -1) {
        RETERR(InvalidBufsize);
    }
    if(strsize == -1) {
        return asciistring::from_cstr(buf);
    } else {
        return asciistring::from_view(buf, strsize);
    }
}

rvoe<u8string> validate_utf8(const char *buf, int32_t strsize) {
    if(!buf) {
        RETERR(ArgIsNull);
    }
    if(strsize < -1) {
        RETERR(InvalidBufsize);
    }
    if(strsize == -1) {
        return u8string::from_cstr(buf);
    } else {
        return u8string::from_view(buf, strsize);
    }
}

#if defined(__cpp_exceptions)
#define API_BOUNDARY_START try {
#define API_BOUNDARY_END                                                                           \
    }                                                                                              \
    catch(...) {                                                                                   \
        return handle_exception();                                                                 \
    }

#ifdef _MSC_VER
__declspec(noinline)
#else
__attribute__((noinline))
#endif
CapyPDF_EC
handle_exception() {
    try {
        throw;
    } catch(ErrorCode ec) {
        return conv_err(ec);
    } catch(const std::exception &e) {
        fprintf(stderr, "%s\n", e.what());
        return conv_err(ErrorCode::DynamicError);
    } catch(const pystd2025::PyException &e) {
        fprintf(stderr, "%s\n", e.what().c_str());
        return conv_err(ErrorCode::DynamicError);
    } catch(const char *msg) {
        fprintf(stderr, "%s\n", msg);
        return conv_err(ErrorCode::DynamicError);
    } catch(...) {
        fprintf(stderr, "An error of an unknown type occurred.\n");
        return conv_err(ErrorCode::DynamicError);
    }
    fprintf(stderr,
            "Unreachable code reached. You might want to verify that the laws of physics still "
            "apply in your universe\n");
    return conv_err(ErrorCode::DynamicError);
}

#else
#define API_BOUNDARY_START
#define API_BOUNDARY_END
#endif

ShadingPoint conv_shpoint(const double *coords, const Color *color) {
    ShadingPoint sp;
    sp.c = *color;
    sp.p.x = coords[0];
    sp.p.y = coords[1];
    return sp;
}

template<typename T> void grab_coons_data(T &patch, const double *coords, const Color **colors) {
    constexpr auto p_count = sizeof(patch.p) / sizeof(patch.p[0]);
    constexpr auto c_count = sizeof(patch.c) / sizeof(patch.c[0]);
    for(size_t i = 0; i < p_count; ++i) {
        patch.p[i].x = coords[2 * i];
        patch.p[i].y = coords[2 * i + 1];
    }
    for(size_t i = 0; i < c_count; ++i) {
        patch.c[i] = *colors[i];
    }
}

} // namespace

struct RasterImageBuilder {
    RawPixelImage i;
};

CapyPDF_EC capy_document_properties_new(CapyPDF_DocumentProperties **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_DocumentProperties *>(new DocumentProperties());
    RETNOERR;
    API_BOUNDARY_END;
}

CapyPDF_EC capy_document_properties_destroy(CapyPDF_DocumentProperties *docprops) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<DocumentProperties *>(docprops);
    RETNOERR;
    API_BOUNDARY_END;
}

CapyPDF_EC capy_document_properties_set_title(CapyPDF_DocumentProperties *docprops,
                                              const char *utf8_title,
                                              int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto rc = validate_utf8(utf8_title, strsize);
    if(rc) {
        reinterpret_cast<DocumentProperties *>(docprops)->title = std::move(rc.value());
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_author(CapyPDF_DocumentProperties *docprops,
                                                              const char *utf8_author,
                                                              int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto rc = validate_utf8(utf8_author, strsize);
    if(rc) {
        reinterpret_cast<DocumentProperties *>(docprops)->author = std::move(rc.value());
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_creator(CapyPDF_DocumentProperties *docprops,
                                                               const char *utf8_creator,
                                                               int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto rc = validate_utf8(utf8_creator, strsize);
    if(rc) {
        reinterpret_cast<DocumentProperties *>(docprops)->creator = std::move(rc.value());
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_language(
    CapyPDF_DocumentProperties *docprops, const char *lang, int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto rc = validate_ascii(lang, strsize);
    if(rc) {
        reinterpret_cast<DocumentProperties *>(docprops)->lang = std::move(rc.value());
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_page_properties_new(CapyPDF_PageProperties **out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_PageProperties *>(new PageProperties);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_page_properties_destroy(CapyPDF_PageProperties *pageprops)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<PageProperties *>(pageprops);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_page_properties_set_pagebox(CapyPDF_PageProperties *pageprops,
                                                           CapyPDF_Page_Box boxtype,
                                                           double x1,
                                                           double y1,
                                                           double x2,
                                                           double y2) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto props = reinterpret_cast<PageProperties *>(pageprops);
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
        return (CapyPDF_EC)ErrorCode::BadEnum;
    }

    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_page_properties_set_transparency_group_properties(
    CapyPDF_PageProperties *pageprops,
    CapyPDF_TransparencyGroupProperties *trprop) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *page = reinterpret_cast<PageProperties *>(pageprops);
    auto *tr = reinterpret_cast<TransparencyGroupProperties *>(trprop);
    page->transparency_props = *tr;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_document_properties_set_device_profile(CapyPDF_DocumentProperties *docprops,
                                            CapyPDF_Device_Colorspace cs,
                                            const char *profile_path) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dp = reinterpret_cast<DocumentProperties *>(docprops);
    switch(cs) {
    case CAPY_DEVICE_CS_RGB:
        dp->prof.rgb_profile_file = profile_path;
        break;
    case CAPY_DEVICE_CS_GRAY:
        dp->prof.gray_profile_file = profile_path;
        break;
    case CAPY_DEVICE_CS_CMYK:
        dp->prof.cmyk_profile_file = profile_path;
        break;
    }
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_colorspace(
    CapyPDF_DocumentProperties *docprops, CapyPDF_Device_Colorspace cs) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dp = reinterpret_cast<DocumentProperties *>(docprops);
    dp->output_colorspace = cs;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_document_properties_set_output_intent(CapyPDF_DocumentProperties *docprops,
                                           const char *identifier,
                                           int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dp = reinterpret_cast<DocumentProperties *>(docprops);
    auto rc = validate_utf8(identifier, strsize);
    if(!rc) {
        return conv_err(rc.error());
    }
    dp->intent_condition_identifier = std::move(rc.value());
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_pdfx(
    CapyPDF_DocumentProperties *docprops, CapyPDF_PDFX_Type xtype) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dp = reinterpret_cast<DocumentProperties *>(docprops);
    dp->subtype = pystd2025::move(xtype);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_pdfa(
    CapyPDF_DocumentProperties *docprops, CapyPDF_PDFA_Type atype) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dp = reinterpret_cast<DocumentProperties *>(docprops);
    dp->subtype = pystd2025::move(atype);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_default_page_properties(
    CapyPDF_DocumentProperties *docprops, const CapyPDF_PageProperties *prop) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dp = reinterpret_cast<DocumentProperties *>(docprops);
    auto props = reinterpret_cast<const PageProperties *>(prop);
    if(!props->mediabox) {
        return conv_err(ErrorCode::MissingMediabox);
    }
    dp->default_page_properties = *props;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_tagged(CapyPDF_DocumentProperties *docprops,
                                                              int32_t is_tagged) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    CHECK_BOOLEAN(is_tagged);
    auto dp = reinterpret_cast<DocumentProperties *>(docprops);
    dp->is_tagged = is_tagged;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_metadata_xml(
    CapyPDF_DocumentProperties *docprops, const char *rdf_xml, int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dp = reinterpret_cast<DocumentProperties *>(docprops);
    auto rx = validate_utf8(rdf_xml, strsize);
    if(!rx) {
        return conv_err(rx.error());
    }
    dp->metadata_xml = std::move(rx.value());
    RETNOERR;
    API_BOUNDARY_END;
}

CapyPDF_EC capy_generator_new(const char *filename,
                              const CapyPDF_DocumentProperties *docprops,
                              CapyPDF_Generator **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    CHECK_NULL(filename);
    CHECK_NULL(docprops);
    CHECK_NULL(out_ptr);
    auto metadata = reinterpret_cast<const DocumentProperties *>(docprops);
    auto rc = PdfGen::construct(filename, *metadata);
    if(rc) {
        *out_ptr = reinterpret_cast<CapyPDF_Generator *>(rc.value().release());
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CapyPDF_EC capy_generator_add_page(CapyPDF_Generator *gen,
                                   CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);

    auto rc = g->add_page(*dc);
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_page_labeling(CapyPDF_Generator *gen,
                                                           uint32_t start_page,
                                                           CapyPDF_Page_Label_Number_Style *style,
                                                           const char *prefix,
                                                           int32_t strsize,
                                                           uint32_t *start_num) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    pystd2025::Optional<CapyPDF_Page_Label_Number_Style> opt_style;
    pystd2025::Optional<u8string> opt_prefix;
    pystd2025::Optional<uint32_t> opt_start_num;
    if(style) {
        opt_style = *style;
    }
    if(prefix) {
        auto u8_prefix = validate_utf8(prefix, strsize);
        if(!u8_prefix) {
            return conv_err(u8_prefix);
        }
        opt_prefix = std::move(*u8_prefix);
    }
    if(start_num) {
        opt_start_num = *start_num;
    }
    return conv_err(g->add_page_labeling(start_page, opt_style, opt_prefix, opt_start_num));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_form_xobject(CapyPDF_Generator *gen,
                                                          CapyPDF_DrawContext *ctx,
                                                          CapyPDF_FormXObjectId *out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);

    auto rc = g->add_form_xobject(*dc);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_generator_add_transparency_group(CapyPDF_Generator *gen,
                                      CapyPDF_DrawContext *ctx,
                                      CapyPDF_TransparencyGroupId *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);

    auto rc = g->add_transparency_group(*dc);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_soft_mask(CapyPDF_Generator *gen,
                                                       const CapyPDF_SoftMask *sm,
                                                       CapyPDF_SoftMaskId *out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *s = reinterpret_cast<const SoftMask *>(sm);

    auto rc = g->add_soft_mask(*s);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_shading_pattern(CapyPDF_Generator *gen,
                                                             CapyPDF_ShadingPattern *shp,
                                                             CapyPDF_PatternId *out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *shad = reinterpret_cast<ShadingPattern *>(shp);
    auto rc = g->add_shading_pattern(*shad);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_tiling_pattern(
    CapyPDF_Generator *gen, CapyPDF_DrawContext *ctx, CapyPDF_PatternId *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *colordc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto rc = g->add_tiling_pattern(*colordc);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_embed_file(CapyPDF_Generator *gen,
                                                    CapyPDF_EmbeddedFile *efile,
                                                    CapyPDF_EmbeddedFileId *out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *ef = reinterpret_cast<EmbeddedFile *>(efile);
    auto rc = g->embed_file(*ef);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_load_font(CapyPDF_Generator *gen,
                                                   const char *fname,
                                                   CapyPDF_FontProperties *fprop,
                                                   CapyPDF_FontId *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto rc = g->load_font(fname,
                           fprop ? *(reinterpret_cast<FontProperties *>(fprop)) : FontProperties());
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_load_image(
    CapyPDF_Generator *gen, const char *fname, CapyPDF_RasterImage **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    pystd2025::Path f(fname);
    auto rc = g->load_image(f);
    if(rc) {
        auto *result = new RasterImage(std::move(rc.value()));
        *out_ptr = reinterpret_cast<CapyPDF_RasterImage *>(result);
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_load_image_from_memory(CapyPDF_Generator *gen,
                                                                const char *buf,
                                                                int64_t bufsize,
                                                                CapyPDF_RasterImage **out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto rc = g->load_image(buf, bufsize);
    if(rc) {
        auto *result = new RasterImage(std::move(rc.value()));
        *out_ptr = reinterpret_cast<CapyPDF_RasterImage *>(result);
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_convert_image(CapyPDF_Generator *gen,
                                                       const CapyPDF_RasterImage *source,
                                                       CapyPDF_Device_Colorspace output_cs,
                                                       CapyPDF_Rendering_Intent ri,
                                                       CapyPDF_RasterImage **out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *image = reinterpret_cast<const RasterImage *>(source);
    if(auto *raw = std::get_if<RawPixelImage>(image)) {
        auto rc = g->convert_image_to_cs(*raw, output_cs, ri);
        if(rc) {
            *out_ptr =
                reinterpret_cast<CapyPDF_RasterImage *>(new RasterImage(std::move(rc.value())));
        }
        return conv_err(rc);
    } else {
        return conv_err(ErrorCode::UnsupportedFormat);
    }
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_image(CapyPDF_Generator *gen,
                                                   CapyPDF_RasterImage *image,
                                                   const CapyPDF_ImagePdfProperties *params,
                                                   CapyPDF_ImageId *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *im = reinterpret_cast<RasterImage *>(image);
    auto *par = reinterpret_cast<const ImagePDFProperties *>(params);
    auto rc = g->add_image(std::move(*im), *par);
    if(rc) {
        *out_ptr = rc.value();
    }
    *im = RasterImage{};
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_function(
    CapyPDF_Generator *gen, CapyPDF_Function *func, CapyPDF_FunctionId *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *f = reinterpret_cast<PdfFunction *>(func);
    auto rc = g->add_function(*f);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_shading(CapyPDF_Generator *gen,
                                                     CapyPDF_Shading *shade,
                                                     CapyPDF_ShadingId *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *sh = reinterpret_cast<PdfShading *>(shade);
    auto rc = g->add_shading(*sh);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_graphics_state(CapyPDF_Generator *gen,
                                                            const CapyPDF_GraphicsState *state,
                                                            CapyPDF_GraphicsStateId *out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *s = reinterpret_cast<const GraphicsState *>(state);
    auto rc = g->add_graphics_state(*s);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_structure_item(CapyPDF_Generator *gen,
                                                            const CapyPDF_Structure_Type stype,
                                                            const CapyPDF_StructureItemId *parent,
                                                            CapyPDF_StructItemExtraData *extra,
                                                            CapyPDF_StructureItemId *out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    pystd2025::Optional<CapyPDF_StructureItemId> item_parent;
    if(parent) {
        item_parent = *parent;
    }
    pystd2025::Optional<StructItemExtraData> ed;
    if(extra) {
        ed = *reinterpret_cast<StructItemExtraData *>(extra);
    }
    auto rc = g->add_structure_item(stype, item_parent, std::move(ed));
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_generator_add_custom_structure_item(CapyPDF_Generator *gen,
                                         const CapyPDF_RoleId role,
                                         const CapyPDF_StructureItemId *parent,
                                         CapyPDF_StructItemExtraData *extra,
                                         CapyPDF_StructureItemId *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    pystd2025::Optional<CapyPDF_StructureItemId> item_parent;
    if(parent) {
        item_parent = *parent;
    }
    pystd2025::Optional<StructItemExtraData> ed;
    if(extra) {
        ed = *reinterpret_cast<StructItemExtraData *>(extra);
    }
    auto rc = g->add_structure_item(role, item_parent, std::move(ed));
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_load_icc_profile(
    CapyPDF_Generator *gen, const char *fname, CapyPDF_IccColorSpaceId *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto rc = g->load_icc_file(fname);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_icc_profile(CapyPDF_Generator *gen,
                                                         const char *buf,
                                                         uint64_t bufsize,
                                                         uint32_t num_channels,
                                                         CapyPDF_IccColorSpaceId *out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto rc = g->add_icc_profile({buf, bufsize}, num_channels);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_lab_colorspace(CapyPDF_Generator *gen,
                                                            double xw,
                                                            double yw,
                                                            double zw,
                                                            double amin,
                                                            double amax,
                                                            double bmin,
                                                            double bmax,
                                                            CapyPDF_LabColorSpaceId *out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    LabColorSpace cl;
    cl.xw = xw;
    cl.yw = yw;
    cl.zw = zw;

    cl.amin = amin;
    cl.amax = amax;
    cl.bmin = bmin;
    cl.bmax = bmax;
    auto rc = g->add_lab_colorspace(cl);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_separation(CapyPDF_Generator *gen,
                                                        const char *separation_name,
                                                        int32_t strsize,
                                                        CapyPDF_Device_Colorspace cs,
                                                        CapyPDF_FunctionId fid,
                                                        CapyPDF_SeparationId *out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto name = validate_ascii(separation_name, strsize);
    if(!name) {
        return conv_err(name);
    }
    auto rc = g->create_separation(name.value(), cs, fid);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CapyPDF_EC capy_generator_write(CapyPDF_Generator *gen) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto rc = g->write();
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_optional_content_group(
    CapyPDF_Generator *gen,
    const CapyPDF_OptionalContentGroup *ocg,
    CapyPDF_OptionalContentGroupId *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    const auto *group = reinterpret_cast<const OptionalContentGroup *>(ocg);
    auto rc = g->add_optional_content_group(*group);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_annotation(CapyPDF_Generator *gen,
                                                        CapyPDF_Annotation *annotation,
                                                        CapyPDF_AnnotationId *out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *a = reinterpret_cast<Annotation *>(annotation);
    auto rc = g->add_annotation(*a);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_rolemap_entry(CapyPDF_Generator *gen,
                                                           const char *name,
                                                           CapyPDF_Structure_Type builtin,
                                                           CapyPDF_RoleId *out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto rc = g->add_rolemap_entry(pystd2025::CString(name), builtin);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CapyPDF_EC capy_generator_destroy(CapyPDF_Generator *gen) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    delete g;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_outline(CapyPDF_Generator *gen,
                                                     const CapyPDF_Outline *outline,
                                                     CapyPDF_OutlineId *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *o = reinterpret_cast<const Outline *>(outline);

    auto rc = g->add_outline(*o);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_text_width(CapyPDF_Generator *gen,
                                                    const char *utf8_text,
                                                    int32_t strsize,
                                                    CapyPDF_FontId font,
                                                    double pointsize,
                                                    double *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto u8t = validate_utf8(utf8_text, strsize);
    if(!u8t) {
        return conv_err(u8t);
    }
    auto rc = g->utf8_text_width(u8t.value(), font, pointsize);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

// Draw Context

CapyPDF_EC capy_page_draw_context_new(CapyPDF_Generator *gen,
                                      CapyPDF_DrawContext **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    *out_ptr = reinterpret_cast<CapyPDF_DrawContext *>(g->new_page_draw_context());
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_b(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_b());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_B(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_B());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_bstar(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_bstar());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_Bstar(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_Bstar());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_BDC_builtin(CapyPDF_DrawContext *ctx,
                                                  CapyPDF_StructureItemId structid,
                                                  const CapyPDF_BDCTags *tags) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto t = reinterpret_cast<const BDCTags *>(tags);
    return conv_err(dc->cmd_BDC(structid, t));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_BDC_testing(CapyPDF_DrawContext *ctx,
                                                  const char *name,
                                                  int32_t namelen,
                                                  const CapyPDF_BDCTags *tags) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto t = reinterpret_cast<const BDCTags *>(tags);
    auto astr = validate_ascii(name, namelen);
    if(!astr) {
        return conv_err(astr.error());
    }
    return conv_err(dc->cmd_BDC(astr.value(), {}, t));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_BDC_ocg(
    CapyPDF_DrawContext *ctx, CapyPDF_OptionalContentGroupId ocgid) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_BDC(ocgid));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_BMC(CapyPDF_DrawContext *ctx,
                                          const char *tag) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_BMC(tag));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_c(CapyPDF_DrawContext *ctx,
                                        double x1,
                                        double y1,
                                        double x2,
                                        double y2,
                                        double x3,
                                        double y3) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_c(x1, y1, x2, y2, x3, y3));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_cm(CapyPDF_DrawContext *ctx,
                                         double m1,
                                         double m2,
                                         double m3,
                                         double m4,
                                         double m5,
                                         double m6) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_cm(m1, m2, m3, m4, m5, m6));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_d(CapyPDF_DrawContext *ctx,
                                        double *dash_array,
                                        int32_t array_size,
                                        double phase) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_d(dash_array, array_size, phase));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_Do_trgroup(
    CapyPDF_DrawContext *ctx, CapyPDF_TransparencyGroupId tgid) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_Do(tgid));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_Do_image(CapyPDF_DrawContext *ctx,
                                               CapyPDF_ImageId iid) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_Do(iid));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_EMC(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_EMC());
    API_BOUNDARY_END;
}

CapyPDF_EC capy_dc_cmd_f(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_f());
    API_BOUNDARY_END;
}

CapyPDF_EC capy_dc_cmd_fstar(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_fstar());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_G(CapyPDF_DrawContext *ctx, double gray) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_G(gray));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_g(CapyPDF_DrawContext *ctx, double gray) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_g(gray));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_gs(CapyPDF_DrawContext *ctx,
                                         CapyPDF_GraphicsStateId gsid) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_gs(gsid));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_h(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_h());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_i(CapyPDF_DrawContext *ctx,
                                        double flatness) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_i(flatness));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_j(CapyPDF_DrawContext *ctx,
                                        CapyPDF_Line_Join join_style) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_j(join_style));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_J(CapyPDF_DrawContext *ctx,
                                        CapyPDF_Line_Cap cap_style) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_J(cap_style));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_dc_cmd_k(CapyPDF_DrawContext *ctx, double c, double m, double y, double k) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_k(c, m, y, k));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_dc_cmd_K(CapyPDF_DrawContext *ctx, double c, double m, double y, double k) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_K(c, m, y, k));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_l(CapyPDF_DrawContext *ctx,
                                        double x,
                                        double y) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_l(x, y));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_m(CapyPDF_DrawContext *ctx,
                                        double x,
                                        double y) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_m(x, y));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_M(CapyPDF_DrawContext *ctx,
                                        double miterlimit) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_M(miterlimit));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_n(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_n());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_q(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_q());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_Q(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_Q());
    API_BOUNDARY_END;
}

CapyPDF_EC
capy_dc_cmd_re(CapyPDF_DrawContext *ctx, double x, double y, double w, double h) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_re(x, y, w, h));
    API_BOUNDARY_END;
}

CapyPDF_EC capy_dc_cmd_RG(CapyPDF_DrawContext *ctx, double r, double g, double b) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_RG(r, g, b));
    API_BOUNDARY_END;
}

CapyPDF_EC capy_dc_cmd_rg(CapyPDF_DrawContext *ctx, double r, double g, double b) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_rg(r, g, b));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_ri(CapyPDF_DrawContext *ctx,
                                         CapyPDF_Rendering_Intent ri) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_ri(ri));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_s(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_s());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_S(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_S());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_sh(CapyPDF_DrawContext *ctx,
                                         CapyPDF_ShadingId shid) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_sh(shid));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_v(
    CapyPDF_DrawContext *ctx, double x2, double y2, double x3, double y3) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_v(x2, y2, x3, y3));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_w(CapyPDF_DrawContext *ctx,
                                        double line_width) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_w(line_width));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_W(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_W());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_Wstar(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_Wstar());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_y(
    CapyPDF_DrawContext *ctx, double x1, double y1, double x3, double y3) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_y(x1, y1, x3, y3));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_set_stroke(CapyPDF_DrawContext *ctx,
                                             CapyPDF_Color *c) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto *color = reinterpret_cast<Color *>(c);
    return conv_err(dc->set_stroke_color(*color));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_set_nonstroke(CapyPDF_DrawContext *ctx,
                                                CapyPDF_Color *c) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto *color = reinterpret_cast<Color *>(c);
    return conv_err(dc->set_nonstroke_color(*color));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_render_text(CapyPDF_DrawContext *ctx,
                                              const char *text,
                                              int32_t strsize,
                                              CapyPDF_FontId fid,
                                              double point_size,
                                              double x,
                                              double y) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    auto utxt = validate_utf8(text, strsize);
    if(!utxt) {
        return conv_err(utxt);
    }
    return conv_err(c->render_text(utxt.value(), fid, point_size, x, y));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_render_text_obj(CapyPDF_DrawContext *ctx,
                                                  CapyPDF_Text *text) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    auto t = reinterpret_cast<PdfText *>(text);
    return conv_err(c->render_text(*t));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_set_page_transition(
    CapyPDF_DrawContext *ctx, CapyPDF_Transition *transition) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto t = reinterpret_cast<Transition *>(transition);
    auto rc = dc->set_transition(*t);
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_set_custom_page_properties(
    CapyPDF_DrawContext *ctx, const CapyPDF_PageProperties *custom_properties) {
    API_BOUNDARY_START;
    CHECK_NULL(custom_properties);
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto *cprop = reinterpret_cast<const PageProperties *>(custom_properties);
    return conv_err(dc->set_custom_page_properties(*cprop));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_annotate(CapyPDF_DrawContext *ctx,
                                           CapyPDF_AnnotationId aid) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->annotate(aid));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_set_transparency_group_properties(
    CapyPDF_DrawContext *ctx, CapyPDF_TransparencyGroupProperties *trprop) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto *tr = reinterpret_cast<TransparencyGroupProperties *>(trprop);
    return conv_err(dc->set_transparency_properties(*tr));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_set_group_matrix(CapyPDF_DrawContext *ctx,
                                                   double a,
                                                   double b,
                                                   double c,
                                                   double d,
                                                   double e,
                                                   double f) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    PdfMatrix m{a, b, c, d, e, f};
    return conv_err(dc->set_group_matrix(m));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_dc_add_simple_navigation(CapyPDF_DrawContext *ctx,
                              const CapyPDF_OptionalContentGroupId *ocgarray,
                              int32_t array_size,
                              const CapyPDF_Transition *tr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    pystd2025::Optional<Transition> transition;
    if(tr) {
        transition = *reinterpret_cast<const Transition *>(tr);
    }
    pystd2025::Span<const CapyPDF_OptionalContentGroupId> ocgspan(ocgarray, array_size);
    auto rc = dc->add_simple_navigation(ocgspan, transition);
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_text_new(CapyPDF_DrawContext *dc,
                                           CapyPDF_Text **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    CHECK_NULL(dc);
    *out_ptr = reinterpret_cast<CapyPDF_Text *>(
        new capypdf::internal::PdfText(reinterpret_cast<PdfDrawContext *>(dc)));
    RETNOERR;
    API_BOUNDARY_END;
}

CapyPDF_EC capy_dc_destroy(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<PdfDrawContext *>(ctx);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_form_xobject_new(CapyPDF_Generator *gen,
                                                double l,
                                                double b,
                                                double r,
                                                double t,
                                                CapyPDF_DrawContext **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto bbox = PdfRectangle::construct(l, b, r, t);
    if(!bbox) {
        return conv_err(bbox);
    }
    *out_ptr = reinterpret_cast<CapyPDF_DrawContext *>(g->new_form_xobject(bbox.value()));
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transparency_group_new(
    CapyPDF_Generator *gen, double l, double b, double r, double t, CapyPDF_DrawContext **out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto bbox = PdfRectangle::construct(l, b, r, t);
    if(!bbox) {
        return conv_err(bbox);
    }
    *out_ptr = reinterpret_cast<CapyPDF_DrawContext *>(g->new_transparency_group(bbox.value()));
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_tiling_pattern_context_new(CapyPDF_Generator *gen,
                                                          CapyPDF_DrawContext **out_ptr,
                                                          double l,
                                                          double b,
                                                          double r,
                                                          double t) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto bbox = PdfRectangle::construct(l, b, r, t);
    if(!bbox) {
        return conv_err(bbox);
    }
    *out_ptr = reinterpret_cast<CapyPDF_DrawContext *>(g->new_color_pattern(bbox.value()));
    RETNOERR;
    API_BOUNDARY_END;
}

// Text

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_new(CapyPDF_TextSequence **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_TextSequence *>(new TextSequence());
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_codepoint(CapyPDF_TextSequence *tseq,
                                                              uint32_t codepoint) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ts = reinterpret_cast<TextSequence *>(tseq);
    auto rc = ts->append_unicode(codepoint);
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_string(CapyPDF_TextSequence *tseq,
                                                           const char *u8str,
                                                           int32_t strlen) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ts = reinterpret_cast<TextSequence *>(tseq);
    auto u8 = validate_utf8(u8str, strlen);
    if(!u8) {
        return conv_err(u8);
    }
    auto rc = ts->append_string(std::move(u8.value()));
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_kerning(CapyPDF_TextSequence *tseq,
                                                            int32_t kern) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ts = reinterpret_cast<TextSequence *>(tseq);
    auto rc = ts->append_kerning(kern);
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_actualtext_start(
    CapyPDF_TextSequence *tseq, const char *actual_text, int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ts = reinterpret_cast<TextSequence *>(tseq);
    auto utxt = validate_utf8(actual_text, strsize);
    if(!utxt) {
        return conv_err(utxt);
    }

    auto rc = ts->append_actualtext_start(utxt.value());
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_actualtext_end(CapyPDF_TextSequence *tseq)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ts = reinterpret_cast<TextSequence *>(tseq);
    auto rc = ts->append_actualtext_end();
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_raw_glyph(CapyPDF_TextSequence *tseq,
                                                              uint32_t glyph_id,
                                                              uint32_t codepoint) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ts = reinterpret_cast<TextSequence *>(tseq);
    if(glyph_id == 0) {
        return conv_err(ErrorCode::MissingGlyph);
    }
    auto rc = ts->append_raw_glyph(glyph_id, codepoint);
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_ligature_glyph(CapyPDF_TextSequence *tseq,
                                                                   uint32_t glyph_id,
                                                                   const char *original_text,
                                                                   int32_t strsize)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ts = reinterpret_cast<TextSequence *>(tseq);
    if(glyph_id == 0) {
        return conv_err(ErrorCode::MissingGlyph);
    }
    auto txt = validate_utf8(original_text, strsize);
    if(!txt) {
        return conv_err(txt);
    }
    auto rc = ts->append_ligature_glyph(glyph_id, std::move(txt.value()));
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_destroy(CapyPDF_TextSequence *tseq) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<TextSequence *>(tseq);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Tj(CapyPDF_Text *text,
                                           const char *utf8_text,
                                           int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    auto txt = validate_utf8(utf8_text, strsize);
    if(!txt) {
        return conv_err(txt);
    }
    return conv_err(t->cmd_Tj(txt.value()));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_set_nonstroke(CapyPDF_Text *text,
                                                  const CapyPDF_Color *color) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    const auto *c = reinterpret_cast<const Color *>(color);
    return conv_err(t->nonstroke_color(*c));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_set_stroke(CapyPDF_Text *text,
                                               const CapyPDF_Color *color) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    const auto *c = reinterpret_cast<const Color *>(color);
    return conv_err(t->stroke_color(*c));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_w(CapyPDF_Text *text, double line_width) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_w(line_width));
    API_BOUNDARY_END;
}
CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_M(CapyPDF_Text *text, double miterlimit) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_M(miterlimit));
    API_BOUNDARY_END;
}
CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_j(CapyPDF_Text *text,
                                          CapyPDF_Line_Join join_style) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_j(join_style));
    API_BOUNDARY_END;
}
CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_J(CapyPDF_Text *text,
                                          CapyPDF_Line_Cap cap_style) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_J(cap_style));
    API_BOUNDARY_END;
}
CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_d(CapyPDF_Text *text,
                                          double *dash_array,
                                          int32_t array_size,
                                          double phase) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_d(dash_array, array_size, phase));
    API_BOUNDARY_END;
}
CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_gs(CapyPDF_Text *text,
                                           CapyPDF_GraphicsStateId gsid) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_gs(gsid));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_BDC_builtin(CapyPDF_Text *text,
                                                    CapyPDF_StructureItemId stid) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_BDC(stid));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_EMC(CapyPDF_Text *text) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    API_BOUNDARY_START;
    return conv_err(t->cmd_EMC());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Tc(CapyPDF_Text *text, double spacing) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tc(spacing));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Td(CapyPDF_Text *text,
                                           double x,
                                           double y) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Td(x, y));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_TD(CapyPDF_Text *text,
                                           double x,
                                           double y) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_TD(x, y));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Tf(CapyPDF_Text *text,
                                           CapyPDF_FontId font,
                                           double pointsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tf(font, pointsize));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_TJ(CapyPDF_Text *text,
                                           CapyPDF_TextSequence *kseq) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    auto *ks = reinterpret_cast<TextSequence *>(kseq);
    auto rc = t->cmd_TJ(*ks);
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_TL(CapyPDF_Text *text, double leading) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_TL(leading));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Tm(CapyPDF_Text *text,
                                           double a,
                                           double b,
                                           double c,
                                           double d,
                                           double e,
                                           double f) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tm(PdfMatrix{a, b, c, d, e, f}));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Tr(CapyPDF_Text *text,
                                           CapyPDF_Text_Mode tmode) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tr(tmode));
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Tstar(CapyPDF_Text *text) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tstar());
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_destroy(CapyPDF_Text *text) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<PdfText *>(text);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_new(CapyPDF_Color **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr =
        reinterpret_cast<CapyPDF_Color *>(new capypdf::internal::Color(DeviceRGBColor{0, 0, 0}));
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_destroy(CapyPDF_Color *color) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<capypdf::internal::Color *>(color);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_set_rgb(CapyPDF_Color *c, double r, double g, double b)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *reinterpret_cast<capypdf::internal::Color *>(c) = DeviceRGBColor{r, g, b};
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_set_gray(CapyPDF_Color *c, double v) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *reinterpret_cast<capypdf::internal::Color *>(c) = DeviceGrayColor{v};
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_color_set_cmyk(CapyPDF_Color *color, double c, double m, double y, double k) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *reinterpret_cast<capypdf::internal::Color *>(color) = DeviceCMYKColor{c, m, y, k};
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_set_icc(CapyPDF_Color *color,
                                             CapyPDF_IccColorSpaceId icc_id,
                                             double *values,
                                             int32_t num_values) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    ICCColor icc;
    icc.id = icc_id;
    icc.values.assign(values, values + num_values);
    *reinterpret_cast<capypdf::internal::Color *>(color) = std::move(icc);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_set_separation(CapyPDF_Color *color,
                                                    CapyPDF_SeparationId sep_id,
                                                    double value) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *c = reinterpret_cast<Color *>(color);
    if(value < 0 || value > 1.0) {
        return conv_err(ErrorCode::ColorOutOfRange);
    }
    *c = SeparationColor{sep_id, value};
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_set_pattern(CapyPDF_Color *color,
                                                 CapyPDF_PatternId pat_id) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *c = reinterpret_cast<Color *>(color);
    *c = pat_id;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_set_lab(CapyPDF_Color *color,
                                             CapyPDF_LabColorSpaceId lab_id,
                                             double l,
                                             double a,
                                             double b) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *c = reinterpret_cast<Color *>(color);
    *c = LabColor{lab_id, l, a, b};
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_new(CapyPDF_Transition **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto pt = new Transition{};
    *out_ptr = reinterpret_cast<CapyPDF_Transition *>(pt);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_set_S(CapyPDF_Transition *tr,
                                                CapyPDF_Transition_Type type) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->type = type;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_set_D(CapyPDF_Transition *tr,
                                                double duration) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->duration = duration;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_transition_set_Dm(CapyPDF_Transition *tr, CapyPDF_Transition_Dimension dim) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->Di = dim;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_set_M(CapyPDF_Transition *tr,
                                                CapyPDF_Transition_Motion m) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->M = m;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_set_Di(CapyPDF_Transition *tr,
                                                 uint32_t direction) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->Di = direction;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_set_SS(CapyPDF_Transition *tr,
                                                 double scale) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->SS = scale;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_set_B(CapyPDF_Transition *tr,
                                                int32_t is_opaque) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    CHECK_BOOLEAN(is_opaque);
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->B = is_opaque;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_destroy(CapyPDF_Transition *transition) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<Transition *>(transition);
    RETNOERR;
    API_BOUNDARY_END;
}

// Optional Content groups

CAPYPDF_PUBLIC CapyPDF_EC capy_optional_content_group_new(CapyPDF_OptionalContentGroup **out_ptr,
                                                          const char *name) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    // FIXME check for ASCIIness (or even more strict?)
    auto *ocg = new OptionalContentGroup();
    ocg->name = name;
    *out_ptr = reinterpret_cast<CapyPDF_OptionalContentGroup *>(ocg);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_optional_content_group_destroy(CapyPDF_OptionalContentGroup *ocg)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<OptionalContentGroup *>(ocg);
    RETNOERR;
    API_BOUNDARY_END;
}

// Graphics state

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_new(CapyPDF_GraphicsState **out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_GraphicsState *>(new GraphicsState);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_SMask(CapyPDF_GraphicsState *state,
                                                        CapyPDF_SoftMaskId smid) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *s = reinterpret_cast<GraphicsState *>(state);
    s->SMask = smid;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_CA(CapyPDF_GraphicsState *state,
                                                     double value) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *s = reinterpret_cast<GraphicsState *>(state);
    s->CA = LimitDouble{value};
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_ca(CapyPDF_GraphicsState *state,
                                                     double value) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *s = reinterpret_cast<GraphicsState *>(state);
    s->ca = LimitDouble{value};
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_BM(
    CapyPDF_GraphicsState *state, CapyPDF_Blend_Mode blendmode) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *s = reinterpret_cast<GraphicsState *>(state);
    s->BM = blendmode;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_op(CapyPDF_GraphicsState *state,
                                                     int32_t value) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *s = reinterpret_cast<GraphicsState *>(state);
    CHECK_BOOLEAN(value);
    s->op = value;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_OP(CapyPDF_GraphicsState *state,
                                                     int32_t value) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *s = reinterpret_cast<GraphicsState *>(state);
    CHECK_BOOLEAN(value);
    s->OP = value;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_OPM(CapyPDF_GraphicsState *state,
                                                      int32_t value) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *s = reinterpret_cast<GraphicsState *>(state);
    // Not actually boolean, but only values 0 and 1 are valid. See PDF spec 8.6.7.
    CHECK_BOOLEAN(value);
    s->OPM = value;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_FL(CapyPDF_GraphicsState *state,
                                                     double value) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *s = reinterpret_cast<GraphicsState *>(state);
    if(value < 0) {
        return conv_err(ErrorCode::InvalidFlatness);
    }
    s->FL = value;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_SM(CapyPDF_GraphicsState *state,
                                                     double value) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *s = reinterpret_cast<GraphicsState *>(state);
    if(value < 0 || value > 1.0) {
        return conv_err(ErrorCode::InvalidFlatness);
    }
    s->SM = value;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_AIS(CapyPDF_GraphicsState *state,
                                                      int32_t value) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *s = reinterpret_cast<GraphicsState *>(state);
    CHECK_BOOLEAN(value);
    s->AIS = value;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_TK(CapyPDF_GraphicsState *state,
                                                     int32_t value) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *s = reinterpret_cast<GraphicsState *>(state);
    CHECK_BOOLEAN(value);
    s->TK = value;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_destroy(CapyPDF_GraphicsState *state)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<GraphicsState *>(state);
    RETNOERR;
    API_BOUNDARY_END;
}

// Transparency Groups.

CAPYPDF_PUBLIC CapyPDF_EC capy_transparency_group_properties_new(
    CapyPDF_TransparencyGroupProperties **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr =
        reinterpret_cast<CapyPDF_TransparencyGroupProperties *>(new TransparencyGroupProperties());
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transparency_group_properties_set_CS(
    CapyPDF_TransparencyGroupProperties *props, CapyPDF_Device_Colorspace cs) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *p = reinterpret_cast<TransparencyGroupProperties *>(props);
    p->CS = cs;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transparency_group_properties_set_I(
    CapyPDF_TransparencyGroupProperties *props, int32_t I) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    CHECK_BOOLEAN(I);
    auto *p = reinterpret_cast<TransparencyGroupProperties *>(props);
    p->I = I;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transparency_group_properties_set_K(
    CapyPDF_TransparencyGroupProperties *props, int32_t K) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    CHECK_BOOLEAN(K);
    auto *p = reinterpret_cast<TransparencyGroupProperties *>(props);
    p->K = K;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transparency_group_properties_destroy(
    CapyPDF_TransparencyGroupProperties *props) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<TransparencyGroupProperties *>(props);
    RETNOERR;
    API_BOUNDARY_END;
}

// Raster images.

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_new(CapyPDF_RasterImageBuilder **out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_RasterImageBuilder *>(new RasterImageBuilder);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_set_size(CapyPDF_RasterImageBuilder *builder,
                                                             uint32_t w,
                                                             uint32_t h) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *b = reinterpret_cast<RasterImageBuilder *>(builder);
    b->i.md.w = w;
    b->i.md.h = h;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_set_pixel_depth(
    CapyPDF_RasterImageBuilder *builder, uint32_t depth) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *b = reinterpret_cast<RasterImageBuilder *>(builder);
    b->i.md.pixel_depth = depth;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_set_pixel_data(
    CapyPDF_RasterImageBuilder *builder, const char *buf, uint64_t bufsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *b = reinterpret_cast<RasterImageBuilder *>(builder);
    b->i.pixels.assign(buf, bufsize);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_set_alpha_depth(
    CapyPDF_RasterImageBuilder *builder, uint32_t depth) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *b = reinterpret_cast<RasterImageBuilder *>(builder);
    b->i.md.alpha_depth = depth;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_set_alpha_data(
    CapyPDF_RasterImageBuilder *builder, const char *buf, uint64_t bufsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *b = reinterpret_cast<RasterImageBuilder *>(builder);
    b->i.alpha.assign(buf, bufsize);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_set_input_data_compression_format(
    CapyPDF_RasterImageBuilder *builder, CapyPDF_Compression compression) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *b = reinterpret_cast<RasterImageBuilder *>(builder);
    b->i.md.compression = compression;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_set_colorspace(
    CapyPDF_RasterImageBuilder *builder, CapyPDF_Image_Colorspace colorspace) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *b = reinterpret_cast<RasterImageBuilder *>(builder);
    b->i.md.cs = colorspace;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_build(
    CapyPDF_RasterImageBuilder *builder, CapyPDF_RasterImage **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *b = reinterpret_cast<RasterImageBuilder *>(builder);
    // FIXME. Check validity.
    *out_ptr = reinterpret_cast<CapyPDF_RasterImage *>(new RasterImage(std::move(b->i)));
    b->i = RawPixelImage{};
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_get_size(const CapyPDF_RasterImage *image,
                                                     uint32_t *w,
                                                     uint32_t *h) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *i = reinterpret_cast<const RasterImage *>(image);
    if(auto *raw = std::get_if<RawPixelImage>(i)) {
        *w = raw->md.w;
        *h = raw->md.h;
    } else if(auto *jpg = std::get_if<jpg_image>(i)) {
        *w = jpg->w;
        *h = jpg->h;
    } else {
        return conv_err(ErrorCode::UnsupportedFormat);
    }
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_get_colorspace(
    const CapyPDF_RasterImage *image, CapyPDF_Image_Colorspace *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *i = reinterpret_cast<const RasterImage *>(image);
    if(auto *raw = std::get_if<RawPixelImage>(i)) {
        *out_ptr = raw->md.cs;
    } else {
        return conv_err(ErrorCode::UnsupportedFormat);
    }
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_has_profile(const CapyPDF_RasterImage *image,
                                                        int32_t *out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *i = reinterpret_cast<const RasterImage *>(image);
    if(auto *raw = std::get_if<RawPixelImage>(i)) {
        *out_ptr = raw->icc_profile.is_empty() ? 0 : 1;
    } else {
        return conv_err(ErrorCode::UnsupportedFormat);
    }
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_destroy(CapyPDF_RasterImageBuilder *builder)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ri = reinterpret_cast<RasterImageBuilder *>(builder);
    delete ri;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_destroy(CapyPDF_RasterImage *image) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ri = reinterpret_cast<RasterImage *>(image);
    delete ri;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type2_function_new(double *domain,
                                                  int32_t domain_size,
                                                  const CapyPDF_Color *c1,
                                                  const CapyPDF_Color *c2,
                                                  double n,
                                                  CapyPDF_Function **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_Function *>(
        new PdfFunction{FunctionType2{std::vector<double>(domain, domain + domain_size),
                                      *reinterpret_cast<const Color *>(c1),
                                      *reinterpret_cast<const Color *>(c2),
                                      n}});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_function_destroy(CapyPDF_Function *func) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<PdfFunction *>(func);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type3_function_new(double *domain,
                                                  int32_t domain_size,
                                                  CapyPDF_FunctionId *functions,
                                                  int32_t functions_size,
                                                  double *bounds,
                                                  int32_t bounds_size,
                                                  double *encode,
                                                  int32_t encode_size,
                                                  CapyPDF_Function **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_Function *>(new PdfFunction{
        FunctionType3{std::vector<double>(domain, domain + domain_size),
                      std::vector<CapyPDF_FunctionId>(functions, functions + functions_size),
                      std::vector<double>(bounds, bounds + bounds_size),
                      std::vector<double>(encode, encode + encode_size)}});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type4_function_new(double *domain,
                                                  int32_t domain_size,
                                                  double *range,
                                                  int32_t range_size,
                                                  const char *code,
                                                  CapyPDF_Function **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_Function *>(
        new PdfFunction{FunctionType4{std::vector<double>(domain, domain + domain_size),
                                      std::vector<double>(range, range + range_size),
                                      pystd2025::CString{code}}});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type2_shading_new(CapyPDF_Device_Colorspace cs,
                                                 double x0,
                                                 double y0,
                                                 double x1,
                                                 double y1,
                                                 CapyPDF_FunctionId func,
                                                 CapyPDF_Shading **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr =
        reinterpret_cast<CapyPDF_Shading *>(new PdfShading{ShadingType2{cs, x0, y0, x1, y1, func}});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_shading_set_extend(CapyPDF_Shading *shade,
                                                  int32_t starting,
                                                  int32_t ending) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    CHECK_BOOLEAN(starting);
    CHECK_BOOLEAN(ending);

    auto *sh = reinterpret_cast<PdfShading *>(shade);
    if(auto *ptr = std::get_if<ShadingType2>(sh)) {
        ptr->extend = ShadingExtend{starting != 0, ending != 0};
    } else if(auto *ptr = std::get_if<ShadingType3>(sh)) {
        ptr->extend = ShadingExtend{starting != 0, ending != 0};
    } else {
        return conv_err(ErrorCode::IncorrectFunctionType);
    }
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_shading_set_domain(CapyPDF_Shading *shade,
                                                  double starting,
                                                  double ending) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *sh = reinterpret_cast<PdfShading *>(shade);
    if(auto *ptr = std::get_if<ShadingType2>(sh)) {
        ptr->domain = ShadingDomain{starting, ending};
    } else if(auto *ptr = std::get_if<ShadingType3>(sh)) {
        ptr->domain = ShadingDomain{starting, ending};
    } else {
        return conv_err(ErrorCode::IncorrectFunctionType);
    }
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_shading_destroy(CapyPDF_Shading *shade) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<PdfShading *>(shade);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type3_shading_new(CapyPDF_Device_Colorspace cs,
                                                 double *coords,
                                                 CapyPDF_FunctionId func,
                                                 CapyPDF_Shading **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_Shading *>(new PdfShading{
        ShadingType3{cs, coords[0], coords[1], coords[2], coords[3], coords[4], coords[5], func}});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type4_shading_new(CapyPDF_Device_Colorspace cs,
                                                 double minx,
                                                 double miny,
                                                 double maxx,
                                                 double maxy,
                                                 CapyPDF_Shading **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_Shading *>(
        new PdfShading{ShadingType4{{}, minx, miny, maxx, maxy, cs}});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type4_shading_add_triangle(
    CapyPDF_Shading *shade, const double *coords, const CapyPDF_Color **color) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *sh = reinterpret_cast<PdfShading *>(shade);
    auto *sh4 = std::get_if<ShadingType4>(sh);
    if(!sh) {
        return conv_err(ErrorCode::IncorrectShadingType);
    }
    auto *cc = reinterpret_cast<const Color **>(color);
    ShadingPoint sp1 = conv_shpoint(coords, cc[0]);
    ShadingPoint sp2 = conv_shpoint(coords + 2, cc[1]);
    ShadingPoint sp3 = conv_shpoint(coords + 4, cc[2]);
    sh4->start_strip(sp1, sp2, sp3);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type4_shading_extend(CapyPDF_Shading *shade,
                                                    int32_t flag,
                                                    const double *coords,
                                                    const CapyPDF_Color *color) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *sh = reinterpret_cast<PdfShading *>(shade);
    auto *sh4 = std::get_if<ShadingType4>(sh);
    if(!sh) {
        return conv_err(ErrorCode::IncorrectShadingType);
    }
    auto *cc = reinterpret_cast<const Color *>(color);
    if(flag == 1 || flag == 2) {
        if(sh4->elements.empty()) {
            return conv_err(ErrorCode::BadStripStart);
        }
        ShadingPoint sp = conv_shpoint(coords, cc);
        sh4->extend_strip(sp, flag);
    } else {
        return conv_err(ErrorCode::BadEnum);
    }
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type6_shading_new(CapyPDF_Device_Colorspace cs,
                                                 double minx,
                                                 double miny,
                                                 double maxx,
                                                 double maxy,
                                                 CapyPDF_Shading **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_Shading *>(new PdfShading{ShadingType6{
        {},
        minx,
        miny,
        maxx,
        maxy,
        cs,
    }});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type6_shading_add_patch(
    CapyPDF_Shading *shade, const double *coords, const CapyPDF_Color **colors) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *sh = reinterpret_cast<PdfShading *>(shade);
    auto *sh6 = std::get_if<ShadingType6>(sh);
    if(!sh6) {
        return conv_err(ErrorCode::IncorrectShadingType);
    }
    auto **cc = reinterpret_cast<const Color **>(colors);
    FullCoonsPatch cp;
    grab_coons_data(cp, coords, cc);
    sh6->elements.emplace_back(std::move(cp));
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type6_shading_extend(CapyPDF_Shading *shade,
                                                    int32_t flag,
                                                    const double *coords,
                                                    const CapyPDF_Color **colors) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *sh = reinterpret_cast<PdfShading *>(shade);
    auto *sh6 = std::get_if<ShadingType6>(sh);
    if(!sh6) {
        return conv_err(ErrorCode::IncorrectShadingType);
    }
    auto **cc = reinterpret_cast<const Color **>(colors);
    if(flag == 1 || flag == 2 || flag == 3) {
        if(sh6->elements.is_empty()) {
            return conv_err(ErrorCode::BadStripStart);
        }
        ContinuationCoonsPatch ccp;
        grab_coons_data(ccp, coords, cc);
        sh6->elements.emplace_back(std::move(ccp));
    } else {
        return conv_err(ErrorCode::BadEnum);
    }
    RETNOERR;
    API_BOUNDARY_END;
}

// Annotations

CAPYPDF_PUBLIC CapyPDF_EC capy_text_annotation_new(const char *utf8_text,
                                                   int32_t strsize,
                                                   CapyPDF_Annotation **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto u8str = validate_utf8(utf8_text, strsize);
    if(!u8str) {
        return conv_err(u8str);
    }
    *out_ptr = reinterpret_cast<CapyPDF_Annotation *>(
        new Annotation{TextAnnotation{std::move(u8str.value())}, {}});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_link_annotation_new(CapyPDF_Annotation **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_Annotation *>(new Annotation{LinkAnnotation{}, {}});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_file_attachment_annotation_new(
    CapyPDF_EmbeddedFileId fid, CapyPDF_Annotation **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr =
        reinterpret_cast<CapyPDF_Annotation *>(new Annotation{FileAttachmentAnnotation{fid}, {}});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_printers_mark_annotation_new(
    CapyPDF_FormXObjectId fid, CapyPDF_Annotation **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr =
        reinterpret_cast<CapyPDF_Annotation *>(new Annotation{PrintersMarkAnnotation{fid}, {}});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_annotation_set_destination(
    CapyPDF_Annotation *annotation, const CapyPDF_Destination *d) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *a = reinterpret_cast<Annotation *>(annotation);
    auto *dest = reinterpret_cast<const Destination *>(d);
    if(auto *linka = a->sub.get_if<LinkAnnotation>()) {
        linka->Dest = *dest;
        linka->URI.reset();
    } else {
        return conv_err(ErrorCode::IncorrectAnnotationType);
    }
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_annotation_set_uri(CapyPDF_Annotation *annotation,
                                                  const char *uri,
                                                  int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *a = reinterpret_cast<Annotation *>(annotation);
    auto urirc = validate_ascii(uri, strsize);
    if(!urirc) {
        return conv_err(urirc);
    }
    if(auto *linka = a->sub.get_if<LinkAnnotation>()) {
        linka->Dest.reset();
        linka->URI = std::move(urirc.value());
    } else {
        return conv_err(ErrorCode::IncorrectAnnotationType);
    }
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_annotation_set_rectangle(
    CapyPDF_Annotation *annotation, double x1, double y1, double x2, double y2) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *a = reinterpret_cast<Annotation *>(annotation);
    a->rect = PdfRectangle{x1, y1, x2, y2};
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_annotation_set_flags(
    CapyPDF_Annotation *annotation, CapyPDF_Annotation_Flags flags) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *a = reinterpret_cast<Annotation *>(annotation);
    a->flags = flags;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_annotation_destroy(CapyPDF_Annotation *annotation) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<Annotation *>(annotation);
    API_BOUNDARY_START;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_struct_item_extra_data_new(CapyPDF_StructItemExtraData **out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_StructItemExtraData *>(new StructItemExtraData());
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_struct_item_extra_data_set_t(CapyPDF_StructItemExtraData *extra,
                                                            const char *ttext,
                                                            int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ed = reinterpret_cast<StructItemExtraData *>(extra);
    auto rc = validate_utf8(ttext, strsize);
    if(rc) {
        ed->T = std::move(rc.value());
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_struct_item_extra_data_set_lang(CapyPDF_StructItemExtraData *extra,
                                                               const char *lang,
                                                               int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ed = reinterpret_cast<StructItemExtraData *>(extra);
    auto rc = validate_ascii(lang, strsize);
    if(rc) {
        ed->Lang = std::move(rc.value());
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_struct_item_extra_data_set_alt(CapyPDF_StructItemExtraData *extra,
                                                              const char *alt,
                                                              int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ed = reinterpret_cast<StructItemExtraData *>(extra);
    auto rc = validate_utf8(alt, strsize);
    if(rc) {
        ed->Alt = std::move(rc.value());
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_struct_item_extra_data_set_actual_text(
    CapyPDF_StructItemExtraData *extra, const char *actual, int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *ed = reinterpret_cast<StructItemExtraData *>(extra);
    auto rc = validate_utf8(actual, strsize);
    if(rc) {
        ed->ActualText = std::move(rc.value());
    }
    return conv_err(rc);
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_struct_item_extra_data_destroy(CapyPDF_StructItemExtraData *extra)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<StructItemExtraData *>(extra);
    RETNOERR;
    API_BOUNDARY_END;
}

// Image load

CAPYPDF_PUBLIC CapyPDF_EC capy_image_pdf_properties_new(CapyPDF_ImagePdfProperties **out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_ImagePdfProperties *>(new ImagePDFProperties());
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_image_pdf_properties_set_mask(CapyPDF_ImagePdfProperties *par,
                                                             int32_t as_mask) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    CHECK_BOOLEAN(as_mask);
    auto p = reinterpret_cast<ImagePDFProperties *>(par);
    p->as_mask = as_mask;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_image_pdf_properties_set_interpolate(
    CapyPDF_ImagePdfProperties *par, CapyPDF_Image_Interpolation interp) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto p = reinterpret_cast<ImagePDFProperties *>(par);
    p->interp = interp;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_image_pdf_properties_destroy(CapyPDF_ImagePdfProperties *par)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<ImagePDFProperties *>(par);
    RETNOERR;
    API_BOUNDARY_END;
}

// Destination

CAPYPDF_PUBLIC CapyPDF_EC capy_destination_new(CapyPDF_Destination **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_Destination *>(new Destination());
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_destination_set_page_fit(
    CapyPDF_Destination *dest, int32_t physical_page_number) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *d = reinterpret_cast<Destination *>(dest);
    if(physical_page_number < 0) {
        return conv_err(ErrorCode::InvalidPageNumber);
    }
    d->page = physical_page_number;
    d->loc = DestinationFit{};
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_destination_set_page_xyz(CapyPDF_Destination *dest,
                                                        int32_t physical_page_number,
                                                        double *x,
                                                        double *y,
                                                        double *z) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *d = reinterpret_cast<Destination *>(dest);
    if(physical_page_number < 0) {
        return conv_err(ErrorCode::InvalidPageNumber);
    }
    d->page = physical_page_number;
    auto dxyz = DestinationXYZ{};
    if(x) {
        dxyz.x = *x;
    }
    if(y) {
        dxyz.y = *y;
    }
    if(z) {
        dxyz.z = *z;
    }
    d->loc = dxyz;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_destination_destroy(CapyPDF_Destination *dest) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<Destination *>(dest);
    RETNOERR;
    API_BOUNDARY_END;
}

// Outline

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_new(CapyPDF_Outline **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_Outline *>(new Outline{});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_set_title(CapyPDF_Outline *outline,
                                                 const char *title,
                                                 int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *o = reinterpret_cast<Outline *>(outline);
    auto u8str = validate_utf8(title, strsize);
    if(!u8str) {
        return conv_err(u8str);
    }
    o->title = std::move(u8str.value());
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_set_destination(
    CapyPDF_Outline *outline, const CapyPDF_Destination *dest) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *o = reinterpret_cast<Outline *>(outline);
    o->dest = *reinterpret_cast<const Destination *>(dest);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_set_C(CapyPDF_Outline *outline, double r, double g, double b)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *o = reinterpret_cast<Outline *>(outline);
    o->C = DeviceRGBColor{r, g, b};
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_set_F(CapyPDF_Outline *outline,
                                             uint32_t F) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *o = reinterpret_cast<Outline *>(outline);
    o->F = F;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_set_parent(CapyPDF_Outline *outline,
                                                  CapyPDF_OutlineId parent) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *o = reinterpret_cast<Outline *>(outline);
    o->parent = parent;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_destroy(CapyPDF_Outline *outline) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<Outline *>(outline);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_shading_pattern_new(
    CapyPDF_ShadingId shid, CapyPDF_ShadingPattern **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_ShadingPattern *>(new ShadingPattern{shid, {}});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_shading_pattern_set_matrix(CapyPDF_ShadingPattern *shp,
                                                          double a,
                                                          double b,
                                                          double c,
                                                          double d,
                                                          double e,
                                                          double f) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *p = reinterpret_cast<ShadingPattern *>(shp);
    p->m = PdfMatrix{a, b, c, d, e, f};
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_shading_pattern_destroy(CapyPDF_ShadingPattern *shp)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<ShadingPattern *>(shp);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_soft_mask_new(CapyPDF_Soft_Mask_Subtype subtype,
                                             CapyPDF_TransparencyGroupId trid,
                                             CapyPDF_SoftMask **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_SoftMask *>(new SoftMask{subtype, trid});
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_soft_mask_destroy(CapyPDF_SoftMask *sm) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<SoftMask *>(sm);
    RETNOERR;
    API_BOUNDARY_END;
}

// Embedded file

CAPYPDF_PUBLIC CapyPDF_EC capy_embedded_file_new(const char *path,
                                                 CapyPDF_EmbeddedFile **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    pystd2025::Path fspath(path);
    auto pathless_name = fspath.filename();
    auto rc = validate_utf8(pathless_name.c_str(), pathless_name.size());
    if(!rc) {
        return conv_err(rc);
    }
    auto *eobj = new EmbeddedFile();
    eobj->path = std::move(fspath);
    eobj->pdfname = std::move(rc.value());
    *out_ptr = reinterpret_cast<CapyPDF_EmbeddedFile *>(eobj);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_embedded_file_set_subtype(CapyPDF_EmbeddedFile *efile,
                                                         const char *subtype,
                                                         int32_t strsize) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *eobj = reinterpret_cast<EmbeddedFile *>(efile);
    auto rc = validate_ascii(subtype, strsize);
    if(!rc) {
        return conv_err(rc);
    }
    eobj->subtype = std::move(rc.value());
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_embedded_file_destroy(CapyPDF_EmbeddedFile *efile) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<EmbeddedFile *>(efile);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_bdc_tags_new(CapyPDF_BDCTags **out_ptr) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_BDCTags *>(new BDCTags());
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_bdc_tags_add_tag(CapyPDF_BDCTags *tags,
                                                const char *key,
                                                int32_t keylen,
                                                const char *value,
                                                int32_t valuelen) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *bt = reinterpret_cast<BDCTags *>(tags);
    auto akey = validate_ascii(key, keylen);
    auto avalue = validate_ascii(value, valuelen);
    if(!akey) {
        return conv_err(akey.error());
    }
    if(!avalue) {
        return conv_err(avalue.error());
    }
    (*bt).insert(akey.value(), avalue.value());
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_bdc_tags_destroy(CapyPDF_BDCTags *tags) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<BDCTags *>(tags);
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_font_properties_new(CapyPDF_FontProperties **out_ptr)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    *out_ptr = reinterpret_cast<CapyPDF_FontProperties *>(new FontProperties());
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_font_properties_set_subfont(CapyPDF_FontProperties *fprop,
                                                           int32_t subfont) CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    auto *fp = reinterpret_cast<FontProperties *>(fprop);
    if(subfont < 0 || subfont >= (1 << 16)) {
        // This is a limitation of Freetype.
        return conv_err(ErrorCode::InvalidSubfont);
    }
    fp->subfont = subfont;
    RETNOERR;
    API_BOUNDARY_END;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_font_properties_destroy(CapyPDF_FontProperties *fprop)
    CAPYPDF_NOEXCEPT {
    API_BOUNDARY_START;
    delete reinterpret_cast<FontProperties *>(fprop);
    RETNOERR;
    API_BOUNDARY_END;
}

// Error handling.

const char *capy_error_message(CapyPDF_EC error_code) CAPYPDF_NOEXCEPT {
    return error_text((ErrorCode)error_code);
}
