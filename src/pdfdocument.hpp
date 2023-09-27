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

#include <filesystem>
#include <optional>
#include <pdfcommon.hpp>
#include <fontsubsetter.hpp>
#include <pdfcolorconverter.hpp>

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

namespace capypdf {

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

struct PageProperties {
    std::optional<PdfRectangle> mediabox;
    std::optional<PdfRectangle> cropbox;
    std::optional<PdfRectangle> bleedbox;
    std::optional<PdfRectangle> trimbox;
    std::optional<PdfRectangle> artbox;

    std::optional<double> user_unit;

    PageProperties merge_with(const PageProperties &o) const {
        PageProperties result = *this;
        if(o.mediabox) {
            result.mediabox = o.mediabox;
        }
        if(o.cropbox) {
            result.cropbox = o.cropbox;
        }
        if(o.bleedbox) {
            result.bleedbox = o.bleedbox;
        }
        if(o.trimbox) {
            result.trimbox = o.trimbox;
        }
        if(o.artbox) {
            result.artbox = o.artbox;
        }

        if(o.user_unit) {
            result.user_unit = o.user_unit;
        }

        return result;
    }
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
    CapyPDF_FontId fid;
    int32_t subset_id;
};

struct DelayedSubsetCMap {
    CapyPDF_FontId fid;
    int32_t subset_id;
};

struct DelayedSubsetFontDescriptor {
    CapyPDF_FontId fid;
    int32_t subfont_data_obj;
    int32_t subset_num;
};

struct DelayedSubsetFont {
    CapyPDF_FontId fid;
    int32_t subfont_descriptor_obj;
    int32_t subfont_cmap_obj;
};

struct DelayedPages {};

struct DelayedPage {
    int32_t page_num;
    std::vector<CapyPDF_FormWidgetId> used_form_widgets;
    std::vector<CapyPDF_AnnotationId> used_annotations;
    std::optional<Transition> transition;
    std::optional<int32_t> subnav_root;
    PageProperties custom_props;
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
    std::filesystem::path rgb_profile_file;
    std::filesystem::path gray_profile_file;
    std::filesystem::path cmyk_profile_file;
};

struct IccInfo {
    int32_t stream_num;
    int32_t object_num;
    int32_t num_channels;
};

struct PdfGenerationData {
    PdfGenerationData() { default_page_properties.mediabox = PdfRectangle::a4(); }

    PageProperties default_page_properties;
    u8string title;
    u8string author;
    u8string creator;
    CapyPDF_Colorspace output_colorspace = CAPYPDF_CS_DEVICE_RGB;
    ColorProfiles prof;
    std::optional<CAPYPDF_Intent_Subtype> subtype;
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
    CapyPDF_FormWidgetId widget;

    // Annotation dict values.
    PdfBox rect;
    CapyPDF_FormXObjectId on;
    CapyPDF_FormXObjectId off;
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
    CapyPDF_EmbeddedFileId fileid;
};

struct TextAnnotation {
    std::string content;
};

struct UriAnnotation {
    std::string uri;
};

struct ClipTimes {
    double starttime;
    double endtime;
};

struct ScreenAnnotation {
    CapyPDF_EmbeddedFileId mediafile;
    std::string mimetype;
    std::optional<ClipTimes> times;
};

typedef std::variant<TextAnnotation, FileAttachmentAnnotation, UriAnnotation, ScreenAnnotation>
    AnnotationSubType;

struct DelayedAnnotation {
    CapyPDF_AnnotationId id;
    PdfRectangle rect;
    AnnotationSubType sub;
};

struct DelayedStructItem {
    CapyPDF_StructureItemId sid;
};

struct StructItem {
    int32_t obj_id;
    std::string stype;
    std::optional<CapyPDF_StructureItemId> parent;
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
                     DelayedAnnotation,
                     DelayedStructItem>
    ObjectType;

typedef std::variant<CapyPDF_Colorspace, int32_t> ColorspaceType;

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
                                 const PageProperties &custom_props,
                                 const std::unordered_set<CapyPDF_FormWidgetId> &form_widgets,
                                 const std::unordered_set<CapyPDF_AnnotationId> &annots,
                                 const std::unordered_set<CapyPDF_StructureItemId> &structs,
                                 const std::optional<Transition> &transition,
                                 const std::vector<SubPageNavigation> &subnav);

    // Form XObjects
    void add_form_xobject(std::string xobj_data, std::string xobj_stream);

    // Colors
    SeparationId create_separation(std::string_view name, const DeviceCMYKColor &fallback);
    LabId add_lab_colorspace(const LabColorSpace &lab);
    rvoe<CapyPDF_IccColorSpaceId> load_icc_file(const std::filesystem::path &fname);

    // Fonts
    rvoe<CapyPDF_FontId> load_font(FT_Library ft, const std::filesystem::path &fname);
    rvoe<SubsetGlyph> get_subset_glyph(CapyPDF_FontId fid, uint32_t glyph);
    uint32_t glyph_for_codepoint(FT_Face face, uint32_t ucs4);
    CapyPDF_FontId get_builtin_font_id(CapyPDF_Builtin_Fonts font);

    // Images
    rvoe<CapyPDF_ImageId> load_image(const std::filesystem::path &fname,
                                     enum CAPYPDF_Image_Interpolation interpolate);
    rvoe<CapyPDF_ImageId> add_mask_image(RasterImage image);
    rvoe<CapyPDF_ImageId>
    add_image(const std::filesystem::path &fname, RasterImage image, bool is_mask);
    rvoe<CapyPDF_ImageId> embed_jpg(jpg_image jpg,
                                    enum CAPYPDF_Image_Interpolation interpolate);

    // Graphics states
    rvoe<CapyPDF_GraphicsStateId> add_graphics_state(const GraphicsState &state);

    // Functions
    FunctionId add_function(const FunctionType2 &func);

    // Shading
    ShadingId add_shading(const ShadingType2 &shade);
    ShadingId add_shading(const ShadingType3 &shade);
    ShadingId add_shading(const ShadingType4 &shade);
    ShadingId add_shading(const ShadingType6 &shade);

    // Patterns
    PatternId add_pattern(std::string_view pattern_dict, std::string_view commands);

    // Outlines
    OutlineId
    add_outline(std::string_view title_utf8, PageId dest, std::optional<OutlineId> parent);

    // Forms
    rvoe<CapyPDF_FormWidgetId> create_form_checkbox(PdfBox loc,
                                                    CapyPDF_FormXObjectId onstate,
                                                    CapyPDF_FormXObjectId offstate,
                                                    std::string_view partial_name);

    // Raw files
    rvoe<CapyPDF_EmbeddedFileId> embed_file(const std::filesystem::path &fname);

    // Annotations.
    rvoe<CapyPDF_AnnotationId> create_annotation(PdfRectangle rect, AnnotationSubType subtype);

    // Structure items
    rvoe<CapyPDF_StructureItemId> add_structure_item(std::string_view stype,
                                                     std::optional<CapyPDF_StructureItemId> parent);

    // Optional content groups
    rvoe<CapyPDF_OptionalContentGroupId> add_optional_content_group(const OptionalContentGroup &g);

    // Transparency groups
    rvoe<CapyPDF_TransparencyGroupId> add_transparency_group(PdfDrawContext &ctx,
                                                             const TransparencyGroupExtra *ex);

    std::optional<double>
    glyph_advance(CapyPDF_FontId fid, double pointsize, uint32_t codepoint) const;

private:
    PdfDocument(const PdfGenerationData &d, PdfColorConverter cm);
    rvoe<NoReturnValue> init();

    rvoe<NoReturnValue> write_to_file_impl();

    int32_t add_object(ObjectType object);

    int32_t create_subnavigation(const std::vector<SubPageNavigation> &subnav);

    int32_t image_object_number(CapyPDF_ImageId iid) { return image_info.at(iid.id).obj; }
    int32_t font_object_number(CapyPDF_FontId fid) { return font_objects.at(fid.id).font_obj; }
    int32_t separation_object_number(SeparationId sid) { return separation_objects.at(sid.id); }
    int32_t ocg_object_number(CapyPDF_OptionalContentGroupId ocgid) {
        return ocg_items.at(ocgid.id);
    }

    std::optional<CapyPDF_IccColorSpaceId> find_icc_profile(std::string_view contents);
    CapyPDF_IccColorSpaceId store_icc_profile(std::string_view contents, int32_t num_channels);

    rvoe<std::vector<uint64_t>> write_objects();

    rvoe<NoReturnValue> create_catalog();
    void create_output_intent();
    rvoe<int32_t> create_name_dict();
    rvoe<int32_t> create_outlines();
    void create_structure_root_dict();
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
    rvoe<NoReturnValue>
    write_subset_cmap(int32_t object_num, const FontThingy &font, int32_t subset_number);
    rvoe<NoReturnValue> write_subset_font(int32_t object_num,
                                          const FontThingy &font,
                                          int32_t subset,
                                          int32_t font_descriptor_obj,
                                          int32_t tounicode_obj);
    rvoe<NoReturnValue> write_checkbox_widget(int obj_num,
                                              const DelayedCheckboxWidgetAnnotation &checkbox);
    rvoe<NoReturnValue> write_annotation(int obj_num, const DelayedAnnotation &annotation);
    rvoe<NoReturnValue> write_delayed_structure_item(int obj_num, const DelayedStructItem &p);

    rvoe<CapyPDF_ImageId> add_image_object(int32_t w,
                                           int32_t h,
                                           int32_t bits_per_component,
                                           enum CAPYPDF_Image_Interpolation interpolate,
                                           ColorspaceType colorspace,
                                           std::optional<int32_t> smask_id,
                                           bool is_mask,
                                           std::string_view uncompressed_bytes);

    int32_t create_page_group();
    void pad_subset_fonts();
    void pad_subset_until_space(std::vector<TTGlyphs> &subset_glyphs);

    PdfGenerationData opts;
    PdfColorConverter cm;
    std::vector<ObjectType> document_objects;
    std::vector<PageOffsets> pages; // Refers to object num.
    std::vector<ImageInfo> image_info;
    std::unordered_map<CapyPDF_Builtin_Fonts, CapyPDF_FontId> builtin_fonts;
    std::vector<FontInfo> font_objects;
    std::vector<int32_t> separation_objects;
    std::vector<FontThingy> fonts;
    OutlineData outlines;
    std::vector<IccInfo> icc_profiles;
    std::vector<FormXObjectInfo> form_xobjects;
    std::vector<int32_t> form_widgets;
    std::vector<EmbeddedFileObject> embedded_files;
    std::vector<int32_t> annotations;
    std::vector<StructItem> structure_items;
    std::vector<int32_t> ocg_items;
    std::vector<int32_t> transparency_groups;
    // A form widget can be used on one and only one page.
    std::unordered_map<CapyPDF_FormWidgetId, int32_t> form_use;
    std::unordered_map<CapyPDF_AnnotationId, int32_t> annotation_use;
    std::unordered_map<CapyPDF_StructureItemId, int32_t> structure_use;
    std::optional<CapyPDF_IccColorSpaceId> output_profile;
    std::optional<int32_t> output_intent_object;
    std::optional<int32_t> structure_root_object;
    int32_t pages_object;
    int32_t page_group_object;

    FILE *ofile = nullptr;
};

} // namespace capypdf
