// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#pragma once

#include <optional>
#include <pdfcommon.hpp>
#include <fontsubsetter.hpp>
#include <colorconverter.hpp>
#include <objectformatter.hpp>

#include <string_view>
#include <vector>
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

namespace capypdf::internal {

struct TtfFont {
    std::unique_ptr<FT_FaceRec_, FT_Error (*)(FT_Face)> face;
    TrueTypeFontFile fontdata;
};

struct PageOffsets {
    int32_t resource_obj_num;
    int32_t commands_obj_num;
    int32_t page_obj_num;
};

struct PageLabel {
    uint32_t start_page;
    std::optional<CapyPDF_Page_Label_Number_Style> style;
    std::optional<u8string> prefix;
    std::optional<uint32_t> start_num;
};

struct ImageSize {
    uint32_t w;
    uint32_t h;
};

struct ImageInfo {
    ImageSize s;
    int32_t obj;
};

struct FormXObjectInfo {
    int32_t xobj_num;
};

struct FontPDFObjects {
    int32_t font_file_obj;
    int32_t font_descriptor_obj;
    int32_t font_obj;
    int32_t cid_dictionary_obj;
    size_t font_index_tmp;
};

struct PageProperties {
    std::optional<PdfRectangle> mediabox;
    std::optional<PdfRectangle> cropbox;
    std::optional<PdfRectangle> bleedbox;
    std::optional<PdfRectangle> trimbox;
    std::optional<PdfRectangle> artbox;

    std::optional<double> user_unit;

    std::optional<TransparencyGroupProperties> transparency_props;

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

        if(o.transparency_props) {
            result.transparency_props = o.transparency_props;
        }

        return result;
    }
};

struct DummyIndexZero {};

struct FullPDFObject {
    std::string dictionary;
    RawData stream;
};

struct DeflatePDFObject {
    ObjectFormatter unclosed_dictionary;
    RawData stream;
    bool leave_uncompressed_in_debug = false;
};

struct DelayedSubsetFontData {
    CapyPDF_FontId fid;
};

struct DelayedSubsetCMap {
    CapyPDF_FontId fid;
};

struct DelayedSubsetFontDescriptor {
    CapyPDF_FontId fid;
    int32_t subfont_data_obj;
};

struct DelayedSubsetFont {
    CapyPDF_FontId fid;
    int32_t subfont_descriptor_obj;
    int32_t subfont_cmap_obj;
};

struct DelayedCIDDictionary {
    CapyPDF_FontId fid;
    int32_t subfont_descriptor_obj;
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
    uint16_t glyph_id;
};

struct FontThingy {
    TtfFont fontdata;
    FontSubsetter subsets;
};

struct IccInfo {
    int32_t stream_num;
    int32_t object_num;
    int32_t num_channels;
};

struct DocumentProperties {
    DocumentProperties();

    PdfVersion version() const;
    bool use_rdf_metadata() const;
    bool require_embedded_files() const;

    PageProperties default_page_properties;
    u8string title;
    u8string author;
    u8string creator;
    asciistring lang;
    bool is_tagged = false;
    CapyPDF_Device_Colorspace output_colorspace = CAPY_DEVICE_CS_RGB;
    ColorProfiles prof;
    std::variant<std::monostate, CapyPDF_PDFX_Type, CapyPDF_PDFA_Type> subtype;
    u8string metadata_xml;
    u8string intent_condition_identifier;
    bool compress_streams = true;
};

struct Outline {
    u8string title;
    std::optional<Destination> dest;
    std::optional<DeviceRGBColor> C;
    uint32_t F = 0;
    std::optional<CapyPDF_OutlineId> parent;
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

struct EmbeddedFile {
    std::string path;
    u8string pdfname;
    asciistring subtype; // actually MIME
    u8string description;
    // bool compress;
};

struct EmbeddedFileObject {
    EmbeddedFile ef;
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

struct LinkAnnotation {
    // This should really be an Action object but hove not
    // gotten around to implementing it yet.
    std::optional<asciistring> URI;
    std::optional<Destination> Dest;
    // HighlightType H
    // UriAction PA;
    // QuadPoints
    // BS
};

struct ClipTimes {
    double starttime;
    double endtime;
};

struct ScreenAnnotation {
    CapyPDF_EmbeddedFileId mediafile;
    asciistring mimetype;
    std::optional<ClipTimes> times;
};

struct PrintersMarkAnnotation {
    CapyPDF_FormXObjectId appearance;
};

typedef std::variant<TextAnnotation,
                     LinkAnnotation,
                     FileAttachmentAnnotation,
                     ScreenAnnotation,
                     PrintersMarkAnnotation>
    AnnotationSubType;

struct Annotation {
    AnnotationSubType sub;
    std::optional<PdfRectangle> rect;
    CapyPDF_Annotation_Flags flags{CAPY_ANNOTATION_FLAG_NONE};
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
    std::variant<CapyPDF_Structure_Type, CapyPDF_RoleId> stype;
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
                     DelayedCIDDictionary,
                     DelayedPages,
                     DelayedPage,
                     DelayedCheckboxWidgetAnnotation, // FIXME, convert to hold all widgets
                     DelayedAnnotation,
                     DelayedStructItem>
    ObjectType;

struct RolemapEnty {
    std::string name;
    CapyPDF_Structure_Type builtin;
};

struct FunctionInfo {
    PdfFunction original;
    int32_t object_number;
};

struct ShadingInfo {
    PdfShading original;
    int32_t object_number;
};

typedef std::variant<CapyPDF_Image_Colorspace, CapyPDF_IccColorSpaceId> ImageColorspaceType;

struct ImageObjectMetadata {
    uint32_t w = 0;
    uint32_t h = 0;
    uint32_t depth = 8;
    uint32_t alpha_depth = 0;
    ImageColorspaceType cs;
    CapyPDF_Compression compression;
    const std::vector<double> *decode = nullptr; // To avoid making a copy.

    void copy_common_from(const RawPixelImage &rmd) {
        w = rmd.md.w;
        h = rmd.md.h;
        cs = rmd.md.cs;
        compression = rmd.md.compression;
        decode = &rmd.decode;
    }
};

// Not really the best place for this but it'll do for now.
rvoe<NoReturnValue>
serialize_destination(ObjectFormatter &fmt, const Destination &dest, int32_t page_object_number);

class PdfDocument {
public:
    static rvoe<PdfDocument> construct(const DocumentProperties &d, PdfColorConverter cm);

    PdfDocument(PdfDocument &&o) = default;

    friend class PdfGen;
    friend class PdfDrawContext;
    friend class PdfWriter;

    // Pages
    rvoe<NoReturnValue> add_page(std::string resource_dict,
                                 std::string command_stream,
                                 const PageProperties &custom_props,
                                 const std::unordered_set<CapyPDF_FormWidgetId> &form_widgets,
                                 const std::unordered_set<CapyPDF_AnnotationId> &annots,
                                 const std::vector<CapyPDF_StructureItemId> &structs,
                                 const std::optional<Transition> &transition,
                                 const std::vector<SubPageNavigation> &subnav);

    rvoe<NoReturnValue> add_page_labeling(uint32_t start_page,
                                          std::optional<CapyPDF_Page_Label_Number_Style> style,
                                          std::optional<u8string> prefix,
                                          std::optional<uint32_t> start_num);

    // Form XObjects
    void add_form_xobject(ObjectFormatter xobj_data, std::string xobj_stream);

    // Colors
    rvoe<CapyPDF_SeparationId> create_separation(const asciistring &name,
                                                 CapyPDF_Device_Colorspace cs,
                                                 CapyPDF_FunctionId fid);
    rvoe<CapyPDF_LabColorSpaceId> add_lab_colorspace(const LabColorSpace &lab);
    rvoe<CapyPDF_IccColorSpaceId> load_icc_file(const char *fname);
    rvoe<CapyPDF_IccColorSpaceId> add_icc_profile(std::span<std::byte> contents,
                                                  int32_t num_channels);

    // Fonts
    rvoe<CapyPDF_FontId> load_font(FT_Library ft, const char *fname, FontProperties props);
    bool font_has_character(CapyPDF_FontId fid, uint32_t codepoint);
    bool font_has_character(FT_Face face, uint32_t codepoint);
    rvoe<SubsetGlyph> get_subset_glyph(CapyPDF_FontId fid,
                                       uint32_t codepoint,
                                       const std::optional<uint32_t> glyph_id);
    rvoe<SubsetGlyph> get_subset_glyph(CapyPDF_FontId fid, const u8string &text, uint32_t glyph_id);
    uint32_t glyph_for_codepoint(FT_Face face, uint32_t ucs4);
    CapyPDF_FontId get_builtin_font_id(CapyPDF_Builtin_Fonts font);

    // Images
    rvoe<CapyPDF_ImageId> load_image(const char *fname, CapyPDF_Image_Interpolation interpolate);
    rvoe<CapyPDF_ImageId> add_mask_image(RawPixelImage image, const ImagePDFProperties &params);
    rvoe<CapyPDF_ImageId> add_image(RawPixelImage image, const ImagePDFProperties &params);
    rvoe<CapyPDF_ImageId> embed_jpg(jpg_image jpg, const ImagePDFProperties &props);

    // Graphics states
    rvoe<CapyPDF_GraphicsStateId> add_graphics_state(const GraphicsState &state);

    // Functions
    rvoe<int32_t> serialize_function(const FunctionType2 &func);
    rvoe<int32_t> serialize_function(const FunctionType3 &func);
    rvoe<int32_t> serialize_function(const FunctionType4 &func);
    rvoe<CapyPDF_FunctionId> add_function(PdfFunction f);

    // Shading
    rvoe<int32_t> serialize_shading(const PdfShading &shade);
    rvoe<int32_t> serialize_shading(const ShadingType2 &shade);
    rvoe<int32_t> serialize_shading(const ShadingType3 &shade);
    rvoe<int32_t> serialize_shading(const ShadingType4 &shade);
    rvoe<int32_t> serialize_shading(const ShadingType6 &shade);
    rvoe<CapyPDF_ShadingId> add_shading(PdfShading sh);

    // Patterns
    rvoe<CapyPDF_PatternId> add_shading_pattern(const ShadingPattern &shp);
    rvoe<CapyPDF_PatternId> add_tiling_pattern(PdfDrawContext &ctx);

    // Outlines
    rvoe<CapyPDF_OutlineId> add_outline(const Outline &o);

    // Forms
    rvoe<CapyPDF_FormWidgetId> create_form_checkbox(PdfBox loc,
                                                    CapyPDF_FormXObjectId onstate,
                                                    CapyPDF_FormXObjectId offstate,
                                                    std::string_view partial_name);

    // Raw files
    rvoe<CapyPDF_EmbeddedFileId> embed_file(EmbeddedFile &ef);

    // Annotations.
    rvoe<CapyPDF_AnnotationId> add_annotation(const Annotation &a);

    // Structure itemsconst std::array<const char *, 3> colorspace_names
    rvoe<CapyPDF_StructureItemId> add_structure_item(const CapyPDF_Structure_Type stype,
                                                     std::optional<CapyPDF_StructureItemId> parent,
                                                     std::optional<StructItemExtraData> extra);
    rvoe<CapyPDF_StructureItemId> add_structure_item(const CapyPDF_RoleId role,
                                                     std::optional<CapyPDF_StructureItemId> parent,
                                                     std::optional<StructItemExtraData> extra);

    // Optional content groups
    rvoe<CapyPDF_OptionalContentGroupId> add_optional_content_group(const OptionalContentGroup &g);

    // Transparency groups
    rvoe<CapyPDF_TransparencyGroupId> add_transparency_group(PdfDrawContext &ctx);

    // Soft Mask
    rvoe<CapyPDF_SoftMaskId> add_soft_mask(const SoftMask &sm);

    std::optional<double>
    glyph_advance(CapyPDF_FontId fid, double pointsize, uint32_t codepoint) const;

    rvoe<int32_t> create_structure_parent_tree();

    rvoe<CapyPDF_RoleId> add_rolemap_entry(std::string name, CapyPDF_Structure_Type builtin_type);

    rvoe<CapyPDF_3DStreamId> add_3d_stream(ThreeDStream stream);

private:
    PdfDocument(const DocumentProperties &d, PdfColorConverter cm);
    rvoe<NoReturnValue> init();

    int32_t add_object(ObjectType object);

    int32_t create_subnavigation(const std::vector<SubPageNavigation> &subnav);

    int32_t image_object_number(CapyPDF_ImageId iid) { return get(iid).obj; }
    int32_t font_object_number(CapyPDF_FontId fid) { return get(fid).font_obj; }
    int32_t separation_object_number(CapyPDF_SeparationId sid) {
        return separation_objects.at(sid.id);
    }
    int32_t ocg_object_number(CapyPDF_OptionalContentGroupId ocgid) {
        return ocg_items.at(ocgid.id);
    }

    std::optional<CapyPDF_IccColorSpaceId> find_icc_profile(std::span<std::byte> contents);

    rvoe<NoReturnValue> create_catalog();

    void create_output_intent();
    rvoe<int32_t> create_name_dict();
    rvoe<int32_t> create_AF_dict();
    rvoe<int32_t> create_outlines();
    void create_structure_root_dict();

    rvoe<CapyPDF_ImageId> add_image_object(const ImageObjectMetadata &md,
                                           std::optional<int32_t> smask_id,
                                           const ImagePDFProperties &params,
                                           std::span<std::byte> original_bytes);

    rvoe<NoReturnValue> generate_info_object();
    int32_t add_document_metadata_object();

    rvoe<NoReturnValue> validate_format(const RawPixelImage &ri) const;

    // Typed getters for less typing.
    IccInfo &get(CapyPDF_IccColorSpaceId id) { return icc_profiles.at(id.id); }
    const IccInfo &get(CapyPDF_IccColorSpaceId id) const { return icc_profiles.at(id.id); }
    EmbeddedFileObject &get(CapyPDF_EmbeddedFileId id) { return embedded_files.at(id.id); }
    const EmbeddedFileObject &get(CapyPDF_EmbeddedFileId id) const {
        return embedded_files.at(id.id);
    }
    ImageInfo &get(CapyPDF_ImageId id) { return image_info.at(id.id); }
    const ImageInfo &get(CapyPDF_ImageId id) const { return image_info.at(id.id); }
    FontPDFObjects &get(CapyPDF_FontId id) { return font_objects.at(id.id); }
    const FontPDFObjects &get(CapyPDF_FontId id) const { return font_objects.at(id.id); }

    DocumentProperties docprops;
    PdfColorConverter cm;
    std::vector<ObjectType> document_objects;
    std::vector<PageOffsets> pages; // Refers to object num.
    std::vector<PageLabel> page_labels;
    std::vector<ImageInfo> image_info;
    std::unordered_map<CapyPDF_Builtin_Fonts, CapyPDF_FontId> builtin_fonts;
    std::vector<FontPDFObjects> font_objects;
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
    std::vector<int32_t> soft_masks;
    std::vector<FunctionInfo> functions;
    std::vector<ShadingInfo> shadings;
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
    std::optional<int32_t> document_md_object;
    int32_t pages_object;
    bool write_attempted = false;
};

} // namespace capypdf::internal
