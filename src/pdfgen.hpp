/*
 * Copyright 2023 Jussi Pakkanen
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

#pragma once

#include <pdfdrawcontext.hpp>

#include <cstdio>
#include <cstdint>

#include <vector>
#include <string_view>
#include <optional>
#include <filesystem>

namespace capypdf {

class PdfGen;

struct DrawContextPopper {
    PdfGen *g;
    PdfDrawContext ctx;

    explicit DrawContextPopper(PdfGen *g,
                               PdfDocument *doc,
                               PdfColorConverter *cm,
                               CAPYPDF_Draw_Context_Type dtype)
        : g{g}, ctx{doc, cm, dtype} {}

    DrawContextPopper() = delete;
    DrawContextPopper(const DrawContextPopper &) = delete;

    ~DrawContextPopper();
};

class PdfGen {
public:
    static rvoe<std::unique_ptr<PdfGen>> construct(const std::filesystem::path &ofname,
                                                   const PdfGenerationData &d);
    PdfGen(PdfGen &&o) = default;
    ~PdfGen();

    rvoe<NoReturnValue> write();
    void new_page();

    rvoe<CapyPdF_ImageId> load_image(const std::filesystem::path &fname) {
        return pdoc.load_image(fname);
    }
    rvoe<CapyPdF_ImageId> load_mask_image(const std::filesystem::path &fname) {
        return pdoc.load_mask_image(fname);
    }
    rvoe<CapyPdF_ImageId> embed_jpg(const std::filesystem::path &fname) {
        return pdoc.embed_jpg(fname);
    }
    rvoe<CapyPdF_EmbeddedFileId> embed_file(const std::filesystem::path &fname) {
        return pdoc.embed_file(fname);
    }
    rvoe<CapyPdF_FontId> load_font(const std::filesystem::path &fname) {
        return pdoc.load_font(ft.get(), fname);
    };

    ImageSize get_image_info(CapyPdF_ImageId img_id) { return pdoc.image_info.at(img_id.id).s; }
    SeparationId create_separation(std::string_view name, const DeviceCMYKColor &fallback) {
        return pdoc.create_separation(name, fallback);
    }
    GstateId add_graphics_state(const GraphicsState &state) {
        return pdoc.add_graphics_state(state);
    }

    FunctionId add_function(const FunctionType2 &func) { return pdoc.add_function(func); }

    ShadingId add_shading(const ShadingType2 &shade) { return pdoc.add_shading(shade); }
    ShadingId add_shading(const ShadingType3 &shade) { return pdoc.add_shading(shade); }
    ShadingId add_shading(const ShadingType4 &shade) { return pdoc.add_shading(shade); }
    ShadingId add_shading(const ShadingType6 &shade) { return pdoc.add_shading(shade); }

    LabId add_lab_colorspace(const LabColorSpace &lab) { return pdoc.add_lab_colorspace(lab); }

    rvoe<CapyPdF_IccColorSpaceId> load_icc_file(const std::filesystem::path &fname) {
        return pdoc.load_icc_file(fname);
    }

    rvoe<CapyPdF_FormWidgetId> create_form_checkbox(PdfBox loc,
                                                    CapyPdF_FormXObjectId onstate,
                                                    CapyPdF_FormXObjectId offstate,
                                                    std::string_view partial_name) {
        return pdoc.create_form_checkbox(loc, onstate, offstate, partial_name);
    }

    rvoe<CapyPdF_AnnotationId> create_annotation(PdfRectangle rect, AnnotationSubType subtype) {
        return pdoc.create_annotation(rect, std::move(subtype));
    }

    DrawContextPopper guarded_page_context();
    PdfDrawContext *new_page_draw_context();

    PdfDrawContext new_form_xobject(double w, double h) {
        return PdfDrawContext(&this->pdoc, &pdoc.cm, CAPY_DC_FORM_XOBJECT, w, h);
    }

    ColorPatternBuilder new_color_pattern_builder(double w, double h);

    rvoe<PageId> add_page(PdfDrawContext &ctx);
    rvoe<CapyPdF_FormXObjectId> add_form_xobject(PdfDrawContext &ctx);
    rvoe<PatternId> add_pattern(ColorPatternBuilder &cp);

    OutlineId
    add_outline(std::string_view title_utf8, PageId dest, std::optional<OutlineId> parent) {
        return pdoc.add_outline(title_utf8, dest, parent);
    }

    rvoe<CapyPdF_StructureItemId>
    add_structure_item(std::string_view stype, std::optional<CapyPdF_StructureItemId> parent) {
        return pdoc.add_structure_item(stype, parent);
    }

    int32_t num_pages() const { return (int32_t)pdoc.pages.size(); }

    std::optional<double>
    glyph_advance(CapyPdF_FontId fid, double pointsize, uint32_t codepoint) const {
        return pdoc.glyph_advance(fid, pointsize, codepoint);
    }

    rvoe<double> utf8_text_width(const char *utf8_text, CapyPdF_FontId fid, double pointsize) const;

private:
    PdfGen(std::filesystem::path ofilename,
           std::unique_ptr<FT_LibraryRec_, FT_Error (*)(FT_LibraryRec_ *)> ft,
           PdfDocument pdoc)
        : ofilename(std::move(ofilename)), ft(std::move(ft)), pdoc(std::move(pdoc)) {}

    std::filesystem::path ofilename;
    std::unique_ptr<FT_LibraryRec_, FT_Error (*)(FT_LibraryRec_ *)> ft;
    PdfDocument pdoc;
};

struct GenPopper {
    std::unique_ptr<PdfGen> g;
    GenPopper(const std::filesystem::path &ofname, const PdfGenerationData &d) : g() {
        auto rc = PdfGen::construct(ofname, d);
        if(!rc) {
            fprintf(stderr, "%s\n", error_text(rc.error()));
            std::abort();
        }
        g = std::move(rc.value());
    }
    ~GenPopper() {
        auto rc = g->write();
        if(!rc) {
            fprintf(stderr, "%s\n", error_text(rc.error()));
            std::abort();
        }
    }
};

} // namespace capypdf
