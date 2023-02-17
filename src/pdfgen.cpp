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
    FILE *ofile = fopen(ofilename.c_str(), "wb");
    if(!ofile) {
        perror("Could not open output file.");
        return;
    }
    try {
        pdoc.write_to_file(ofile);
    } catch(const std::exception &e) {
        fprintf(stderr, "%s", e.what());
    } catch(...) {
        fprintf(stderr, "Unexpected error.\n");
    }

    if(fflush(ofile) != 0) {
        perror("Writing output file failed");
    }
    if(fclose(ofile) != 0) {
        perror("Closing output file failed");
    }
    pdoc.font_objects.clear();
    pdoc.fonts.clear();
    auto error = FT_Done_FreeType(ft);
    if(error) {
        fprintf(stderr, "Closing FreeType failed: %s\n", FT_Error_String(error));
    }
}

void PdfGen::add_page(PdfDrawContext &ctx) {
    ctx.finalize();
    ctx.clear();
}

DrawContextPopper PdfGen::guarded_page_context() {
    return DrawContextPopper{this, &pdoc, &pdoc.cm, A4PDF_Page_Context};
}

PdfDrawContext *PdfGen::new_page_draw_context() {
    return new PdfDrawContext{&pdoc, &pdoc.cm, A4PDF_Page_Context};
}

} // namespace A4PDF
