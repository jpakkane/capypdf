// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

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
    std::optional<int32_t> structparents;
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
    asciistring lang;
    bool is_tagged = false;
    CapyPDF_DeviceColorspace output_colorspace = CAPY_DEVICE_CS_RGB;
    ColorProfiles prof;
    std::optional<CapyPDF_PDFX_Type> xtype;
    std::string intent_condition_identifier;
    bool compress_streams = false;
};

struct Outline {
    u8string title;
    Destination dest;
    std::optional<CapyPDF_OutlineId> parent;
};

struct OutlineLimits {
    int32_t first;
    int32_t last;
};

class PdfGen;
class PdfDrawContext;
class PdfWriter;
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
    u8string content;
};

struct UriAnnotation {
    asciistring uri;
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

struct PrintersMarkAnnotation {
    CapyPDF_FormXObjectId appearance;
};

typedef std::variant<TextAnnotation,
                     FileAttachmentAnnotation,
                     UriAnnotation,
                     ScreenAnnotation,
                     PrintersMarkAnnotation>
    AnnotationSubType;

struct Annotation {
    AnnotationSubType sub;
    std::optional<PdfRectangle> rect;
    CapyPDF_AnnotationFlags flags{CAPY_ANNOTATION_FLAG_NONE};
};

struct DelayedAnnotation {
    CapyPDF_AnnotationId id;
    Annotation a;
};

struct DelayedStructItem {
    CapyPDF_StructureItemId sid;
};

// 14.7.2 table 355
struct StructItemExtraData {
    u8string T;
    asciistring Lang;
    u8string Alt;
    u8string ActualText;
};

struct StructItem {
    int32_t obj_id;
    std::variant<CapyPDF_StructureType, CapyPDF_RoleId> stype;
    std::optional<CapyPDF_StructureItemId> parent;
    StructItemExtraData extra;
};

struct StructureUsage {
    int32_t page_num;
    int32_t mcid_num;
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

struct RolemapEnty {
    std::string name;
    CapyPDF_StructureType builtin;
};

typedef std::variant<CapyPDF_ImageColorspace, CapyPDF_IccColorSpaceId> ImageColorspaceType;

class PdfDocument {
public:
    static rvoe<PdfDocument> construct(const PdfGenerationData &d, PdfColorConverter cm);

    PdfDocument(PdfDocument &&o) = default;

    friend class PdfGen;
    friend class PdfDrawContext;
    friend class PdfWriter;

    // Pages
    rvoe<NoReturnValue> add_page(std::string resource_dict,
                                 std::string unclosed_object_dict,
                                 std::string command_stream,
                                 const PageProperties &custom_props,
                                 const std::unordered_set<CapyPDF_FormWidgetId> &form_widgets,
                                 const std::unordered_set<CapyPDF_AnnotationId> &annots,
                                 const std::vector<CapyPDF_StructureItemId> &structs,
                                 const std::optional<Transition> &transition,
                                 const std::vector<SubPageNavigation> &subnav);

    // Form XObjects
    void add_form_xobject(std::string xobj_data, std::string xobj_stream);

    // Colors
    rvoe<CapyPDF_SeparationId> create_separation(const asciistring &name,
                                                 const DeviceCMYKColor &fallback);
    rvoe<CapyPDF_LabColorSpaceId> add_lab_colorspace(const LabColorSpace &lab);
    rvoe<CapyPDF_IccColorSpaceId> load_icc_file(const std::filesystem::path &fname);

    // Fonts
    rvoe<CapyPDF_FontId> load_font(FT_Library ft, const std::filesystem::path &fname);
    rvoe<SubsetGlyph> get_subset_glyph(CapyPDF_FontId fid, uint32_t glyph);
    uint32_t glyph_for_codepoint(FT_Face face, uint32_t ucs4);
    CapyPDF_FontId get_builtin_font_id(CapyPDF_Builtin_Fonts font);

    // Images
    rvoe<CapyPDF_ImageId> load_image(const std::filesystem::path &fname,
                                     CapyPDF_Image_Interpolation interpolate);
    rvoe<CapyPDF_ImageId> add_mask_image(RasterImage image, const ImagePDFProperties &params);
    rvoe<CapyPDF_ImageId> add_image(RasterImage image, const ImagePDFProperties &params);
    rvoe<CapyPDF_ImageId> embed_jpg(jpg_image jpg, CapyPDF_Image_Interpolation interpolate);

    // Graphics states
    rvoe<CapyPDF_GraphicsStateId> add_graphics_state(const GraphicsState &state);

    // Functions
    rvoe<CapyPDF_FunctionId> add_function(const FunctionType2 &func);

    // Shading
    rvoe<CapyPDF_ShadingId> add_shading(const ShadingType2 &shade);
    rvoe<CapyPDF_ShadingId> add_shading(const ShadingType3 &shade);
    rvoe<CapyPDF_ShadingId> add_shading(const ShadingType4 &shade);
    rvoe<CapyPDF_ShadingId> add_shading(const ShadingType6 &shade);

    // Patterns
    rvoe<CapyPDF_PatternId> add_pattern(PdfDrawContext &ctx);

    // Outlines
    rvoe<CapyPDF_OutlineId>
    add_outline(const u8string &title_utf8, const Destination &dest, std::optional<CapyPDF_OutlineId> parent);

    // Forms
    rvoe<CapyPDF_FormWidgetId> create_form_checkbox(PdfBox loc,
                                                    CapyPDF_FormXObjectId onstate,
                                                    CapyPDF_FormXObjectId offstate,
                                                    std::string_view partial_name);

    // Raw files
    rvoe<CapyPDF_EmbeddedFileId> embed_file(const std::filesystem::path &fname);

    // Annotations.
    rvoe<CapyPDF_AnnotationId> create_annotation(const Annotation &a);

    // Structure items
    rvoe<CapyPDF_StructureItemId> add_structure_item(const CapyPDF_StructureType stype,
                                                     std::optional<CapyPDF_StructureItemId> parent,
                                                     std::optional<StructItemExtraData> extra);
    rvoe<CapyPDF_StructureItemId> add_structure_item(const CapyPDF_RoleId role,
                                                     std::optional<CapyPDF_StructureItemId> parent,
                                                     std::optional<StructItemExtraData> extra);

    // Optional content groups
    rvoe<CapyPDF_OptionalContentGroupId> add_optional_content_group(const OptionalContentGroup &g);

    // Transparency groups
    rvoe<CapyPDF_TransparencyGroupId> add_transparency_group(PdfDrawContext &ctx,
                                                             const TransparencyGroupExtra *ex);

    std::optional<double>
    glyph_advance(CapyPDF_FontId fid, double pointsize, uint32_t codepoint) const;

    rvoe<int32_t> create_structure_parent_tree();

    rvoe<CapyPDF_RoleId> add_rolemap_entry(std::string name, CapyPDF_StructureType builtin_type);

private:
    PdfDocument(const PdfGenerationData &d, PdfColorConverter cm);
    rvoe<NoReturnValue> init();

    int32_t add_object(ObjectType object);

    int32_t create_subnavigation(const std::vector<SubPageNavigation> &subnav);

    int32_t image_object_number(CapyPDF_ImageId iid) { return image_info.at(iid.id).obj; }
    int32_t font_object_number(CapyPDF_FontId fid) { return font_objects.at(fid.id).font_obj; }
    int32_t separation_object_number(CapyPDF_SeparationId sid) {
        return separation_objects.at(sid.id);
    }
    int32_t ocg_object_number(CapyPDF_OptionalContentGroupId ocgid) {
        return ocg_items.at(ocgid.id);
    }

    std::optional<CapyPDF_IccColorSpaceId> find_icc_profile(std::string_view contents);
    CapyPDF_IccColorSpaceId store_icc_profile(std::string_view contents, int32_t num_channels);

    rvoe<NoReturnValue> create_catalog();

    void create_output_intent();
    rvoe<int32_t> create_name_dict();
    rvoe<int32_t> create_outlines();
    void create_structure_root_dict();

    rvoe<CapyPDF_ImageId> add_image_object(int32_t w,
                                           int32_t h,
                                           int32_t bits_per_component,
                                           ImageColorspaceType colorspace,
                                           std::optional<int32_t> smask_id,
                                           const ImagePDFProperties &params,
                                           std::string_view uncompressed_bytes);

    rvoe<NoReturnValue> generate_info_object();
    int32_t create_page_group();
    void pad_subset_fonts();
    void pad_subset_until_space(std::vector<TTGlyphs> &subset_glyphs);

    rvoe<NoReturnValue> validate_format(const RasterImage &ri) const;

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
    std::vector<RolemapEnty> rolemap;
    // A form widget can be used on one and only one page.
    std::unordered_map<CapyPDF_FormWidgetId, int32_t> form_use;
    std::unordered_map<CapyPDF_AnnotationId, int32_t> annotation_use;
    std::unordered_map<CapyPDF_StructureItemId, StructureUsage> structure_use;
    std::vector<std::vector<CapyPDF_StructureItemId>>
        structure_parent_tree_items; // FIXME should be a variant of some sort?
    std::optional<CapyPDF_IccColorSpaceId> output_profile;
    std::optional<int32_t> output_intent_object;
    std::optional<int32_t> structure_root_object;
    std::optional<int32_t> structure_parent_tree_object;
    int32_t pages_object;
    int32_t page_group_object;
    bool write_attempted = false;
};

} // namespace capypdf
