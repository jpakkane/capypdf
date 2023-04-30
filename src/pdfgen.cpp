/*
 * Copyright 2022 Jussi Pakkanen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <pdfgen.hpp>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <lcms2.h>
#include <iconv.h>
#include <stdexcept>
#include <array>
#include <fmt/core.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include FT_OPENTYPE_VALIDATE_H

namespace A4PDF {

LcmsHolder::~LcmsHolder() { deallocate(); }

void LcmsHolder::deallocate() {
    if(h) {
        cmsCloseProfile(h);
    }
    h = nullptr;
}

DrawContextPopper::~DrawContextPopper() {
    switch(ctx.draw_context_type()) {
    case A4PDF_Page_Context: {
        auto rc = g->add_page(ctx);
        if(!rc) {
            fprintf(stderr, "%s\n", error_text(rc.error()));
            std::abort();
        }
        break;
    }
    default:
        std::abort();
    }
}

rvoe<std::unique_ptr<PdfGen>> PdfGen::construct(const char *ofname, const PdfGenerationData &d) {
    FT_Library ft_;
    auto error = FT_Init_FreeType(&ft_);
    if(error) {
        RETERR(FreeTypeError);
    }
    std::unique_ptr<FT_LibraryRec_, FT_Error (*)(FT_LibraryRec_ *)> ft(ft_, FT_Done_FreeType);
    std::filesystem::path opath(ofname);
    ERC(cm,
        PdfColorConverter::construct(
            d.prof.rgb_profile_file, d.prof.gray_profile_file, d.prof.cmyk_profile_file));
    ERC(pdoc, PdfDocument::construct(d, std::move(cm)));
    return std::unique_ptr<PdfGen>(new PdfGen(std::move(opath), std::move(ft), std::move(pdoc)));
}

PdfGen::~PdfGen() {
    pdoc.font_objects.clear();
    pdoc.fonts.clear();
}

ErrorCode PdfGen::write() {
    if(pdoc.pages.size() == 0) {
        return ErrorCode::NoPages;
    }

    std::string tempfname = ofilename.c_str();
    tempfname += "~";
    FILE *ofile = fopen(tempfname.c_str(), "wb");
    if(!ofile) {
        perror(nullptr);
        return ErrorCode::CouldNotOpenFile;
    }

    try {
        auto rc = pdoc.write_to_file(ofile);
        if(!rc) {
            return rc.error();
        }
    } catch(const std::exception &e) {
        fprintf(stderr, "%s\n", e.what());
        fclose(ofile);
        return ErrorCode::DynamicError;
    } catch(...) {
        fprintf(stderr, "Unexpected error.\n");
        fclose(ofile);
        return ErrorCode::DynamicError;
    }

    if(fflush(ofile) != 0) {
        perror(nullptr);
        fclose(ofile);
        return ErrorCode::DynamicError;
    }
    if(fsync(fileno(ofile)) != 0) {
        perror(nullptr);
        fclose(ofile);
        return ErrorCode::FileWriteError;
    }
    if(fclose(ofile) != 0) {
        perror(nullptr);
        return ErrorCode::FileWriteError;
    }

    // If we made it here, the file has been fully written and fsynd'd to disk. Now replace.
    if(rename(tempfname.c_str(), ofilename.c_str()) != 0) {
        perror(nullptr);
        return ErrorCode::FileWriteError;
    }
    return ErrorCode::NoError;
}

rvoe<PageId> PdfGen::add_page(PdfDrawContext &ctx) {
    if(ctx.draw_context_type() != A4PDF_Page_Context) {
        RETERR(InvalidDrawContextType);
    }
    if(ctx.marked_content_depth() != 0) {
        RETERR(UnclosedMarkedContent);
    }
    auto sc_var = ctx.serialize();
    assert(std::holds_alternative<SerializedBasicContext>(sc_var));
    auto &sc = std::get<SerializedBasicContext>(sc_var);
    ERCV(pdoc.add_page(std::move(sc.dict),
                       std::move(sc.commands),
                       ctx.get_form_usage(),
                       ctx.get_annotation_usage(),
                       ctx.get_structure_usage()));
    ctx.clear();
    return PageId{(int32_t)pdoc.pages.size() - 1};
}

rvoe<A4PDF_FormXObjectId> PdfGen::add_form_xobject(PdfDrawContext &ctx) {
    if(ctx.draw_context_type() != A4PDF_Form_XObject_Context) {
        RETERR(InvalidDrawContextType);
    }
    if(ctx.marked_content_depth() != 0) {
        RETERR(UnclosedMarkedContent);
    }
    auto sc_var = ctx.serialize();
    assert(std::holds_alternative<SerializedXObject>(sc_var));
    auto &sc = std::get<SerializedXObject>(sc_var);
    pdoc.add_form_xobject(std::move(sc.dict), std::move(sc.stream));
    ctx.clear();
    A4PDF_FormXObjectId fxoid;
    fxoid.id = (int32_t)pdoc.form_xobjects.size() - 1;
    return rvoe<A4PDF_FormXObjectId>{fxoid};
}

rvoe<PatternId> PdfGen::add_pattern(ColorPatternBuilder &cp) {
    if(cp.pctx.draw_context_type() != A4PDF_Color_Tiling_Pattern_Context) {
        RETERR(InvalidDrawContextType);
    }
    if(cp.pctx.marked_content_depth() != 0) {
        RETERR(UnclosedMarkedContent);
    }
    auto resources = cp.pctx.build_resource_dict();
    auto commands = cp.pctx.get_command_stream();
    auto buf = fmt::format(R"(<<
  /Type /Pattern
  /PatternType 1
  /PaintType 1
  /TilingType 1
  /BBox [ {} {} {} {}]
  /XStep {}
  /YStep {}
  /Resources {}
  /Length {}
>>
)",
                           0,
                           0,
                           cp.w,
                           cp.h,
                           cp.w,
                           cp.h,
                           resources,
                           commands.length());

    return pdoc.add_pattern(buf, commands);
}

DrawContextPopper PdfGen::guarded_page_context() {
    return DrawContextPopper{this, &pdoc, &pdoc.cm, A4PDF_Page_Context};
}

PdfDrawContext *PdfGen::new_page_draw_context() {
    return new PdfDrawContext{&pdoc, &pdoc.cm, A4PDF_Page_Context};
}

ColorPatternBuilder PdfGen::new_color_pattern_builder(double w, double h) {
    return ColorPatternBuilder{
        PdfDrawContext{&pdoc, &pdoc.cm, A4PDF_Color_Tiling_Pattern_Context}, w, h};
}

rvoe<double>
PdfGen::utf8_text_width(const char *utf8_text, A4PDF_FontId fid, double pointsize) const {
    double w = 0;
    errno = 0;
    auto to_codepoint = iconv_open("UCS-4LE", "UTF-8");
    if(errno != 0) {
        perror(nullptr);
        RETERR(IconvError);
    }
    std::unique_ptr<void, int (*)(void *)> iconvcloser(to_codepoint, iconv_close);
    FT_Face face = pdoc.fonts.at(pdoc.font_objects.at(fid.id).font_index_tmp).fontdata.face.get();
    if(!face) {
        RETERR(BuiltinFontNotSupported);
    }

    uint32_t previous_codepoint = -1;
    auto in_ptr = (char *)utf8_text;
    const auto text_length = strlen(utf8_text);
    auto in_bytes = text_length;
    // Freetype does not support GPOS kerning because it is context-sensitive.
    // So this method might produce incorrect kerning. Users that need precision
    // need to use the glyph based rendering method.
    const bool has_kerning = FT_HAS_KERNING(face);
    if(has_kerning) {
        printf("HAS KERNING\n");
    }
    while(in_bytes > 0) {
        uint32_t codepoint{0};
        auto out_ptr = (char *)&codepoint;
        auto out_bytes = sizeof(codepoint);
        errno = 0;
        auto iconv_result = iconv(to_codepoint, &in_ptr, &in_bytes, &out_ptr, &out_bytes);
        if(iconv_result == (size_t)-1 && errno != E2BIG) {
            perror(nullptr);
            RETERR(IconvError);
        }
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

} // namespace A4PDF
