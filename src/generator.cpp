// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <generator.hpp>
#include <pdfwriter.hpp>
#include <imagefileops.hpp>
#include <utils.hpp>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include FT_OPENTYPE_VALIDATE_H

namespace capypdf::internal {

DrawContextPopper::~DrawContextPopper() {
    switch(ctx.draw_context_type()) {
    case CAPY_DC_PAGE: {
        auto rc = g->add_page(ctx);
        if(!rc) {
            fprintf(stderr, "%s\n", error_text(rc.error()));
            abort();
        }
        break;
    }
    case CAPY_DC_FORM_XOBJECT: {
        // Adding a form xobject automatically makes no sense,
        // since you need to have the return value id to use it.
        break;
    }
    default:
        abort();
    }
}

rvoe<pystd2025::unique_ptr<PdfGen>> PdfGen::construct(const pystd2025::Path &ofname,
                                                      const DocumentProperties &d) {
    FT_Library ft_;
    auto error = FT_Init_FreeType(&ft_);
    if(error) {
        RETERR(FreeTypeError);
    }
    pystd2025::unique_ptr<FT_LibraryRec_, FreetypeCloser> ft(ft_);
    ERC(cm,
        construct_colorconverter(
            d.prof.rgb_profile_file, d.prof.gray_profile_file, d.prof.cmyk_profile_file));
    ERC(pdoc, construct_document(d, pystd2025::move(cm)));
    return pystd2025::unique_ptr<PdfGen>(
        new PdfGen(ofname, pystd2025::move(ft), pystd2025::move(pdoc)));
}

PdfGen::~PdfGen() {
    pdoc.font_objects.clear();
    pdoc.fonts.clear();
}

rvoe<NoReturnValue> PdfGen::write() {
    PdfWriter pwriter(pdoc);
    return pwriter.write_to_file(ofilename);
}

rvoe<RasterImage> PdfGen::load_image(const pystd2025::Path &fname) {
    return load_image_file(fname);
}

rvoe<RasterImage> PdfGen::load_image(const char *buf, int64_t bufsize) {
    return load_image_from_memory(buf, bufsize);
}

rvoe<CapyPDF_ImageId> PdfGen::add_image(RasterImage image, const ImagePDFProperties &params) {
    if(auto *raster = image.get_if<RawPixelImage>()) {
        if(params.as_mask) {
            return pdoc.add_mask_image(pystd2025::move(*raster), params);
        } else {
            return pdoc.add_image(pystd2025::move(*raster), params);
        }
    } else if(auto *jpg = image.get_if<jpg_image>()) {
        return pdoc.embed_jpg(pystd2025::move(*jpg), params);
    } else {
        RETERR(Unreachable);
    }
}

rvoe<RawPixelImage> PdfGen::convert_image_to_cs(RawPixelImage image,
                                                CapyPDF_Device_Colorspace cs,
                                                CapyPDF_Rendering_Intent ri) const {
    return pdoc.cm.convert_image_to(image, cs, ri);
}

rvoe<PageId> PdfGen::add_page(PdfDrawContext &ctx) {
    if(&ctx.get_doc() != &pdoc) {
        RETERR(IncorrectDocumentForObject);
    }
    if(ctx.draw_context_type() != CAPY_DC_PAGE) {
        RETERR(InvalidDrawContextType);
    }
    if(ctx.marked_content_depth() != 0) {
        RETERR(UnclosedMarkedContent);
    }
    if(ctx.has_unclosed_state()) {
        RETERR(DrawStateEndMismatch);
    }
    ERC(sc_var, ctx.serialize());
    assert(sc_var.contains<SerializedBasicContext>());
    auto &sc = sc_var.get<SerializedBasicContext>();
    ERCV(pdoc.add_page(pystd2025::move(sc.resource_dict),
                       pystd2025::move(sc.command_stream),
                       ctx.get_custom_props(),
                       ctx.get_form_usage(),
                       ctx.get_annotation_usage(),
                       ctx.get_structure_usage(),
                       ctx.get_transition(),
                       ctx.get_subpage_navigation()));
    ctx.clear();
    return PageId{(int32_t)pdoc.pages.size() - 1};
}
rvoe<NoReturnValue>
PdfGen::add_page_labeling(uint32_t start_page,
                          pystd2025::Optional<CapyPDF_Page_Label_Number_Style> style,
                          pystd2025::Optional<pystd2025::U8String> prefix,
                          pystd2025::Optional<uint32_t> start_num) {
    return pdoc.add_page_labeling(start_page, style, prefix, start_num);
}

rvoe<CapyPDF_FormXObjectId> PdfGen::add_form_xobject(PdfDrawContext &ctx) {
    if(ctx.draw_context_type() != CAPY_DC_FORM_XOBJECT) {
        RETERR(InvalidDrawContextType);
    }
    if(ctx.marked_content_depth() != 0) {
        RETERR(UnclosedMarkedContent);
    }
    ERC(sc_var, ctx.serialize());
    assert(sc_var.contains<SerializedXObject>());
    auto &sc = sc_var.get<SerializedXObject>();
    pdoc.add_form_xobject(pystd2025::move(sc.dict), pystd2025::move(sc.command_stream));
    ctx.clear();
    CapyPDF_FormXObjectId fxoid;
    fxoid.id = (int32_t)pdoc.form_xobjects.size() - 1;
    return rvoe<CapyPDF_FormXObjectId>{fxoid};
}

rvoe<CapyPDF_PatternId> PdfGen::add_shading_pattern(const ShadingPattern &shp) {
    return pdoc.add_shading_pattern(shp);
}

rvoe<CapyPDF_PatternId> PdfGen::add_tiling_pattern(PdfDrawContext &ctx) {
    return pdoc.add_tiling_pattern(ctx);
}

DrawContextPopper PdfGen::guarded_page_context() {
    return DrawContextPopper{this,
                             &pdoc,
                             &pdoc.cm,
                             CAPY_DC_PAGE,
                             pdoc.docprops.default_page_properties.mediabox.value()};
}

PdfDrawContext *PdfGen::new_page_draw_context() {
    return new PdfDrawContext{
        &pdoc, &pdoc.cm, CAPY_DC_PAGE, pdoc.docprops.default_page_properties.mediabox.value()};
}

PdfDrawContext PdfGen::new_color_pattern_builder(const PdfRectangle &rect) {
    return PdfDrawContext{&pdoc, &pdoc.cm, CAPY_DC_COLOR_TILING, rect};
}

PdfDrawContext *PdfGen::new_color_pattern(const PdfRectangle &rect) {
    return new PdfDrawContext(&pdoc, &pdoc.cm, CAPY_DC_COLOR_TILING, rect);
}

rvoe<double> PdfGen::utf8_text_width(const pystd2025::U8String &txt,
                                     CapyPDF_FontId fid,
                                     double pointsize) const {
    if(txt.empty()) {
        return 0;
    }
    double w = 0;
    FT_Face face = pdoc.fonts.at(pdoc.get(fid).font_index_tmp).fontdata.face.get();
    if(!face) {
        RETERR(BuiltinFontNotSupported);
    }
    uint32_t previous_codepoint = -1;
    const bool has_kerning = FT_HAS_KERNING(face);
    for(const auto codepoint : txt) {
        if(has_kerning && previous_codepoint != (uint32_t)-1) {
            FT_Vector kerning;
            const auto index_left = FT_Get_Char_Index(face, previous_codepoint);
            const auto index_right = FT_Get_Char_Index(face, codepoint);
            auto ec = FT_Get_Kerning(face, index_left, index_right, FT_KERNING_DEFAULT, &kerning);
            if(ec != 0) {
                RETERR(FreeTypeError);
            }
            if(kerning.x != 0) {
                // None of the fonts I tested had kerning that Freetype recognized,
                // so don't know if this actually works.
                w += int(kerning.x) / face->units_per_EM;
            }
        }
        auto bob = glyph_advance(fid, pointsize, codepoint);
        assert(bob);
        w += *bob;
        previous_codepoint = codepoint;
    }
    return w;
}

} // namespace capypdf::internal
