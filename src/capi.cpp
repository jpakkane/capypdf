// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <capypdf.h>
#include <cstring>
#include <generator.hpp>
#include <drawcontext.hpp>
#include <errorhandling.hpp>

#define RETNOERR return conv_err(ErrorCode::NoError)

#define CHECK_NULL(x)                                                                              \
    if(x == nullptr) {                                                                             \
        return conv_err(ErrorCode::ArgIsNull);                                                     \
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

} // namespace

CapyPDF_EC capy_document_properties_new(CapyPDF_DocumentProperties **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_DocumentProperties *>(new DocumentProperties());
    RETNOERR;
}

CapyPDF_EC capy_document_properties_destroy(CapyPDF_DocumentProperties *docprops) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<DocumentProperties *>(docprops);
    RETNOERR;
}

CapyPDF_EC capy_document_properties_set_title(CapyPDF_DocumentProperties *docprops,
                                              const char *utf8_title,
                                              int32_t strsize) CAPYPDF_NOEXCEPT {
    auto rc = validate_utf8(utf8_title, strsize);
    if(rc) {
        reinterpret_cast<DocumentProperties *>(docprops)->title = std::move(rc.value());
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_author(CapyPDF_DocumentProperties *docprops,
                                                              const char *utf8_author,
                                                              int32_t strsize) CAPYPDF_NOEXCEPT {
    auto rc = validate_utf8(utf8_author, strsize);
    if(rc) {
        reinterpret_cast<DocumentProperties *>(docprops)->author = std::move(rc.value());
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_creator(CapyPDF_DocumentProperties *docprops,
                                                               const char *utf8_creator,
                                                               int32_t strsize) CAPYPDF_NOEXCEPT {
    auto rc = validate_utf8(utf8_creator, strsize);
    if(rc) {
        reinterpret_cast<DocumentProperties *>(docprops)->creator = std::move(rc.value());
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_language(
    CapyPDF_DocumentProperties *docprops, const char *lang, int32_t strsize) CAPYPDF_NOEXCEPT {
    auto rc = validate_ascii(lang, strsize);
    if(rc) {
        reinterpret_cast<DocumentProperties *>(docprops)->lang = std::move(rc.value());
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_page_properties_new(CapyPDF_PageProperties **out_ptr)
    CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_PageProperties *>(new PageProperties);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_page_properties_destroy(CapyPDF_PageProperties *pageprops)
    CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<PageProperties *>(pageprops);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_page_properties_set_pagebox(CapyPDF_PageProperties *pageprops,
                                                           CapyPDF_Page_Box boxtype,
                                                           double x1,
                                                           double y1,
                                                           double x2,
                                                           double y2) CAPYPDF_NOEXCEPT {
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
}

CAPYPDF_PUBLIC CapyPDF_EC capy_page_properties_set_transparency_group_properties(
    CapyPDF_PageProperties *pageprops,
    CapyPDF_TransparencyGroupProperties *trprop) CAPYPDF_NOEXCEPT {
    auto *page = reinterpret_cast<PageProperties *>(pageprops);
    auto *tr = reinterpret_cast<TransparencyGroupProperties *>(trprop);
    page->transparency_props = *tr;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_document_properties_set_device_profile(CapyPDF_DocumentProperties *docprops,
                                            CapyPDF_Device_Colorspace cs,
                                            const char *profile_path) CAPYPDF_NOEXCEPT {
    auto metadata = reinterpret_cast<DocumentProperties *>(docprops);
    switch(cs) {
    case CAPY_DEVICE_CS_RGB:
        metadata->prof.rgb_profile_file = profile_path;
        break;
    case CAPY_DEVICE_CS_GRAY:
        metadata->prof.gray_profile_file = profile_path;
        break;
    case CAPY_DEVICE_CS_CMYK:
        metadata->prof.cmyk_profile_file = profile_path;
        break;
    }
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_colorspace(
    CapyPDF_DocumentProperties *docprops, CapyPDF_Device_Colorspace cs) CAPYPDF_NOEXCEPT {
    auto metadata = reinterpret_cast<DocumentProperties *>(docprops);
    metadata->output_colorspace = cs;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_document_properties_set_output_intent(CapyPDF_DocumentProperties *docprops,
                                           const char *identifier,
                                           int32_t strsize) CAPYPDF_NOEXCEPT {
    auto metadata = reinterpret_cast<DocumentProperties *>(docprops);
    auto rc = validate_utf8(identifier, strsize);
    if(!rc) {
        return conv_err(rc.error());
    }
    metadata->intent_condition_identifier = std::move(rc.value());
    RETNOERR;
}
CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_pdfx(
    CapyPDF_DocumentProperties *docprops, CapyPDF_PDFX_Type xtype) CAPYPDF_NOEXCEPT {
    auto metadata = reinterpret_cast<DocumentProperties *>(docprops);
    metadata->subtype = xtype;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_pdfa(
    CapyPDF_DocumentProperties *docprops, CapyPDF_PDFA_Type atype) CAPYPDF_NOEXCEPT {
    auto metadata = reinterpret_cast<DocumentProperties *>(docprops);
    metadata->subtype = atype;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_default_page_properties(
    CapyPDF_DocumentProperties *docprops, const CapyPDF_PageProperties *prop) CAPYPDF_NOEXCEPT {
    auto metadata = reinterpret_cast<DocumentProperties *>(docprops);
    auto props = reinterpret_cast<const PageProperties *>(prop);
    if(!props->mediabox) {
        return conv_err(ErrorCode::MissingMediabox);
    }
    metadata->default_page_properties = *props;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_document_properties_set_tagged(CapyPDF_DocumentProperties *docprops,
                                                              int32_t is_tagged) CAPYPDF_NOEXCEPT {
    CHECK_BOOLEAN(is_tagged);
    auto metadata = reinterpret_cast<DocumentProperties *>(docprops);
    metadata->is_tagged = is_tagged;
    RETNOERR;
}

CapyPDF_EC capy_generator_new(const char *filename,
                              const CapyPDF_DocumentProperties *docprops,
                              CapyPDF_Generator **out_ptr) CAPYPDF_NOEXCEPT {
    CHECK_NULL(filename);
    CHECK_NULL(docprops);
    CHECK_NULL(out_ptr);
    auto metadata = reinterpret_cast<const DocumentProperties *>(docprops);
    auto rc = PdfGen::construct(filename, *metadata);
    if(rc) {
        *out_ptr = reinterpret_cast<CapyPDF_Generator *>(rc.value().release());
    }
    return conv_err(rc);
}

CapyPDF_EC capy_generator_add_page(CapyPDF_Generator *gen,
                                   CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);

    auto rc = g->add_page(*dc);
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_page_labeling(CapyPDF_Generator *gen,
                                                           uint32_t start_page,
                                                           CapyPDF_Page_Label_Number_Style *style,
                                                           const char *prefix,
                                                           int32_t strsize,
                                                           uint32_t *start_num) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    std::optional<CapyPDF_Page_Label_Number_Style> opt_style;
    std::optional<u8string> opt_prefix;
    std::optional<uint32_t> opt_start_num;
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
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_form_xobject(CapyPDF_Generator *gen,
                                                          CapyPDF_DrawContext *ctx,
                                                          CapyPDF_FormXObjectId *out_ptr)
    CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);

    auto rc = g->add_form_xobject(*dc);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_generator_add_transparency_group(CapyPDF_Generator *gen,
                                      CapyPDF_DrawContext *ctx,
                                      CapyPDF_TransparencyGroupId *out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);

    auto rc = g->add_transparency_group(*dc);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_soft_mask(CapyPDF_Generator *gen,
                                                       const CapyPDF_SoftMask *sm,
                                                       CapyPDF_SoftMaskId *out_ptr)
    CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *s = reinterpret_cast<const SoftMask *>(sm);

    auto rc = g->add_soft_mask(*s);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_shading_pattern(CapyPDF_Generator *gen,
                                                             CapyPDF_ShadingPattern *shp,
                                                             CapyPDF_PatternId *out_ptr)
    CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *shad = reinterpret_cast<ShadingPattern *>(shp);
    auto rc = g->add_shading_pattern(*shad);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_tiling_pattern(
    CapyPDF_Generator *gen, CapyPDF_DrawContext *ctx, CapyPDF_PatternId *out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *colordc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto rc = g->add_tiling_pattern(*colordc);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_embed_file(CapyPDF_Generator *gen,
                                                    CapyPDF_EmbeddedFile *efile,
                                                    CapyPDF_EmbeddedFileId *out_ptr)
    CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *ef = reinterpret_cast<EmbeddedFile *>(efile);
    auto rc = g->embed_file(*ef);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_load_font(CapyPDF_Generator *gen,
                                                   const char *fname,
                                                   CapyPDF_FontId *out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto rc = g->load_font(fname);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_load_image(
    CapyPDF_Generator *gen, const char *fname, CapyPDF_RasterImage **out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto rc = g->load_image(fname);
    if(rc) {
        auto *result = new RasterImage(std::move(rc.value()));
        *out_ptr = reinterpret_cast<CapyPDF_RasterImage *>(result);
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_convert_image(CapyPDF_Generator *gen,
                                                       const CapyPDF_RasterImage *source,
                                                       CapyPDF_Device_Colorspace output_cs,
                                                       CapyPDF_Rendering_Intent ri,
                                                       CapyPDF_RasterImage **out_ptr)
    CAPYPDF_NOEXCEPT {
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
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_image(CapyPDF_Generator *gen,
                                                   CapyPDF_RasterImage *image,
                                                   const CapyPDF_ImagePdfProperties *params,
                                                   CapyPDF_ImageId *out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *im = reinterpret_cast<RasterImage *>(image);
    auto *par = reinterpret_cast<const ImagePDFProperties *>(params);
    auto rc = g->add_image(std::move(*im), *par);
    if(rc) {
        *out_ptr = rc.value();
    }
    *im = RasterImage{};
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_function(
    CapyPDF_Generator *gen, CapyPDF_Function *func, CapyPDF_FunctionId *out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *f = reinterpret_cast<PdfFunction *>(func);
    auto rc = g->add_function(*f);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_shading(CapyPDF_Generator *gen,
                                                     CapyPDF_Shading *shade,
                                                     CapyPDF_ShadingId *out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *sh = reinterpret_cast<PdfShading *>(shade);
    auto rc = g->add_shading(*sh);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_graphics_state(CapyPDF_Generator *gen,
                                                            const CapyPDF_GraphicsState *state,
                                                            CapyPDF_GraphicsStateId *out_ptr)
    CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *s = reinterpret_cast<const GraphicsState *>(state);
    auto rc = g->add_graphics_state(*s);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_structure_item(CapyPDF_Generator *gen,
                                                            const CapyPDF_Structure_Type stype,
                                                            const CapyPDF_StructureItemId *parent,
                                                            CapyPDF_StructItemExtraData *extra,
                                                            CapyPDF_StructureItemId *out_ptr)
    CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    std::optional<CapyPDF_StructureItemId> item_parent;
    if(parent) {
        item_parent = *parent;
    }
    std::optional<StructItemExtraData> ed;
    if(extra) {
        ed = *reinterpret_cast<StructItemExtraData *>(extra);
    }
    auto rc = g->add_structure_item(stype, item_parent, std::move(ed));
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_generator_add_custom_structure_item(CapyPDF_Generator *gen,
                                         const CapyPDF_RoleId role,
                                         const CapyPDF_StructureItemId *parent,
                                         CapyPDF_StructItemExtraData *extra,
                                         CapyPDF_StructureItemId *out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    std::optional<CapyPDF_StructureItemId> item_parent;
    if(parent) {
        item_parent = *parent;
    }
    std::optional<StructItemExtraData> ed;
    if(extra) {
        ed = *reinterpret_cast<StructItemExtraData *>(extra);
    }
    auto rc = g->add_structure_item(role, item_parent, std::move(ed));
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_load_icc_profile(
    CapyPDF_Generator *gen, const char *fname, CapyPDF_IccColorSpaceId *out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto rc = g->load_icc_file(fname);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}
CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_icc_profile(CapyPDF_Generator *gen,
                                                         const char *buf,
                                                         uint64_t bufsize,
                                                         uint32_t num_channels,
                                                         CapyPDF_IccColorSpaceId *out_ptr)
    CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    std::byte *bytebuf = (std::byte *)buf;
    auto rc = g->add_icc_profile({bytebuf, bufsize}, num_channels);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
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
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_separation(CapyPDF_Generator *gen,
                                                        const char *separation_name,
                                                        int32_t strsize,
                                                        CapyPDF_Device_Colorspace cs,
                                                        CapyPDF_FunctionId fid,
                                                        CapyPDF_SeparationId *out_ptr)
    CAPYPDF_NOEXCEPT {
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
}

CapyPDF_EC capy_generator_write(CapyPDF_Generator *gen) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto rc = g->write();
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_optional_content_group(
    CapyPDF_Generator *gen,
    const CapyPDF_OptionalContentGroup *ocg,
    CapyPDF_OptionalContentGroupId *out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    const auto *group = reinterpret_cast<const OptionalContentGroup *>(ocg);
    auto rc = g->add_optional_content_group(*group);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_annotation(CapyPDF_Generator *gen,
                                                        CapyPDF_Annotation *annotation,
                                                        CapyPDF_AnnotationId *out_ptr)
    CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *a = reinterpret_cast<Annotation *>(annotation);
    auto rc = g->add_annotation(*a);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_rolemap_entry(CapyPDF_Generator *gen,
                                                           const char *name,
                                                           CapyPDF_Structure_Type builtin,
                                                           CapyPDF_RoleId *out_ptr)
    CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto rc = g->add_rolemap_entry(name, builtin);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CapyPDF_EC capy_generator_destroy(CapyPDF_Generator *gen) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    delete g;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_add_outline(CapyPDF_Generator *gen,
                                                     const CapyPDF_Outline *outline,
                                                     CapyPDF_OutlineId *out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    auto *o = reinterpret_cast<const Outline *>(outline);

    auto rc = g->add_outline(*o);
    if(rc) {
        *out_ptr = rc.value();
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_generator_text_width(CapyPDF_Generator *gen,
                                                    const char *utf8_text,
                                                    int32_t strsize,
                                                    CapyPDF_FontId font,
                                                    double pointsize,
                                                    double *out_ptr) CAPYPDF_NOEXCEPT {
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
}

// Draw Context

CapyPDF_EC capy_page_draw_context_new(CapyPDF_Generator *gen,
                                      CapyPDF_DrawContext **out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    *out_ptr = reinterpret_cast<CapyPDF_DrawContext *>(g->new_page_draw_context());
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_b(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_b());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_B(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_B());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_bstar(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_bstar());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_Bstar(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_Bstar());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_BDC_builtin(
    CapyPDF_DrawContext *ctx, CapyPDF_StructureItemId structid) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_BDC(structid));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_BDC_ocg(
    CapyPDF_DrawContext *ctx, CapyPDF_OptionalContentGroupId ocgid) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_BDC(ocgid));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_BMC(CapyPDF_DrawContext *ctx,
                                          const char *tag) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_BMC(tag));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_c(CapyPDF_DrawContext *ctx,
                                        double x1,
                                        double y1,
                                        double x2,
                                        double y2,
                                        double x3,
                                        double y3) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_c(x1, y1, x2, y2, x3, y3));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_cm(CapyPDF_DrawContext *ctx,
                                         double m1,
                                         double m2,
                                         double m3,
                                         double m4,
                                         double m5,
                                         double m6) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_cm(m1, m2, m3, m4, m5, m6));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_d(CapyPDF_DrawContext *ctx,
                                        double *dash_array,
                                        int32_t array_size,
                                        double phase) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_d(dash_array, array_size, phase));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_Do_trgroup(
    CapyPDF_DrawContext *ctx, CapyPDF_TransparencyGroupId tgid) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_Do(tgid));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_Do_image(CapyPDF_DrawContext *ctx,
                                               CapyPDF_ImageId iid) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_Do(iid));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_EMC(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_EMC());
}

CapyPDF_EC capy_dc_cmd_f(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_f());
}

CapyPDF_EC capy_dc_cmd_fstar(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_fstar());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_G(CapyPDF_DrawContext *ctx, double gray) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_G(gray));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_g(CapyPDF_DrawContext *ctx, double gray) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_g(gray));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_gs(CapyPDF_DrawContext *ctx,
                                         CapyPDF_GraphicsStateId gsid) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_gs(gsid));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_h(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_h());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_i(CapyPDF_DrawContext *ctx,
                                        double flatness) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_i(flatness));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_j(CapyPDF_DrawContext *ctx,
                                        CapyPDF_Line_Join join_style) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_j(join_style));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_J(CapyPDF_DrawContext *ctx,
                                        CapyPDF_Line_Cap cap_style) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_J(cap_style));
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_dc_cmd_k(CapyPDF_DrawContext *ctx, double c, double m, double y, double k) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_k(c, m, y, k));
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_dc_cmd_K(CapyPDF_DrawContext *ctx, double c, double m, double y, double k) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_K(c, m, y, k));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_l(CapyPDF_DrawContext *ctx,
                                        double x,
                                        double y) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->cmd_l(x, y));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_m(CapyPDF_DrawContext *ctx,
                                        double x,
                                        double y) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_m(x, y));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_M(CapyPDF_DrawContext *ctx,
                                        double miterlimit) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_M(miterlimit));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_n(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_n());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_q(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_q());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_Q(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_Q());
}

CapyPDF_EC
capy_dc_cmd_re(CapyPDF_DrawContext *ctx, double x, double y, double w, double h) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_re(x, y, w, h));
}

CapyPDF_EC capy_dc_cmd_RG(CapyPDF_DrawContext *ctx, double r, double g, double b) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_RG(r, g, b));
}

CapyPDF_EC capy_dc_cmd_rg(CapyPDF_DrawContext *ctx, double r, double g, double b) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_rg(r, g, b));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_ri(CapyPDF_DrawContext *ctx,
                                         CapyPDF_Rendering_Intent ri) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_ri(ri));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_s(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_s());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_S(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_S());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_sh(CapyPDF_DrawContext *ctx,
                                         CapyPDF_ShadingId shid) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_sh(shid));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_v(
    CapyPDF_DrawContext *ctx, double x2, double y2, double x3, double y3) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_v(x2, y2, x3, y3));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_w(CapyPDF_DrawContext *ctx,
                                        double line_width) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_w(line_width));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_W(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_W());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_Wstar(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_Wstar());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_cmd_y(
    CapyPDF_DrawContext *ctx, double x1, double y1, double x3, double y3) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(c->cmd_y(x1, y1, x3, y3));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_set_stroke(CapyPDF_DrawContext *ctx,
                                             CapyPDF_Color *c) CAPYPDF_NOEXCEPT {
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto *color = reinterpret_cast<Color *>(c);
    return conv_err(dc->set_stroke_color(*color));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_set_nonstroke(CapyPDF_DrawContext *ctx,
                                                CapyPDF_Color *c) CAPYPDF_NOEXCEPT {
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto *color = reinterpret_cast<Color *>(c);
    return conv_err(dc->set_nonstroke_color(*color));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_render_text(CapyPDF_DrawContext *ctx,
                                              const char *text,
                                              int32_t strsize,
                                              CapyPDF_FontId fid,
                                              double point_size,
                                              double x,
                                              double y) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    auto utxt = validate_utf8(text, strsize);
    if(!utxt) {
        return conv_err(utxt);
    }
    return conv_err(c->render_text(utxt.value(), fid, point_size, x, y));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_render_text_obj(CapyPDF_DrawContext *ctx,
                                                  CapyPDF_Text *text) CAPYPDF_NOEXCEPT {
    auto c = reinterpret_cast<PdfDrawContext *>(ctx);
    auto t = reinterpret_cast<PdfText *>(text);
    return conv_err(c->render_text(*t));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_set_page_transition(
    CapyPDF_DrawContext *ctx, CapyPDF_Transition *transition) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto t = reinterpret_cast<Transition *>(transition);
    auto rc = dc->set_transition(*t);
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_set_custom_page_properties(
    CapyPDF_DrawContext *ctx, const CapyPDF_PageProperties *custom_properties) {
    CHECK_NULL(custom_properties);
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto *cprop = reinterpret_cast<const PageProperties *>(custom_properties);
    return conv_err(dc->set_custom_page_properties(*cprop));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_annotate(CapyPDF_DrawContext *ctx,
                                           CapyPDF_AnnotationId aid) CAPYPDF_NOEXCEPT {
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    return conv_err(dc->annotate(aid));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_set_transparency_group_properties(
    CapyPDF_DrawContext *ctx, CapyPDF_TransparencyGroupProperties *trprop) CAPYPDF_NOEXCEPT {
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    auto *tr = reinterpret_cast<TransparencyGroupProperties *>(trprop);
    return conv_err(dc->set_transparency_properties(*tr));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_set_group_matrix(CapyPDF_DrawContext *ctx,
                                                   double a,
                                                   double b,
                                                   double c,
                                                   double d,
                                                   double e,
                                                   double f) CAPYPDF_NOEXCEPT {
    auto *dc = reinterpret_cast<PdfDrawContext *>(ctx);
    PdfMatrix m{a, b, c, d, e, f};
    return conv_err(dc->set_group_matrix(m));
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_dc_add_simple_navigation(CapyPDF_DrawContext *ctx,
                              const CapyPDF_OptionalContentGroupId *ocgarray,
                              int32_t array_size,
                              const CapyPDF_Transition *tr) CAPYPDF_NOEXCEPT {
    auto dc = reinterpret_cast<PdfDrawContext *>(ctx);
    std::optional<Transition> transition;
    if(tr) {
        transition = *reinterpret_cast<const Transition *>(tr);
    }
    std::span<const CapyPDF_OptionalContentGroupId> ocgspan(ocgarray, ocgarray + array_size);
    auto rc = dc->add_simple_navigation(ocgspan, transition);
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_dc_text_new(CapyPDF_DrawContext *dc,
                                           CapyPDF_Text **out_ptr) CAPYPDF_NOEXCEPT {
    CHECK_NULL(dc);
    *out_ptr = reinterpret_cast<CapyPDF_Text *>(
        new capypdf::internal::PdfText(reinterpret_cast<PdfDrawContext *>(dc)));
    RETNOERR;
}

CapyPDF_EC capy_dc_destroy(CapyPDF_DrawContext *ctx) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<PdfDrawContext *>(ctx);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_form_xobject_new(CapyPDF_Generator *gen,
                                                double l,
                                                double b,
                                                double t,
                                                double r,
                                                CapyPDF_DrawContext **out_ptr) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    *out_ptr =
        reinterpret_cast<CapyPDF_DrawContext *>(g->new_form_xobject(PdfRectangle{l, b, t, r}));
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transparency_group_new(
    CapyPDF_Generator *gen, double l, double b, double r, double t, CapyPDF_DrawContext **out_ptr)
    CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    PdfRectangle bbox{l, b, r, t};
    *out_ptr = reinterpret_cast<CapyPDF_DrawContext *>(g->new_transparency_group(bbox));
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_tiling_pattern_context_new(CapyPDF_Generator *gen,
                                                          CapyPDF_DrawContext **out_ptr,
                                                          double l,
                                                          double b,
                                                          double r,
                                                          double t) CAPYPDF_NOEXCEPT {
    auto *g = reinterpret_cast<PdfGen *>(gen);
    *out_ptr =
        reinterpret_cast<CapyPDF_DrawContext *>(g->new_color_pattern(PdfRectangle{l, b, r, t}));
    RETNOERR;
}

// Text

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_new(CapyPDF_TextSequence **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_TextSequence *>(new TextSequence());
    RETNOERR;
}
CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_codepoint(CapyPDF_TextSequence *tseq,
                                                              uint32_t codepoint) CAPYPDF_NOEXCEPT {
    auto *ts = reinterpret_cast<TextSequence *>(tseq);
    auto rc = ts->append_unicode(codepoint);
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_kerning(CapyPDF_TextSequence *tseq,
                                                            int32_t kern) CAPYPDF_NOEXCEPT {
    auto *ts = reinterpret_cast<TextSequence *>(tseq);
    auto rc = ts->append_kerning(kern);
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_actualtext_start(
    CapyPDF_TextSequence *tseq, const char *actual_text, int32_t strsize) CAPYPDF_NOEXCEPT {
    auto *ts = reinterpret_cast<TextSequence *>(tseq);
    auto utxt = validate_utf8(actual_text, strsize);
    if(!utxt) {
        return conv_err(utxt);
    }

    auto rc = ts->append_actualtext_start(utxt.value());
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_actualtext_end(CapyPDF_TextSequence *tseq)
    CAPYPDF_NOEXCEPT {
    auto *ts = reinterpret_cast<TextSequence *>(tseq);
    auto rc = ts->append_actualtext_end();
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_raw_glyph(CapyPDF_TextSequence *tseq,
                                                              uint32_t glyph_id,
                                                              uint32_t codepoint) CAPYPDF_NOEXCEPT {
    auto *ts = reinterpret_cast<TextSequence *>(tseq);
    if(glyph_id == 0) {
        return conv_err(ErrorCode::MissingGlyph);
    }
    auto rc = ts->append_raw_glyph(glyph_id, codepoint);
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_append_ligature_glyph(CapyPDF_TextSequence *tseq,
                                                                   uint32_t glyph_id,
                                                                   const char *original_text,
                                                                   int32_t strsize)
    CAPYPDF_NOEXCEPT {
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
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_sequence_destroy(CapyPDF_TextSequence *tseq) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<TextSequence *>(tseq);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_render_text(CapyPDF_Text *text,
                                                const char *utf8_text,
                                                int32_t strsize) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    auto txt = validate_utf8(utf8_text, strsize);
    if(!txt) {
        return conv_err(txt);
    }
    return conv_err(t->render_text(txt.value()));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_set_nonstroke(CapyPDF_Text *text,
                                                  const CapyPDF_Color *color) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    const auto *c = reinterpret_cast<const Color *>(color);
    return conv_err(t->nonstroke_color(*c));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_set_stroke(CapyPDF_Text *text,
                                               const CapyPDF_Color *color) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    const auto *c = reinterpret_cast<const Color *>(color);
    return conv_err(t->stroke_color(*c));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_BDC_builtin(CapyPDF_Text *text,
                                                    CapyPDF_StructureItemId stid) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_BDC(stid));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_EMC(CapyPDF_Text *text) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_EMC());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Tc(CapyPDF_Text *text, double spacing) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tc(spacing));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Td(CapyPDF_Text *text,
                                           double x,
                                           double y) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Td(x, y));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_TD(CapyPDF_Text *text,
                                           double x,
                                           double y) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_TD(x, y));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Tf(CapyPDF_Text *text,
                                           CapyPDF_FontId font,
                                           double pointsize) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tf(font, pointsize));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_TJ(CapyPDF_Text *text,
                                           CapyPDF_TextSequence *kseq) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    auto *ks = reinterpret_cast<TextSequence *>(kseq);
    auto rc = t->cmd_TJ(*ks);
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_TL(CapyPDF_Text *text, double leading) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_TL(leading));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Tm(CapyPDF_Text *text,
                                           double a,
                                           double b,
                                           double c,
                                           double d,
                                           double e,
                                           double f) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tm(PdfMatrix{a, b, c, d, e, f}));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Tr(CapyPDF_Text *text,
                                           CapyPDF_Text_Mode tmode) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tr(tmode));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Tw(CapyPDF_Text *text, double spacing) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tw(spacing));
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_cmd_Tstar(CapyPDF_Text *text) CAPYPDF_NOEXCEPT {
    auto *t = reinterpret_cast<PdfText *>(text);
    return conv_err(t->cmd_Tstar());
}

CAPYPDF_PUBLIC CapyPDF_EC capy_text_destroy(CapyPDF_Text *text) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<PdfText *>(text);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_new(CapyPDF_Color **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr =
        reinterpret_cast<CapyPDF_Color *>(new capypdf::internal::Color(DeviceRGBColor{0, 0, 0}));
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_destroy(CapyPDF_Color *color) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<capypdf::internal::Color *>(color);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_set_rgb(CapyPDF_Color *c, double r, double g, double b)
    CAPYPDF_NOEXCEPT {
    *reinterpret_cast<capypdf::internal::Color *>(c) = DeviceRGBColor{r, g, b};
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_set_gray(CapyPDF_Color *c, double v) CAPYPDF_NOEXCEPT {
    *reinterpret_cast<capypdf::internal::Color *>(c) = DeviceGrayColor{v};
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_color_set_cmyk(CapyPDF_Color *color, double c, double m, double y, double k) CAPYPDF_NOEXCEPT {
    *reinterpret_cast<capypdf::internal::Color *>(color) = DeviceCMYKColor{c, m, y, k};
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_set_icc(CapyPDF_Color *color,
                                             CapyPDF_IccColorSpaceId icc_id,
                                             double *values,
                                             int32_t num_values) CAPYPDF_NOEXCEPT {
    ICCColor icc;
    icc.id = icc_id;
    icc.values.assign(values, values + num_values);
    *reinterpret_cast<capypdf::internal::Color *>(color) = std::move(icc);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_set_separation(CapyPDF_Color *color,
                                                    CapyPDF_SeparationId sep_id,
                                                    double value) CAPYPDF_NOEXCEPT {
    auto *c = reinterpret_cast<Color *>(color);
    if(value < 0 || value > 1.0) {
        return conv_err(ErrorCode::ColorOutOfRange);
    }
    *c = SeparationColor{sep_id, value};
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_set_pattern(CapyPDF_Color *color,
                                                 CapyPDF_PatternId pat_id) CAPYPDF_NOEXCEPT {
    auto *c = reinterpret_cast<Color *>(color);
    *c = pat_id;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_color_set_lab(CapyPDF_Color *color,
                                             CapyPDF_LabColorSpaceId lab_id,
                                             double l,
                                             double a,
                                             double b) CAPYPDF_NOEXCEPT {
    auto *c = reinterpret_cast<Color *>(color);
    *c = LabColor{lab_id, l, a, b};
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_new(CapyPDF_Transition **out_ptr) CAPYPDF_NOEXCEPT {
    auto pt = new Transition{};
    *out_ptr = reinterpret_cast<CapyPDF_Transition *>(pt);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_set_S(CapyPDF_Transition *tr,
                                                CapyPDF_Transition_Type type) CAPYPDF_NOEXCEPT {
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->type = type;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_set_D(CapyPDF_Transition *tr,
                                                double duration) CAPYPDF_NOEXCEPT {
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->duration = duration;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC
capy_transition_set_Dm(CapyPDF_Transition *tr, CapyPDF_Transition_Dimension dim) CAPYPDF_NOEXCEPT {
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->Di = dim;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_set_M(CapyPDF_Transition *tr,
                                                CapyPDF_Transition_Motion m) CAPYPDF_NOEXCEPT {
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->M = m;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_set_Di(CapyPDF_Transition *tr,
                                                 uint32_t direction) CAPYPDF_NOEXCEPT {
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->Di = direction;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_set_SS(CapyPDF_Transition *tr,
                                                 double scale) CAPYPDF_NOEXCEPT {
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->SS = scale;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_set_B(CapyPDF_Transition *tr,
                                                int32_t is_opaque) CAPYPDF_NOEXCEPT {
    CHECK_BOOLEAN(is_opaque);
    auto pt = reinterpret_cast<Transition *>(tr);
    pt->B = is_opaque;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transition_destroy(CapyPDF_Transition *transition) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<Transition *>(transition);
    RETNOERR;
}

// Optional Content groups

CAPYPDF_PUBLIC CapyPDF_EC capy_optional_content_group_new(CapyPDF_OptionalContentGroup **out_ptr,
                                                          const char *name) CAPYPDF_NOEXCEPT {
    // FIXME check for ASCIIness (or even more strict?)
    auto *ocg = new OptionalContentGroup();
    ocg->name = name;
    *out_ptr = reinterpret_cast<CapyPDF_OptionalContentGroup *>(ocg);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_optional_content_group_destroy(CapyPDF_OptionalContentGroup *ocg)
    CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<OptionalContentGroup *>(ocg);
    RETNOERR;
}

// Graphics state

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_new(CapyPDF_GraphicsState **out_ptr)
    CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_GraphicsState *>(new GraphicsState);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_SMask(CapyPDF_GraphicsState *state,
                                                        CapyPDF_SoftMaskId smid) CAPYPDF_NOEXCEPT {
    auto *s = reinterpret_cast<GraphicsState *>(state);
    s->SMask = smid;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_CA(CapyPDF_GraphicsState *state,
                                                     double value) CAPYPDF_NOEXCEPT {
    auto *s = reinterpret_cast<GraphicsState *>(state);
    s->CA = LimitDouble{value};
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_ca(CapyPDF_GraphicsState *state,
                                                     double value) CAPYPDF_NOEXCEPT {
    auto *s = reinterpret_cast<GraphicsState *>(state);
    s->ca = LimitDouble{value};
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_BM(
    CapyPDF_GraphicsState *state, CapyPDF_Blend_Mode blendmode) CAPYPDF_NOEXCEPT {
    auto *s = reinterpret_cast<GraphicsState *>(state);
    s->BM = blendmode;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_op(CapyPDF_GraphicsState *state,
                                                     int32_t value) CAPYPDF_NOEXCEPT {
    auto *s = reinterpret_cast<GraphicsState *>(state);
    CHECK_BOOLEAN(value);
    s->op = value;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_OP(CapyPDF_GraphicsState *state,
                                                     int32_t value) CAPYPDF_NOEXCEPT {
    auto *s = reinterpret_cast<GraphicsState *>(state);
    CHECK_BOOLEAN(value);
    s->OP = value;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_OPM(CapyPDF_GraphicsState *state,
                                                      int32_t value) CAPYPDF_NOEXCEPT {
    auto *s = reinterpret_cast<GraphicsState *>(state);
    // Not actually boolean, but only values 0 and 1 are valid. See PDF spec 8.6.7.
    CHECK_BOOLEAN(value);
    s->OPM = value;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_set_TK(CapyPDF_GraphicsState *state,
                                                     int32_t value) CAPYPDF_NOEXCEPT {
    auto *s = reinterpret_cast<GraphicsState *>(state);
    CHECK_BOOLEAN(value);
    s->TK = value;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_graphics_state_destroy(CapyPDF_GraphicsState *state)
    CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<GraphicsState *>(state);
    RETNOERR;
}

// Transparency Groups.

CAPYPDF_PUBLIC CapyPDF_EC capy_transparency_group_properties_new(
    CapyPDF_TransparencyGroupProperties **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr =
        reinterpret_cast<CapyPDF_TransparencyGroupProperties *>(new TransparencyGroupProperties());
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transparency_group_properties_set_CS(
    CapyPDF_TransparencyGroupProperties *props, CapyPDF_Device_Colorspace cs) CAPYPDF_NOEXCEPT {
    auto *p = reinterpret_cast<TransparencyGroupProperties *>(props);
    p->CS = cs;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transparency_group_properties_set_I(
    CapyPDF_TransparencyGroupProperties *props, int32_t I) CAPYPDF_NOEXCEPT {
    CHECK_BOOLEAN(I);
    auto *p = reinterpret_cast<TransparencyGroupProperties *>(props);
    p->I = I;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transparency_group_properties_set_K(
    CapyPDF_TransparencyGroupProperties *props, int32_t K) CAPYPDF_NOEXCEPT {
    CHECK_BOOLEAN(K);
    auto *p = reinterpret_cast<TransparencyGroupProperties *>(props);
    p->K = K;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_transparency_group_properties_destroy(
    CapyPDF_TransparencyGroupProperties *props) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<TransparencyGroupProperties *>(props);
    RETNOERR;
}

// Raster images.

struct RasterImageBuilder {
    RawPixelImage i;
};

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_new(CapyPDF_RasterImageBuilder **out_ptr)
    CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_RasterImageBuilder *>(new RasterImageBuilder);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_set_size(CapyPDF_RasterImageBuilder *builder,
                                                             uint32_t w,
                                                             uint32_t h) CAPYPDF_NOEXCEPT {
    auto *b = reinterpret_cast<RasterImageBuilder *>(builder);
    b->i.md.w = w;
    b->i.md.h = h;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_set_pixel_data(
    CapyPDF_RasterImageBuilder *builder, const char *buf, uint64_t bufsize) CAPYPDF_NOEXCEPT {
    auto *b = reinterpret_cast<RasterImageBuilder *>(builder);
    b->i.pixels.assign(buf, buf + bufsize);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_set_compression(
    CapyPDF_RasterImageBuilder *builder, CapyPDF_Compression compression) CAPYPDF_NOEXCEPT {
    auto *b = reinterpret_cast<RasterImageBuilder *>(builder);
    b->i.md.compression = compression;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_build(
    CapyPDF_RasterImageBuilder *builder, CapyPDF_RasterImage **out_ptr) CAPYPDF_NOEXCEPT {
    auto *b = reinterpret_cast<RasterImageBuilder *>(builder);
    // FIXME. Check validity.
    *out_ptr = reinterpret_cast<CapyPDF_RasterImage *>(new RasterImage(std::move(b->i)));
    b->i = RawPixelImage{};
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_get_size(const CapyPDF_RasterImage *image,
                                                     uint32_t *w,
                                                     uint32_t *h) CAPYPDF_NOEXCEPT {
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
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_get_colorspace(
    const CapyPDF_RasterImage *image, CapyPDF_Image_Colorspace *out_ptr) CAPYPDF_NOEXCEPT {
    auto *i = reinterpret_cast<const RasterImage *>(image);
    if(auto *raw = std::get_if<RawPixelImage>(i)) {
        *out_ptr = raw->md.cs;
    } else {
        return conv_err(ErrorCode::UnsupportedFormat);
    }
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_has_profile(const CapyPDF_RasterImage *image,
                                                        int32_t *out_ptr) CAPYPDF_NOEXCEPT {
    auto *i = reinterpret_cast<const RasterImage *>(image);
    if(auto *raw = std::get_if<RawPixelImage>(i)) {
        *out_ptr = raw->icc_profile.empty() ? 0 : 1;
    } else {
        return conv_err(ErrorCode::UnsupportedFormat);
    }
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_builder_destroy(CapyPDF_RasterImageBuilder *builder)
    CAPYPDF_NOEXCEPT {
    auto *ri = reinterpret_cast<RasterImageBuilder *>(builder);
    delete ri;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_raster_image_destroy(CapyPDF_RasterImage *image) CAPYPDF_NOEXCEPT {
    auto *ri = reinterpret_cast<RasterImage *>(image);
    delete ri;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type2_function_new(double *domain,
                                                  int32_t domain_size,
                                                  const CapyPDF_Color *c1,
                                                  const CapyPDF_Color *c2,
                                                  double n,
                                                  CapyPDF_Function **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_Function *>(
        new PdfFunction{FunctionType2{std::vector<double>(domain, domain + domain_size),
                                      *reinterpret_cast<const Color *>(c1),
                                      *reinterpret_cast<const Color *>(c2),
                                      n}});
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_function_destroy(CapyPDF_Function *func) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<PdfFunction *>(func);
    RETNOERR;
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
    *out_ptr = reinterpret_cast<CapyPDF_Function *>(new PdfFunction{
        FunctionType3{std::vector<double>(domain, domain + domain_size),
                      std::vector<CapyPDF_FunctionId>(functions, functions + functions_size),
                      std::vector<double>(bounds, bounds + bounds_size),
                      std::vector<double>(encode, encode + encode_size)}});
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type4_function_new(double *domain,
                                                  int32_t domain_size,
                                                  double *range,
                                                  int32_t range_size,
                                                  const char *code,
                                                  CapyPDF_Function **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_Function *>(
        new PdfFunction{FunctionType4{std::vector<double>(domain, domain + domain_size),
                                      std::vector<double>(range, range + range_size),
                                      std::string{code}}});
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type2_shading_new(CapyPDF_Device_Colorspace cs,
                                                 double x0,
                                                 double y0,
                                                 double x1,
                                                 double y1,
                                                 CapyPDF_FunctionId func,
                                                 CapyPDF_Shading **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr =
        reinterpret_cast<CapyPDF_Shading *>(new PdfShading{ShadingType2{cs, x0, y0, x1, y1, func}});
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_shading_set_extend(CapyPDF_Shading *shade,
                                                  int32_t starting,
                                                  int32_t ending) CAPYPDF_NOEXCEPT {
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
}

CAPYPDF_PUBLIC CapyPDF_EC capy_shading_set_domain(CapyPDF_Shading *shade,
                                                  double starting,
                                                  double ending) CAPYPDF_NOEXCEPT {
    auto *sh = reinterpret_cast<PdfShading *>(shade);
    if(auto *ptr = std::get_if<ShadingType2>(sh)) {
        ptr->domain = ShadingDomain{starting, ending};
    } else if(auto *ptr = std::get_if<ShadingType3>(sh)) {
        ptr->domain = ShadingDomain{starting, ending};
    } else {
        return conv_err(ErrorCode::IncorrectFunctionType);
    }
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_shading_destroy(CapyPDF_Shading *shade) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<PdfShading *>(shade);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type3_shading_new(CapyPDF_Device_Colorspace cs,
                                                 double *coords,
                                                 CapyPDF_FunctionId func,
                                                 CapyPDF_Shading **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_Shading *>(new PdfShading{
        ShadingType3{cs, coords[0], coords[1], coords[2], coords[3], coords[4], coords[5], func}});
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type4_shading_new(CapyPDF_Device_Colorspace cs,
                                                 double minx,
                                                 double miny,
                                                 double maxx,
                                                 double maxy,
                                                 CapyPDF_Shading **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_Shading *>(
        new PdfShading{ShadingType4{{}, minx, miny, maxx, maxy, cs}});
    RETNOERR;
}

static ShadingPoint conv_shpoint(const double *coords, const Color *color) {
    ShadingPoint sp;
    sp.c = *color;
    sp.p.x = coords[0];
    sp.p.y = coords[1];
    return sp;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type4_shading_add_triangle(
    CapyPDF_Shading *shade, const double *coords, const CapyPDF_Color **color) CAPYPDF_NOEXCEPT {
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
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type4_shading_extend(CapyPDF_Shading *shade,
                                                    int32_t flag,
                                                    const double *coords,
                                                    const CapyPDF_Color *color) CAPYPDF_NOEXCEPT {
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
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type6_shading_new(CapyPDF_Device_Colorspace cs,
                                                 double minx,
                                                 double miny,
                                                 double maxx,
                                                 double maxy,
                                                 CapyPDF_Shading **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_Shading *>(new PdfShading{ShadingType6{
        {},
        minx,
        miny,
        maxx,
        maxy,
        cs,
    }});
    RETNOERR;
}

template<typename T>
static void grab_coons_data(T &patch, const double *coords, const Color **colors) {
    for(int i = 0; i < (int)patch.p.size(); ++i) {
        patch.p[i].x = coords[2 * i];
        patch.p[i].y = coords[2 * i + 1];
    }
    for(int i = 0; i < (int)patch.c.size(); ++i) {
        patch.c[i] = *colors[i];
    }
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type6_shading_add_patch(
    CapyPDF_Shading *shade, const double *coords, const CapyPDF_Color **colors) CAPYPDF_NOEXCEPT {
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
}

CAPYPDF_PUBLIC CapyPDF_EC capy_type6_shading_extend(CapyPDF_Shading *shade,
                                                    int32_t flag,
                                                    const double *coords,
                                                    const CapyPDF_Color **colors) CAPYPDF_NOEXCEPT {
    auto *sh = reinterpret_cast<PdfShading *>(shade);
    auto *sh6 = std::get_if<ShadingType6>(sh);
    if(!sh6) {
        return conv_err(ErrorCode::IncorrectShadingType);
    }
    auto **cc = reinterpret_cast<const Color **>(colors);
    if(flag == 1 || flag == 2 || flag == 3) {
        if(sh6->elements.empty()) {
            return conv_err(ErrorCode::BadStripStart);
        }
        ContinuationCoonsPatch ccp;
        grab_coons_data(ccp, coords, cc);
        sh6->elements.emplace_back(std::move(ccp));
    } else {
        return conv_err(ErrorCode::BadEnum);
    }
    RETNOERR;
}

// Annotations

CAPYPDF_PUBLIC CapyPDF_EC capy_text_annotation_new(const char *utf8_text,
                                                   int32_t strsize,
                                                   CapyPDF_Annotation **out_ptr) CAPYPDF_NOEXCEPT {
    auto u8str = validate_utf8(utf8_text, strsize);
    if(!u8str) {
        return conv_err(u8str);
    }
    *out_ptr = reinterpret_cast<CapyPDF_Annotation *>(
        new Annotation{TextAnnotation{std::move(u8str.value())}, {}});
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_link_annotation_new(CapyPDF_Annotation **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_Annotation *>(new Annotation{LinkAnnotation{}, {}});
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_file_attachment_annotation_new(
    CapyPDF_EmbeddedFileId fid, CapyPDF_Annotation **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr =
        reinterpret_cast<CapyPDF_Annotation *>(new Annotation{FileAttachmentAnnotation{fid}, {}});
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_printers_mark_annotation_new(
    CapyPDF_FormXObjectId fid, CapyPDF_Annotation **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr =
        reinterpret_cast<CapyPDF_Annotation *>(new Annotation{PrintersMarkAnnotation{fid}, {}});
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_annotation_set_destination(
    CapyPDF_Annotation *annotation, const CapyPDF_Destination *d) CAPYPDF_NOEXCEPT {
    auto *a = reinterpret_cast<Annotation *>(annotation);
    auto *dest = reinterpret_cast<const Destination *>(d);
    if(auto *linka = std::get_if<LinkAnnotation>(&a->sub)) {
        linka->Dest = *dest;
        linka->URI.reset();
    } else {
        return conv_err(ErrorCode::IncorrectAnnotationType);
    }
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_annotation_set_uri(CapyPDF_Annotation *annotation,
                                                  const char *uri,
                                                  int32_t strsize) CAPYPDF_NOEXCEPT {
    auto *a = reinterpret_cast<Annotation *>(annotation);
    auto urirc = validate_ascii(uri, strsize);
    if(!urirc) {
        return conv_err(urirc);
    }
    if(auto *linka = std::get_if<LinkAnnotation>(&a->sub)) {
        linka->Dest.reset();
        linka->URI = std::move(urirc.value());
    } else {
        return conv_err(ErrorCode::IncorrectAnnotationType);
    }
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_annotation_set_rectangle(
    CapyPDF_Annotation *annotation, double x1, double y1, double x2, double y2) CAPYPDF_NOEXCEPT {
    auto *a = reinterpret_cast<Annotation *>(annotation);
    a->rect = PdfRectangle{x1, y1, x2, y2};
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_annotation_set_flags(
    CapyPDF_Annotation *annotation, CapyPDF_Annotation_Flags flags) CAPYPDF_NOEXCEPT {
    auto *a = reinterpret_cast<Annotation *>(annotation);
    a->flags = flags;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_annotation_destroy(CapyPDF_Annotation *annotation) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<Annotation *>(annotation);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_struct_item_extra_data_new(CapyPDF_StructItemExtraData **out_ptr)
    CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_StructItemExtraData *>(new StructItemExtraData());
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_struct_item_extra_data_set_t(CapyPDF_StructItemExtraData *extra,
                                                            const char *ttext,
                                                            int32_t strsize) CAPYPDF_NOEXCEPT {
    auto *ed = reinterpret_cast<StructItemExtraData *>(extra);
    auto rc = validate_utf8(ttext, strsize);
    if(rc) {
        ed->T = std::move(rc.value());
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_struct_item_extra_data_set_lang(CapyPDF_StructItemExtraData *extra,
                                                               const char *lang,
                                                               int32_t strsize) CAPYPDF_NOEXCEPT {
    auto *ed = reinterpret_cast<StructItemExtraData *>(extra);
    auto rc = validate_ascii(lang, strsize);
    if(rc) {
        ed->Lang = std::move(rc.value());
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_struct_item_extra_data_set_alt(CapyPDF_StructItemExtraData *extra,
                                                              const char *alt,
                                                              int32_t strsize) CAPYPDF_NOEXCEPT {
    auto *ed = reinterpret_cast<StructItemExtraData *>(extra);
    auto rc = validate_utf8(alt, strsize);
    if(rc) {
        ed->Alt = std::move(rc.value());
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_struct_item_extra_data_set_actual_text(
    CapyPDF_StructItemExtraData *extra, const char *actual, int32_t strsize) CAPYPDF_NOEXCEPT {
    auto *ed = reinterpret_cast<StructItemExtraData *>(extra);
    auto rc = validate_utf8(actual, strsize);
    if(rc) {
        ed->ActualText = std::move(rc.value());
    }
    return conv_err(rc);
}

CAPYPDF_PUBLIC CapyPDF_EC capy_struct_item_extra_data_destroy(CapyPDF_StructItemExtraData *extra)
    CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<StructItemExtraData *>(extra);
    RETNOERR;
}

// Image load

CAPYPDF_PUBLIC CapyPDF_EC capy_image_pdf_properties_new(CapyPDF_ImagePdfProperties **out_ptr)
    CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_ImagePdfProperties *>(new ImagePDFProperties());
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_image_pdf_properties_set_mask(CapyPDF_ImagePdfProperties *par,
                                                             int32_t as_mask) CAPYPDF_NOEXCEPT {
    CHECK_BOOLEAN(as_mask);
    auto p = reinterpret_cast<ImagePDFProperties *>(par);
    p->as_mask = as_mask;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_image_pdf_properties_set_interpolate(
    CapyPDF_ImagePdfProperties *par, CapyPDF_Image_Interpolation interp) CAPYPDF_NOEXCEPT {
    auto p = reinterpret_cast<ImagePDFProperties *>(par);
    p->interp = interp;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_image_pdf_properties_destroy(CapyPDF_ImagePdfProperties *par)
    CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<ImagePDFProperties *>(par);
    RETNOERR;
}

// Destination

CAPYPDF_PUBLIC CapyPDF_EC capy_destination_new(CapyPDF_Destination **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_Destination *>(new Destination());
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_destination_set_page_fit(
    CapyPDF_Destination *dest, int32_t physical_page_number) CAPYPDF_NOEXCEPT {
    auto *d = reinterpret_cast<Destination *>(dest);
    if(physical_page_number < 0) {
        return conv_err(ErrorCode::InvalidPageNumber);
    }
    d->page = physical_page_number;
    d->loc = DestinationFit{};
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_destination_set_page_xyz(CapyPDF_Destination *dest,
                                                        int32_t physical_page_number,
                                                        double *x,
                                                        double *y,
                                                        double *z) CAPYPDF_NOEXCEPT {
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
}

CAPYPDF_PUBLIC CapyPDF_EC capy_destination_destroy(CapyPDF_Destination *dest) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<Destination *>(dest);
    RETNOERR;
}

// Outline

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_new(CapyPDF_Outline **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_Outline *>(new Outline{});
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_set_title(CapyPDF_Outline *outline,
                                                 const char *title,
                                                 int32_t strsize) CAPYPDF_NOEXCEPT {
    auto *o = reinterpret_cast<Outline *>(outline);
    auto u8str = validate_utf8(title, strsize);
    if(!u8str) {
        return conv_err(u8str);
    }
    o->title = std::move(u8str.value());
    RETNOERR;
}
CAPYPDF_PUBLIC CapyPDF_EC capy_outline_set_destination(
    CapyPDF_Outline *outline, const CapyPDF_Destination *dest) CAPYPDF_NOEXCEPT {
    auto *o = reinterpret_cast<Outline *>(outline);
    o->dest = *reinterpret_cast<const Destination *>(dest);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_set_C(CapyPDF_Outline *outline, double r, double g, double b)
    CAPYPDF_NOEXCEPT {
    auto *o = reinterpret_cast<Outline *>(outline);
    o->C = DeviceRGBColor{r, g, b};
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_set_F(CapyPDF_Outline *outline,
                                             uint32_t F) CAPYPDF_NOEXCEPT {
    auto *o = reinterpret_cast<Outline *>(outline);
    o->F = F;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_set_parent(CapyPDF_Outline *outline,
                                                  CapyPDF_OutlineId parent) CAPYPDF_NOEXCEPT {
    auto *o = reinterpret_cast<Outline *>(outline);
    o->parent = parent;
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_outline_destroy(CapyPDF_Outline *outline) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<Outline *>(outline);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_shading_pattern_new(
    CapyPDF_ShadingId shid, CapyPDF_ShadingPattern **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_ShadingPattern *>(new ShadingPattern{shid, {}});
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_shading_pattern_set_matrix(CapyPDF_ShadingPattern *shp,
                                                          double a,
                                                          double b,
                                                          double c,
                                                          double d,
                                                          double e,
                                                          double f) CAPYPDF_NOEXCEPT {
    auto *p = reinterpret_cast<ShadingPattern *>(shp);
    p->m = PdfMatrix{a, b, c, d, e, f};
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_shading_pattern_destroy(CapyPDF_ShadingPattern *shp)
    CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<ShadingPattern *>(shp);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_soft_mask_new(CapyPDF_Soft_Mask_Subtype subtype,
                                             CapyPDF_TransparencyGroupId trid,
                                             CapyPDF_SoftMask **out_ptr) CAPYPDF_NOEXCEPT {
    *out_ptr = reinterpret_cast<CapyPDF_SoftMask *>(new SoftMask{subtype, trid});
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_soft_mask_destroy(CapyPDF_SoftMask *sm) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<SoftMask *>(sm);
    RETNOERR;
}

// Embedded file

CAPYPDF_PUBLIC CapyPDF_EC capy_embedded_file_new(const char *path,
                                                 CapyPDF_EmbeddedFile **out_ptr) CAPYPDF_NOEXCEPT {
    std::filesystem::path fspath(path);
    auto pathless_name = fspath.filename();
    auto rc = u8string::from_cstr(pathless_name.string().c_str());
    if(!rc) {
        return conv_err(rc);
    }
    auto *eobj = new EmbeddedFile();
    eobj->path = std::move(fspath);
    eobj->pdfname = std::move(rc.value());
    *out_ptr = reinterpret_cast<CapyPDF_EmbeddedFile *>(eobj);
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_embedded_file_set_subtype(CapyPDF_EmbeddedFile *efile,
                                                         const char *subtype,
                                                         int32_t strsize) CAPYPDF_NOEXCEPT {
    auto *eobj = reinterpret_cast<EmbeddedFile *>(efile);
    auto rc = validate_ascii(subtype, strsize);
    if(!rc) {
        return conv_err(rc);
    }
    eobj->subtype = std::move(rc.value());
    RETNOERR;
}

CAPYPDF_PUBLIC CapyPDF_EC capy_embedded_file_destroy(CapyPDF_EmbeddedFile *efile) CAPYPDF_NOEXCEPT {
    delete reinterpret_cast<EmbeddedFile *>(efile);
    RETNOERR;
}

// Error handling.

const char *capy_error_message(CapyPDF_EC error_code) CAPYPDF_NOEXCEPT {
    return error_text((ErrorCode)error_code);
}
