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

#include <pdfcommon.hpp>
#include <fontsubsetter.hpp>
#include <pdfcolorconverter.hpp>
#include <imageops.hpp>

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <variant>

// To avoid pulling all of LittleCMS in this file.
typedef void *cmsHPROFILE;
// Ditto for Freetype
typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_FaceRec_ *FT_Face;
typedef int FT_Error;

FT_Error guarded_face_close(FT_Face face);

namespace A4PDF {

struct TtfFont {
    std::unique_ptr<FT_FaceRec_, FT_Error (*)(FT_Face)> face;
    std::string fontdata;
};

struct PageOffsets {
    int32_t resource_obj_num;
    int32_t commands_obj_num;
};

struct ImageSize {
    int32_t w;
    int32_t h;
};

struct ImageInfo {
    ImageSize s;
    int32_t obj;
};

struct FontInfo {
    int32_t font_file_obj;
    int32_t font_descriptor_obj;
    int32_t font_obj;
    size_t font_index_tmp;
};

struct FullPDFObject {
    std::string dictionary;
    std::string stream;
};

struct DelayedSubsetFontData {
    FontId fid;
    int32_t subset_id;
};

struct DelayedSubsetCMap {
    FontId fid;
    int32_t subset_id;
};

struct DelayedSubsetFontDescriptor {
    FontId fid;
    int32_t subfont_data_obj;
    int32_t subset_num;
};

struct DelayedSubsetFont {
    FontId fid;
    int32_t subfont_descriptor_obj;
    int32_t subfont_cmap_obj;
};

struct SubsetGlyph {
    FontSubset ss;
    uint8_t glyph_id;
};

struct FontThingy {
    TtfFont fontdata;
    FontSubsetter subsets;
};

struct ColorProfiles {
    const char *rgb_profile_file = nullptr;
    const char *gray_profile_file = nullptr;
    const char *cmyk_profile_file = nullptr;
};

struct IccInfo {
    int32_t object_num;
    int32_t num_channels;
};

struct PdfGenerationData {
    PdfBox mediabox = PdfBox::a4();
    std::optional<PdfBox> cropbox;
    std::optional<PdfBox> bleedbox;
    std::optional<PdfBox> trimbox;
    std::optional<PdfBox> artbox;

    std::string title;
    std::string author;
    A4PDF_Colorspace output_colorspace = A4PDF_DEVICE_RGB;
    ColorProfiles prof;
};

struct Outline {
    std::string title;
    PageId dest;
    std::optional<OutlineId> parent;
};

struct OutlineLimits {
    int32_t first;
    int32_t last;
};

class PdfGen;
class PdfDrawContext;
struct ColorPatternBuilder;

typedef std::variant<FullPDFObject,
                     DelayedSubsetFontData,
                     DelayedSubsetFontDescriptor,
                     DelayedSubsetCMap,
                     DelayedSubsetFont>
    ObjectType;

class PdfDocument {
public:
    PdfDocument(const PdfGenerationData &d);

    friend class PdfGen;
    friend class PdfDrawContext;

    void write_to_file(FILE *output_file);

    // Pages
    void add_page(std::string_view resource_data, std::string_view page_data);

    // Colors
    SeparationId create_separation(std::string_view name, const DeviceCMYKColor &fallback);
    LabId add_lab_colorspace(const LabColorSpace &lab);
    IccColorId load_icc_file(const char *fname);

    // Fonts
    FontId load_font(FT_Library ft, const char *fname);
    SubsetGlyph get_subset_glyph(FontId fid, uint32_t glyph);
    uint32_t glyph_for_codepoint(FT_Face face, uint32_t ucs4);
    FontId get_builtin_font_id(A4PDF_Builtin_Fonts font);

    // Images
    ImageId load_image(const char *fname);
    ImageId embed_jpg(const char *fname);

    // Graphics states
    GstateId add_graphics_state(const GraphicsState &state);

    // Functions
    FunctionId add_function(const FunctionType2 &func);

    // Shading
    ShadingId add_shading(const ShadingType2 &shade);
    ShadingId add_shading(const ShadingType3 &shade);

    // Patterns
    PatternId add_pattern(std::string_view pattern_dict, std::string_view commands);

    // Outlines
    OutlineId
    add_outline(std::string_view title_utf8, PageId dest, std::optional<OutlineId> parent);

private:
    int32_t add_object(ObjectType object);

    int32_t image_object_number(ImageId iid) { return image_info.at(iid.id).obj; }
    int32_t font_object_number(FontId fid) { return font_objects.at(fid.id).font_obj; }
    int32_t separation_object_number(SeparationId sid) { return separation_objects.at(sid.id); }

    int32_t store_icc_profile(std::string_view contents, int32_t num_channels);

    std::vector<uint64_t> write_objects();

    void create_catalog(const std::vector<int32_t> &);
    int32_t create_outlines(const std::vector<int32_t> &);
    std::vector<int32_t> write_pages();
    void write_header();
    void generate_info_object();
    void write_cross_reference_table(const std::vector<uint64_t> object_offsets);
    void write_trailer(int64_t xref_offset);

    void write_finished_object(int32_t object_number,
                               std::string_view dict_data,
                               std::string_view stream_data);
    void write_bytes(const char *buf, size_t buf_size); // With error checking.
    void write_bytes(std::string_view view) { write_bytes(view.data(), view.size()); }

    void write_subset_font_data(int32_t object_num, const DelayedSubsetFontData &ssfont);
    void write_subset_font_descriptor(int32_t object_num,
                                      const TtfFont &font,
                                      int32_t font_data_obj,
                                      int32_t subset_number);
    void write_subset_cmap(int32_t object_num, const FontThingy &font, int32_t subset_number);
    void write_subset_font(int32_t object_num,
                           const FontThingy &font,
                           int32_t subset,
                           int32_t font_descriptor_obj,
                           int32_t tounicode_obj);

    ImageId process_rgb_image(const rgb_image &image);
    ImageId process_gray_image(const gray_image &image);
    ImageId process_mono_image(const mono_image &image);

    OutlineLimits
    write_outline_tree(const std::vector<int32_t> &page_objects,
                       const std::unordered_map<int32_t, std::vector<int32_t>> &children,
                       int32_t node_id);

    PdfGenerationData opts;
    PdfColorConverter cm;
    std::vector<ObjectType> document_objects;
    std::vector<PageOffsets> pages; // Refers to object num.
    std::vector<ImageInfo> image_info;
    std::unordered_map<A4PDF_Builtin_Fonts, FontId> builtin_fonts;
    std::vector<FontInfo> font_objects;
    std::vector<int32_t> separation_objects;
    std::vector<FontThingy> fonts;
    std::vector<Outline> outlines;
    std::vector<IccInfo> icc_profiles;
    int32_t rgb_profile_obj, gray_profile_obj, cmyk_profile_obj;
    FILE *ofile = nullptr;
};

} // namespace A4PDF
