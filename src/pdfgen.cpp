// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <pdfgen.hpp>
#include <pdfwriter.hpp>
#include <imagefileops.hpp>
#include <utils.hpp>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <stdexcept>
#include <array>
#include <fmt/core.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include FT_OPENTYPE_VALIDATE_H

namespace capypdf {

DrawContextPopper::~DrawContextPopper() {
    switch(ctx.draw_context_type()) {
    case CAPY_DC_PAGE: {
        auto rc = g->add_page(ctx);
        if(!rc) {
            fprintf(stderr, "%s\n", error_text(rc.error()));
            std::abort();
        }
        break;
    }
    case CAPY_DC_FORM_XOBJECT: {
        // Adding a form xobject automatically makes no sense,
        // since you need to have the return value id to use it.
        break;
    }
    default:
        std::abort();
    }
}

rvoe<std::unique_ptr<PdfGen>> PdfGen::construct(const std::filesystem::path &ofname,
                                                const PdfGenerationData &d) {
    FT_Library ft_;
    auto error = FT_Init_FreeType(&ft_);
    if(error) {
        RETERR(FreeTypeError);
    }
    std::unique_ptr<FT_LibraryRec_, FT_Error (*)(FT_LibraryRec_ *)> ft(ft_, FT_Done_FreeType);
    ERC(cm,
        PdfColorConverter::construct(
            d.prof.rgb_profile_file, d.prof.gray_profile_file, d.prof.cmyk_profile_file));
    ERC(pdoc, PdfDocument::construct(d, std::move(cm)));
    return std::unique_ptr<PdfGen>(new PdfGen(ofname, std::move(ft), std::move(pdoc)));
}

PdfGen::~PdfGen() {
    pdoc.font_objects.clear();
    pdoc.fonts.clear();
}

rvoe<NoReturnValue> PdfGen::write() {
    PdfWriter pwriter(pdoc);
    return pwriter.write_to_file(ofilename);
}

rvoe<CapyPDF_ImageId> PdfGen::load_image(const std::filesystem::path &fname,
                                         const ImageLoadParameters &p) {
    ERC(image, load_image_file(fname));

    if(p.as_mask) {
        return pdoc.add_mask_image(image);
    } else {
        image.md.interp = p.interp;
        return pdoc.add_image(std::move(image), p);
    }
}

rvoe<CapyPDF_ImageId> PdfGen::embed_jpg(const std::filesystem::path &fname,
                                        CapyPDF_Image_Interpolation interpolate) {
    ERC(jpg, load_jpg(fname));
    return pdoc.embed_jpg(std::move(jpg), interpolate);
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
    auto sc_var = ctx.serialize();
    assert(std::holds_alternative<SerializedBasicContext>(sc_var));
    auto &sc = std::get<SerializedBasicContext>(sc_var);
    ERCV(pdoc.add_page(std::move(sc.resource_dict),
                       std::move(sc.unclosed_object_dict),
                       std::move(sc.command_stream),
                       ctx.get_custom_props(),
                       ctx.get_form_usage(),
                       ctx.get_annotation_usage(),
                       ctx.get_structure_usage(),
                       ctx.get_transition(),
                       ctx.get_subpage_navigation()));
    ctx.clear();
    return PageId{(int32_t)pdoc.pages.size() - 1};
}

rvoe<CapyPDF_FormXObjectId> PdfGen::add_form_xobject(PdfDrawContext &ctx) {
    if(ctx.draw_context_type() != CAPY_DC_FORM_XOBJECT) {
        RETERR(InvalidDrawContextType);
    }
    if(ctx.marked_content_depth() != 0) {
        RETERR(UnclosedMarkedContent);
    }
    auto sc_var = ctx.serialize();
    assert(std::holds_alternative<SerializedXObject>(sc_var));
    auto &sc = std::get<SerializedXObject>(sc_var);
    pdoc.add_form_xobject(std::move(sc.dict), std::move(sc.command_stream));
    ctx.clear();
    CapyPDF_FormXObjectId fxoid;
    fxoid.id = (int32_t)pdoc.form_xobjects.size() - 1;
    return rvoe<CapyPDF_FormXObjectId>{fxoid};
}

rvoe<CapyPDF_PatternId> PdfGen::add_pattern(PdfDrawContext &ctx) { return pdoc.add_pattern(ctx); }

DrawContextPopper PdfGen::guarded_page_context() {
    return DrawContextPopper{this,
                             &pdoc,
                             &pdoc.cm,
                             CAPY_DC_PAGE,
                             pdoc.opts.default_page_properties.mediabox->w(),
                             pdoc.opts.default_page_properties.mediabox->h()};
}

PdfDrawContext *PdfGen::new_page_draw_context() {
    return new PdfDrawContext{&pdoc,
                              &pdoc.cm,
                              CAPY_DC_PAGE,
                              pdoc.opts.default_page_properties.mediabox->w(),
                              pdoc.opts.default_page_properties.mediabox->h()};
}

PdfDrawContext PdfGen::new_color_pattern_builder(double w, double h) {
    return PdfDrawContext{&pdoc, &pdoc.cm, CAPY_DC_COLOR_TILING, w, h};
}

PdfDrawContext *PdfGen::new_color_pattern(double w, double h) {
    return new PdfDrawContext(&pdoc, &pdoc.cm, CAPY_DC_COLOR_TILING, w, h);
}

rvoe<double>
PdfGen::utf8_text_width(const u8string &txt, CapyPDF_FontId fid, double pointsize) const {
    if(txt.empty()) {
        return 0;
    }
    double w = 0;
    FT_Face face = pdoc.fonts.at(pdoc.font_objects.at(fid.id).font_index_tmp).fontdata.face.get();
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

} // namespace capypdf
