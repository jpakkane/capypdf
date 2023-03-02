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
    case A4PDF_Page_Context:
        g->add_page(ctx);
        break;
    default:
        std::abort();
    }
}

PdfGen::PdfGen(const char *ofname, const PdfGenerationData &d) : ofilename(ofname), pdoc{d} {
    auto error = FT_Init_FreeType(&ft);
    if(error) {
        throw std::runtime_error(FT_Error_String(error));
    }
}

PdfGen::~PdfGen() {
    pdoc.font_objects.clear();
    pdoc.fonts.clear();
    auto error = FT_Done_FreeType(ft);
    if(error) {
        fprintf(stderr, "Closing FreeType failed: %s\n", FT_Error_String(error));
    }
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
        pdoc.write_to_file(ofile);
    } catch(const std::exception &e) {
        fprintf(stderr, "%s", e.what());
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

PageId PdfGen::add_page(PdfDrawContext &ctx) {
    if(ctx.draw_context_type() != A4PDF_Page_Context) {
        throw std::runtime_error("Tried to pass a non-page context to add_page.");
    }
    auto sc = ctx.serialize();
    pdoc.add_page(std::move(sc.dict), std::move(sc.commands));
    ctx.clear();
    return PageId{(int32_t)pdoc.pages.size() - 1};
}

PatternId PdfGen::add_pattern(ColorPatternBuilder &cp) {
    if(cp.pctx.draw_context_type() != A4PDF_Color_Tiling_Pattern_Context) {
        throw std::runtime_error("Tried to pass an incorrect pattern type to add_pattern.");
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

} // namespace A4PDF
