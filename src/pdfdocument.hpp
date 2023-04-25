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

#include <optional>
#include <pdfcommon.hpp>
#include <fontsubsetter.hpp>
#include <pdfcolorconverter.hpp>
#include <imageops.hpp>

#include <string_view>
#include <vector>
#include <expected>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    TrueTypeFontFile fontdata;
};

struct PageOffsets {
    int32_t resource_obj_num;
    int32_t commands_obj_num;
    int32_t page_obj_num;
};

struct ImageSize {
    int32_t w;
    int32_t h;
};

struct ImageInfo {
    ImageSize s;
    int32_t obj;
};

struct FormXObjectInfo {
    int32_t xobj_num;
};

struct FontInfo {
    int32_t font_file_obj;
    int32_t font_descriptor_obj;
    int32_t font_obj;
    size_t font_index_tmp;
};

struct DummyIndexZero {};

struct FullPDFObject {
    std::string dictionary;
    std::string stream;
};

struct DeflatePDFObject {
    std::string unclosed_dictionary;
    std::string stream;
};

struct DelayedSubsetFontData {
    A4PDF_FontId fid;
    int32_t subset_id;
};

struct DelayedSubsetCMap {
    A4PDF_FontId fid;
    int32_t subset_id;
};

struct DelayedSubsetFontDescriptor {
    A4PDF_FontId fid;
    int32_t subfont_data_obj;
    int32_t subset_num;
};

struct DelayedSubsetFont {
    A4PDF_FontId fid;
    int32_t subfont_descriptor_obj;
    int32_t subfont_cmap_obj;
};

struct DelayedPages {};

struct DelayedPage {
    int32_t page_num;
    std::vector<A4PDF_FormWidgetId> used_form_widgets;
    std::vector<A4PDF_AnnotationId> used_annotations;
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
    int32_t stream_num;
    int32_t object_num;
    int32_t num_channels;
};

enum IntentSubtype {
    SUBTYPE_PDFX,
    SUBTYPE_PDFA,
    // SUBTYPE_PDFE,
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
    std::optional<IntentSubtype> subtype;
    std::string intent_condition_identifier;
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

struct DelayedCheckboxWidgetAnnotation {
    A4PDF_FormWidgetId widget;

    // Annotation dict values.
    PdfBox rect;
    A4PDF_FormXObjectId on;
    A4PDF_FormXObjectId off;
    // uint32_t F; // Annotation flags;

    // Field dict values.
    std::string T;
};

struct OutlineData {
    std::vector<Outline> items;
    std::unordered_map<int32_t, std::vector<int32_t>> children;
    std::unordered_map<int32_t, int32_t> parent;
};

struct EmbeddedFileObject {
    int32_t filespec_obj;
    int32_t contents_obj;
};

// Other types here.

struct FileAttachmentAnnotation {
    A4PDF_EmbeddedFileId fileid;
};

struct TextAnnotation {};

typedef std::variant<TextAnnotation, FileAttachmentAnnotation> AnnotationSubType;

struct DelayedAnnotation {
    A4PDF_AnnotationId id;
    PdfBox rect;
    std::string contents;
    AnnotationSubType sub;
};

typedef std::variant<DummyIndexZero,
                     FullPDFObject,
                     DeflatePDFObject,
                     DelayedSubsetFontData,
                     DelayedSubsetFontDescriptor,
                     DelayedSubsetCMap,
                     DelayedSubsetFont,
                     DelayedPages,
                     DelayedPage,
                     DelayedCheckboxWidgetAnnotation, // FIXME, convert to hold all widgets
                     DelayedAnnotation>
    ObjectType;

typedef std::variant<A4PDF_Colorspace, int32_t> ColorspaceType;

class PdfDocument {
public:
    static rvoe<PdfDocument> construct(const PdfGenerationData &d, PdfColorConverter cm);

    PdfDocument(PdfDocument &&o) = default;

    friend class PdfGen;
    friend class PdfDrawContext;

    rvoe<NoReturnValue> write_to_file(FILE *output_file);

    // Pages
    rvoe<NoReturnValue> add_page(std::string resource_data,
                                 std::string page_data,
                                 const std::unordered_set<A4PDF_FormWidgetId> &form_widgets,
                                 const std::unordered_set<A4PDF_AnnotationId> &annots);

    // Form XObjects
    void add_form_xobject(std::string xobj_data, std::string xobj_stream);

    // Colors
    SeparationId create_separation(std::string_view name, const DeviceCMYKColor &fallback);
    LabId add_lab_colorspace(const LabColorSpace &lab);
    rvoe<A4PDF_IccColorSpaceId> load_icc_file(const char *fname);

    // Fonts
    rvoe<A4PDF_FontId> load_font(FT_Library ft, const char *fname);
    rvoe<SubsetGlyph> get_subset_glyph(A4PDF_FontId fid, uint32_t glyph);
    uint32_t glyph_for_codepoint(FT_Face face, uint32_t ucs4);
    A4PDF_FontId get_builtin_font_id(A4PDF_Builtin_Fonts font);

    // Images
    rvoe<A4PDF_ImageId> load_image(const char *fname);
    rvoe<A4PDF_ImageId> embed_jpg(const char *fname);

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

    // Forms
    rvoe<A4PDF_FormWidgetId> create_form_checkbox(PdfBox loc,
                                                  A4PDF_FormXObjectId onstate,
                                                  A4PDF_FormXObjectId offstate,
                                                  std::string_view partial_name);

    // Raw files
    rvoe<A4PDF_EmbeddedFileId> embed_file(const char *fname);

    // Annotations.
    rvoe<A4PDF_AnnotationId>
    create_annotation(PdfBox rect, std::string contents, AnnotationSubType subtype);

    std::optional<double>
    glyph_advance(A4PDF_FontId fid, double pointsize, uint32_t codepoint) const;

private:
    PdfDocument(const PdfGenerationData &d, PdfColorConverter cm);
    rvoe<NoReturnValue> init();

    rvoe<NoReturnValue> write_to_file_impl();

    int32_t add_object(ObjectType object);

    int32_t image_object_number(A4PDF_ImageId iid) { return image_info.at(iid.id).obj; }
    int32_t font_object_number(A4PDF_FontId fid) { return font_objects.at(fid.id).font_obj; }
    int32_t separation_object_number(SeparationId sid) { return separation_objects.at(sid.id); }

    std::optional<A4PDF_IccColorSpaceId> find_icc_profile(std::string_view contents);
    A4PDF_IccColorSpaceId store_icc_profile(std::string_view contents, int32_t num_channels);

    rvoe<std::vector<uint64_t>> write_objects();

    rvoe<NoReturnValue> create_catalog();
    void create_output_intent();
    rvoe<int32_t> create_name_dict();
    rvoe<int32_t> create_outlines();
    std::vector<int32_t> write_pages();
    rvoe<NoReturnValue> write_delayed_page(const DelayedPage &p);

    rvoe<NoReturnValue> write_pages_root();
    rvoe<NoReturnValue> write_header();
    rvoe<NoReturnValue> generate_info_object();
    rvoe<NoReturnValue> write_cross_reference_table(const std::vector<uint64_t> &object_offsets);
    rvoe<NoReturnValue> write_trailer(int64_t xref_offset);

    rvoe<NoReturnValue> write_finished_object(int32_t object_number,
                                              std::string_view dict_data,
                                              std::string_view stream_data);
    rvoe<NoReturnValue> write_bytes(const char *buf,
                                    size_t buf_size); // With error checking.
    rvoe<NoReturnValue> write_bytes(std::string_view view) {
        return write_bytes(view.data(), view.size());
    }

    rvoe<NoReturnValue> write_subset_font_data(int32_t object_num,
                                               const DelayedSubsetFontData &ssfont);
    void write_subset_font_descriptor(int32_t object_num,
                                      const TtfFont &font,
                                      int32_t font_data_obj,
                                      int32_t subset_number);
    void write_subset_cmap(int32_t object_num, const FontThingy &font, int32_t subset_number);
    rvoe<NoReturnValue> write_subset_font(int32_t object_num,
                                          const FontThingy &font,
                                          int32_t subset,
                                          int32_t font_descriptor_obj,
                                          int32_t tounicode_obj);
    rvoe<NoReturnValue> write_checkbox_widget(int obj_num,
                                              const DelayedCheckboxWidgetAnnotation &checkbox);
    rvoe<NoReturnValue> write_annotation(int obj_num, const DelayedAnnotation &annotation);

    rvoe<A4PDF_ImageId> add_image_object(int32_t w,
                                         int32_t h,
                                         int32_t bits_per_component,
                                         ColorspaceType colorspace,
                                         std::optional<int32_t> smask_id,
                                         std::string_view uncompressed_bytes);

    rvoe<A4PDF_ImageId> process_rgb_image(const rgb_image &image);
    rvoe<A4PDF_ImageId> process_gray_image(const gray_image &image);
    rvoe<A4PDF_ImageId> process_mono_image(const mono_image &image);
    rvoe<A4PDF_ImageId> process_cmyk_image(const cmyk_image &image);

    void pad_subset_fonts();
    void pad_subset_until_space(std::vector<TTGlyphs> &subset_glyphs);

    PdfGenerationData opts;
    PdfColorConverter cm;
    std::vector<ObjectType> document_objects;
    std::vector<PageOffsets> pages; // Refers to object num.
    std::vector<ImageInfo> image_info;
    std::unordered_map<A4PDF_Builtin_Fonts, A4PDF_FontId> builtin_fonts;
    std::vector<FontInfo> font_objects;
    std::vector<int32_t> separation_objects;
    std::vector<FontThingy> fonts;
    OutlineData outlines;
    std::vector<IccInfo> icc_profiles;
    std::vector<FormXObjectInfo> form_xobjects;
    std::vector<int32_t> form_widgets;
    std::vector<EmbeddedFileObject> embedded_files;
    std::vector<int32_t> annotations;
    // A form widget can be used on one and only one page.
    std::unordered_map<A4PDF_FormWidgetId, int32_t> form_use;
    std::unordered_map<A4PDF_AnnotationId, int32_t> annotation_use;
    std::optional<A4PDF_IccColorSpaceId> output_profile;
    std::optional<int32_t> output_intent_object;
    int32_t pages_object;

    FILE *ofile = nullptr;
};

} // namespace A4PDF
