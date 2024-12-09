// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <document.hpp>
#include <utils.hpp>
#include <drawcontext.hpp>

#include <cassert>
#include <array>
#include <bit>
#include <filesystem>
#include <algorithm>
#include <format>
#include <ft2build.h>
#include <variant>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include FT_OPENTYPE_VALIDATE_H

namespace capypdf::internal {

const std::array<const char *, 3> colorspace_names{
    "/DeviceRGB",
    "/DeviceGray",
    "/DeviceCMYK",
};

const std::array<const char, 5> page_label_types{
    'D', // Decimal
    'R', // Roman Upper
    'r', // Roman Lower
    'A', // Letter Upper
    'a', // Letter Lower
};

namespace {

constexpr char pdfa_rdf_template[] = R"(<?xpacket begin="{}" id="W5M0MpCehiHzreSzNTczkc9d"?>
<x:xmpmeta xmlns:x="adobe:ns:meta/">
 <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <rdf:Description rdf:about="" xmlns:pdfaid="http://www.aiim.org/pdfa/ns/id/">
   <pdfaid:part>{}</pdfaid:part>
   <pdfaid:conformance>{}</pdfaid:conformance>
  </rdf:Description>
 </rdf:RDF>
</x:xmpmeta>
<?xpacket end="w"?>
)";

const unsigned char rdf_magic[4] = {0xef, 0xbb, 0xbf, 0};

// const std::array<const char *, 3> intentnames{"/GTS_PDFX", "/GTS_PDFA", "/ISO_PDFE"};

const std::array<const char *, 9> pdfx_names{
    "PDF/X-1:2001",
    "PDF/X-1a:2001",
    "PDF/X-1a:2003",
    "PDF/X-3:2002",
    "PDF/X-3:2003",
    "PDF/X-4",
    "PDF/X-4p",
    "PDF/X-5g",
    "PDF/X-5pg",
};

const std::array<const char, 12> pdfa_part{
    '1', '1', '2', '2', '2', '3', '3', '3', '4', '4', '4', '4'};
const std::array<const char, 12> pdfa_conformance{
    'A', 'B', 'A', 'B', 'U', 'A', 'B', 'U', 'A', 'B', 'F', 'E'};

FT_Error guarded_face_close(FT_Face face) {
    // Freetype segfaults if you give it a null pointer.
    if(face) {
        return FT_Done_Face(face);
    }
    return 0;
}

const std::array<const char *, 14> font_names{
    "Times-Roman",
    "Helvetica",
    "Courier",
    "Symbol",
    "Times-Roman-Bold",
    "Helvetica-Bold",
    "Courier-Bold",
    "ZapfDingbats",
    "Times-Italic",
    "Helvetica-Oblique",
    "Courier-Oblique",
    "Times-BoldItalic",
    "Helvetica-BoldOblique",
    "Courier-BoldOblique",
};

const std::array<const char *, 16> blend_mode_names{
    "Normal",
    "Multiply",
    "Screen",
    "Overlay",
    "Darken",
    "Lighten",
    "ColorDodge",
    "ColorBurn",
    "HardLight",
    "SoftLight",
    "Difference",
    "Exclusion",
    "Hue",
    "Saturation",
    "Color",
    "Luminosity",
};

template<typename T> rvoe<NoReturnValue> append_floatvalue(std::string &buf, double v) {
    if(v < 0 || v > 1.0) {
        RETERR(ColorOutOfRange);
    }
    T cval = (T)(std::numeric_limits<T>::max() * v);
    cval = std::byteswap(cval);
    const char *ptr = (const char *)(&cval);
    buf.append(ptr, ptr + sizeof(T));
    RETOK;
}

rvoe<std::string> serialize_shade4(const ShadingType4 &shade) {
    std::string s;
    for(const auto &e : shade.elements) {
        const double xratio =
            std::clamp((e.sp.p.x - shade.minx) / (shade.maxx - shade.minx), 0.0, 1.0);
        const double yratio =
            std::clamp((e.sp.p.y - shade.miny) / (shade.maxy - shade.miny), 0.0, 1.0);
        char flag = (char)e.flag;
        assert(flag >= 0 && flag < 3);
        const char *ptr;
        ptr = (const char *)(&flag);
        s.append(ptr, ptr + sizeof(char));
        ERCV(append_floatvalue<uint32_t>(s, xratio));
        ERCV(append_floatvalue<uint32_t>(s, yratio));

        if(auto *c = std::get_if<DeviceRGBColor>(&e.sp.c)) {
            if(shade.colorspace != CAPY_DEVICE_CS_RGB) {
                RETERR(ColorspaceMismatch);
            }
            ERCV(append_floatvalue<uint16_t>(s, c->r.v()));
            ERCV(append_floatvalue<uint16_t>(s, c->g.v()));
            ERCV(append_floatvalue<uint16_t>(s, c->b.v()));
        } else if(auto *c = std::get_if<DeviceGrayColor>(&e.sp.c)) {
            if(shade.colorspace != CAPY_DEVICE_CS_GRAY) {
                RETERR(ColorspaceMismatch);
            }
            ERCV(append_floatvalue<uint16_t>(s, c->v.v()));
        } else if(auto *c = std::get_if<DeviceCMYKColor>(&e.sp.c)) {
            if(shade.colorspace != CAPY_DEVICE_CS_CMYK) {
                RETERR(ColorspaceMismatch);
            }
            ERCV(append_floatvalue<uint16_t>(s, c->c.v()));
            ERCV(append_floatvalue<uint16_t>(s, c->m.v()));
            ERCV(append_floatvalue<uint16_t>(s, c->y.v()));
            ERCV(append_floatvalue<uint16_t>(s, c->k.v()));
        } else {
            fprintf(stderr, "Color space not supported yet.");
            std::abort();
        }
    }
    return rvoe<std::string>{std::move(s)};
}

rvoe<std::string> serialize_shade6(const ShadingType6 &shade) {
    std::string s;
    for(const auto &eh : shade.elements) {
        if(!std::holds_alternative<FullCoonsPatch>(eh)) {
            fprintf(stderr, "Continuation patches not yet supported.\n");
            std::abort();
        }
        const auto &e = std::get<FullCoonsPatch>(eh);
        char flag = 0; //(char)e.flag;
        assert(flag >= 0 && flag < 3);
        const char *ptr;
        ptr = (const char *)(&flag);
        s.append(ptr, ptr + sizeof(char));

        for(const auto &p : e.p) {
            const double xratio =
                std::clamp((p.x - shade.minx) / (shade.maxx - shade.minx), 0.0, 1.0);
            const double yratio =
                std::clamp((p.y - shade.miny) / (shade.maxy - shade.miny), 0.0, 1.0);

            ERCV(append_floatvalue<uint32_t>(s, xratio));
            ERCV(append_floatvalue<uint32_t>(s, yratio));
        }
        for(const auto &colorobj : e.c) {
            if(shade.colorspace == CAPY_DEVICE_CS_RGB) {
                if(!std::holds_alternative<DeviceRGBColor>(colorobj)) {
                    RETERR(ColorspaceMismatch);
                }
                const auto &c = std::get<DeviceRGBColor>(colorobj);
                ERCV(append_floatvalue<uint16_t>(s, c.r.v()));
                ERCV(append_floatvalue<uint16_t>(s, c.g.v()));
                ERCV(append_floatvalue<uint16_t>(s, c.b.v()));
            } else if(shade.colorspace == CAPY_DEVICE_CS_GRAY) {
                if(!std::holds_alternative<DeviceGrayColor>(colorobj)) {
                    RETERR(ColorspaceMismatch);
                }
                const auto &c = std::get<DeviceGrayColor>(colorobj);
                ERCV(append_floatvalue<uint16_t>(s, c.v.v()));
            } else if(shade.colorspace == CAPY_DEVICE_CS_CMYK) {
                if(!std::holds_alternative<DeviceCMYKColor>(colorobj)) {
                    RETERR(ColorspaceMismatch);
                }
                const auto &c = std::get<DeviceCMYKColor>(colorobj);
                ERCV(append_floatvalue<uint16_t>(s, c.c.v()));
                ERCV(append_floatvalue<uint16_t>(s, c.m.v()));
                ERCV(append_floatvalue<uint16_t>(s, c.y.v()));
                ERCV(append_floatvalue<uint16_t>(s, c.k.v()));
            } else {
                fprintf(stderr, "Color space not yet supported.\n");
                std::abort();
            }
        }
    }
    return s;
}

int32_t num_channels_for(const CapyPDF_Image_Colorspace cs) {
    switch(cs) {
    case CAPY_IMAGE_CS_RGB:
        return 3;
    case CAPY_IMAGE_CS_GRAY:
        return 1;
    case CAPY_IMAGE_CS_CMYK:
        return 4;
    }
    std::abort();
}

void color2numbers(std::back_insert_iterator<std::string> &app, const Color &c) {
    if(auto *rgb = std::get_if<DeviceRGBColor>(&c)) {
        std::format_to(app, "{} {} {}", rgb->r.v(), rgb->g.v(), rgb->b.v());
    } else if(auto *gray = std::get_if<DeviceGrayColor>(&c)) {
        std::format_to(app, "{}", gray->v.v());
    } else if(auto *cmyk = std::get_if<DeviceCMYKColor>(&c)) {
        std::format_to(app, "{} {} {} {}", cmyk->c.v(), cmyk->m.v(), cmyk->y.v(), cmyk->k.v());
    } else {
        fprintf(stderr, "Colorspace not supported yet.\n");
        std::abort();
    }
}

} // namespace

const std::array<const char *, 4> rendering_intent_names{
    "RelativeColorimetric",
    "AbsoluteColorimetric",
    "Saturation",
    "Perceptual",
};

rvoe<PdfDocument> PdfDocument::construct(const DocumentProperties &d, PdfColorConverter cm) {
    rvoe<PdfDocument> newdoc{PdfDocument(d, std::move(cm))};
    ERCV(newdoc->init());
    return newdoc;
}

PdfDocument::PdfDocument(const DocumentProperties &d, PdfColorConverter cm)
    : docprops{d}, cm{std::move(cm)} {}

rvoe<NoReturnValue> PdfDocument::init() {
    // PDF uses 1-based indexing so add a dummy thing in this vector
    // to make PDF and vector indices are the same.
    document_objects.emplace_back(DummyIndexZero{});
    generate_info_object();
    switch(docprops.output_colorspace) {
    case CAPY_DEVICE_CS_RGB:
        if(!cm.get_rgb().empty()) {
            ERC(retval, add_icc_profile(cm.get_rgb(), 3));
            output_profile = retval;
        }
        break;
    case CAPY_DEVICE_CS_GRAY:
        if(!cm.get_gray().empty()) {
            ERC(retval, add_icc_profile(cm.get_gray(), 1));
            output_profile = retval;
        }
        break;
    case CAPY_DEVICE_CS_CMYK:
        if(cm.get_cmyk().empty()) {
            RETERR(OutputProfileMissing);
        }
        ERC(retval, add_icc_profile(cm.get_cmyk(), 4));
        output_profile = retval;
        break;
    }
    document_objects.push_back(DelayedPages{});
    pages_object = document_objects.size() - 1;
    if(!std::holds_alternative<std::monostate>(docprops.subtype)) {
        if(!output_profile) {
            RETERR(OutputProfileMissing);
        }
        if(docprops.intent_condition_identifier.empty()) {
            RETERR(MissingIntentIdentifier);
        }
        create_output_intent();
    }
    if(auto *aptr = std::get_if<CapyPDF_PDFA_Type>(&docprops.subtype)) {
        pdfa_md_object = add_pdfa_metadata_object(*aptr);
    }
    RETOK;
}

rvoe<NoReturnValue> PdfDocument::add_page(std::string resource_dict,
                                          std::string unclosed_object_dict,
                                          std::string command_stream,
                                          const PageProperties &custom_props,
                                          const std::unordered_set<CapyPDF_FormWidgetId> &fws,
                                          const std::unordered_set<CapyPDF_AnnotationId> &annots,
                                          const std::vector<CapyPDF_StructureItemId> &structs,
                                          const std::optional<Transition> &transition,
                                          const std::vector<SubPageNavigation> &subnav) {
    for(const auto &a : fws) {
        if(form_use.find(a) != form_use.cend()) {
            RETERR(AnnotationReuse);
        }
    }
    for(const auto &a : annots) {
        if(annotation_use.find(a) != annotation_use.cend()) {
            RETERR(AnnotationReuse);
        }
    }
    for(const auto &s : structs) {
        if(structure_use.find(s) != structure_use.cend()) {
            RETERR(StructureReuse);
        }
    }
    const auto resource_num = add_object(FullPDFObject{std::move(resource_dict), {}});
    int32_t commands_num{-1};
    if(docprops.compress_streams) {
        commands_num = add_object(
            DeflatePDFObject{std::move(unclosed_object_dict), std::move(command_stream)});
    } else {
        std::format_to(std::back_inserter(unclosed_object_dict),
                       "  /Length {}\n>>\n",
                       command_stream.length());
        commands_num =
            add_object(FullPDFObject{std::move(unclosed_object_dict), std::move(command_stream)});
    }
    DelayedPage p;
    p.page_num = (int32_t)pages.size();
    p.custom_props = custom_props;
    for(const auto &a : fws) {
        p.used_form_widgets.push_back(a);
    }
    for(const auto &a : annots) {
        p.used_annotations.push_back(CapyPDF_AnnotationId{a});
    }
    p.transition = transition;
    if(!subnav.empty()) {
        p.subnav_root = create_subnavigation(subnav);
    }
    if(!structs.empty()) {
        p.structparents = (int32_t)structure_parent_tree_items.size();
        structure_parent_tree_items.push_back(structs);
    }
    const auto page_object_num = add_object(std::move(p));
    for(const auto &fw : fws) {
        form_use[fw] = page_object_num;
    }
    for(const auto &a : annots) {
        annotation_use[a] = page_object_num;
    }
    int32_t mcid_num = 0;
    for(const auto &s : structs) {
        structure_use[s] = StructureUsage{(int32_t)pages.size(), mcid_num++};
    }
    pages.emplace_back(PageOffsets{resource_num, commands_num, page_object_num});
    RETOK;
}

rvoe<NoReturnValue>
PdfDocument::add_page_labeling(uint32_t start_page,
                               std::optional<CapyPDF_Page_Label_Number_Style> style,
                               std::optional<u8string> prefix,
                               std::optional<uint32_t> start_num) {
    if(!page_labels.empty() && page_labels.back().start_page < start_page) {
        RETERR(NonSequentialPageNumber);
    }
    page_labels.emplace_back(start_page, style, std::move(prefix), start_num);
    return NoReturnValue{};
}

// Form XObjects
void PdfDocument::add_form_xobject(std::string xobj_dict, std::string xobj_stream) {
    const auto xobj_num = add_object(FullPDFObject{std::move(xobj_dict), std::move(xobj_stream)});

    form_xobjects.emplace_back(FormXObjectInfo{xobj_num});
}

int32_t PdfDocument::create_subnavigation(const std::vector<SubPageNavigation> &subnav) {
    assert(!subnav.empty());
    const int32_t root_obj = document_objects.size();
    {
        std::string rootbuf{
            R"(<<
  /Type /NavNode
  /NA <<
    /S /SetOCGState
    /State [ /OFF
)"};
        auto rootapp = std::back_inserter(rootbuf);
        for(const auto &i : subnav) {
            std::format_to(rootapp, "      {} 0 R\n", ocg_object_number(i.id));
        }
        rootbuf += "    ]\n  >>\n";
        std::format_to(rootapp, "  /Next {} 0 R\n", root_obj + 1);
        rootbuf += R"(  /PA <<
    /S /SetOCGState
    /State [ /ON
)";
        for(const auto &i : subnav) {
            std::format_to(rootapp, "      {} 0 R\n", ocg_object_number(i.id));
        }
        rootbuf += "    ]\n  >>\n";
        std::format_to(rootapp, "  /Prev {} 0 R\n>>\n", root_obj + 1 + subnav.size());

        add_object(FullPDFObject{std::move(rootbuf), {}});
    }
    int32_t first_obj = document_objects.size();

    for(size_t i = 0; i < subnav.size(); ++i) {
        const auto &sn = subnav[i];
        std::string buf = R"(<<
  /Type /NavNode
)";
        auto app = std::back_inserter(buf);
        buf += "  /NA  <<\n";
        std::format_to(app,
                       R"(    /S /SetOCGState
    /State [ /ON {} 0 R ]
)",
                       ocg_object_number(sn.id));
        if(sn.tr) {
            buf += R"(    /Next <<
      /S /Trans
)";
            serialize_trans(app, *sn.tr, "      ");
            buf += "    >>\n";
        }

        buf += "  >>\n";
        std::format_to(app, "  /Next {} 0 R\n", first_obj + i + 1);
        if(i > 0) {
            std::format_to(app,
                           R"(  /PA <<
    /S /SetOCGState
    /State [ /OFF {} 0 R ]
  >>
)",
                           ocg_object_number(subnav[i - 1].id));
            std::format_to(app, "  /Prev {} 0 R\n", first_obj + i - 1);
        }
        buf += ">>\n";
        add_object(FullPDFObject{std::move(buf), {}});
    }
    add_object(FullPDFObject{std::format(R"(<<
  /Type /NavNode
  /PA <<
    /S /SetOCGState
    /State [ /OFF {} 0 R ]
  >>
  /Prev {} 0 R
>>
)",
                                         ocg_object_number(subnav.back().id),
                                         first_obj + subnav.size() - 1),
                             {}});
    return root_obj;
}

int32_t PdfDocument::add_object(ObjectType object) {
    auto object_num = (int32_t)document_objects.size();
    document_objects.push_back(std::move(object));
    return object_num;
}

rvoe<CapyPDF_SeparationId> PdfDocument::create_separation(const asciistring &name,
                                                          CapyPDF_Device_Colorspace cs,
                                                          const CapyPDF_FunctionId fid) {
    const auto &f4 = functions.at(fid.id);
    if(!std::holds_alternative<FunctionType4>(f4.original)) {
        RETERR(IncorrectFunctionType);
    }
    std::string buf;
    std::format_to(std::back_inserter(buf),
                   R"([
  /Separation
    /{}
    {}
    {} 0 R
]
)",
                   name.c_str(),
                   colorspace_names.at((int)cs),
                   f4.object_number);
    separation_objects.push_back(add_object(FullPDFObject{buf, {}}));
    return CapyPDF_SeparationId{(int32_t)separation_objects.size() - 1};
}

rvoe<CapyPDF_LabColorSpaceId> PdfDocument::add_lab_colorspace(const LabColorSpace &lab) {
    std::string buf = std::format(
        R"([ /Lab
  <<
    /WhitePoint [ {:f} {:f} {:f} ]
    /Range [ {:f} {:f} {:f} {:f} ]
  >>
]
)",
        lab.xw,
        lab.yw,
        lab.zw,
        lab.amin,
        lab.amax,
        lab.bmin,
        lab.bmax);

    add_object(FullPDFObject{std::move(buf), {}});
    return CapyPDF_LabColorSpaceId{(int32_t)document_objects.size() - 1};
}

rvoe<CapyPDF_IccColorSpaceId> PdfDocument::load_icc_file(const std::filesystem::path &fname) {
    ERC(contents, load_file(fname));
    const auto iccid = find_icc_profile(contents);
    if(iccid) {
        return *iccid;
    }
    ERC(num_channels, cm.get_num_channels(contents));
    return add_icc_profile(contents, num_channels);
}

void PdfDocument::pad_subset_fonts() {
    const uint32_t SPACE = ' ';
    const uint32_t max_count = 100;

    // A hidden requirement of the PDF text model is that _every_ subset font
    // must have the space character mapped at location 32.

    for(auto &sf : fonts) {
        auto face = sf.fontdata.face.get();
        if(!font_has_character(face, SPACE)) {
            // This font does not have the space character.
            // Thus nobody can use it and the subset does not need padding.
            continue;
        }
        auto &subsetter = sf.subsets;
        assert(subsetter.num_subsets() > 0);
        const auto subset_id = subsetter.num_subsets() - 1;
        // Try to add glyphs until the subset has 32 elements.
        bool padding_succeeded = false;
        uint32_t gindex;
        auto charcode = FT_Get_First_Char(face, &gindex);
        for(uint32_t i = 0; i < max_count; ++i) {
            if(subsetter.get_subset(subset_id).size() >= SPACE) {
                padding_succeeded = true;
            }
            auto rv = subsetter.get_glyph_subset(charcode, gindex);
            if(!rv) {
                // fprintf(stderr, "Bad!\n");
                // std::abort();
            }
            charcode = FT_Get_Next_Char(face, charcode, &gindex);
        }
        if(!padding_succeeded) {
            fprintf(stderr,
                    "Font subset padding failed for file %s.\n",
                    sf.fontdata.original_file.string().c_str());
            std::abort();
        }
        subsetter.unchecked_insert_glyph_to_last_subset(' ', {});
        assert(subsetter.get_subset(subset_id).size() > SPACE);
        const auto &space_glyph = subsetter.get_subset(subset_id).at(SPACE);
        assert(std::holds_alternative<RegularGlyph>(space_glyph));
        assert(std::get<RegularGlyph>(space_glyph).unicode_codepoint == SPACE);
    }
}

rvoe<int32_t> PdfDocument::create_name_dict() {
    assert(!embedded_files.empty());
    std::string buf = std::format(R"(<<
/EmbeddedFiles <<
  /Limits [ (embobj{:05}) (embojb{:05}) ]
  /Names [
)",
                                  0,
                                  embedded_files.size() - 1);
    auto app = std::back_inserter(buf);
    for(size_t i = 0; i < embedded_files.size(); ++i) {
        std::format_to(app, "    (embobj{:06}) {} 0 R\n", i, embedded_files[i].filespec_obj);
    }
    buf += "  ]\n>>\n";
    return add_object(FullPDFObject{std::move(buf), {}});
}

rvoe<int32_t> PdfDocument::create_structure_parent_tree() {
    std::string buf = "<< /Nums [\n";
    auto app = std::back_inserter(buf);
    for(size_t i = 0; i < structure_parent_tree_items.size(); ++i) {
        const auto &entry = structure_parent_tree_items[i];
        std::format_to(app, "  {} [\n", i);
        for(const auto &sitem : entry) {
            std::format_to(app, "    {} 0 R\n", structure_items.at(sitem.id).obj_id);
        }
        buf += "  ]\n";
    }
    buf += "] >>\n";
    return add_object(FullPDFObject{std::move(buf), {}});
}

rvoe<CapyPDF_RoleId> PdfDocument::add_rolemap_entry(std::string name,
                                                    CapyPDF_Structure_Type builtin_type) {
    if(name.empty() || name.front() == '/') {
        RETERR(SlashStart);
    }
    for(const auto &i : rolemap) {
        if(i.name == name) {
            RETERR(RoleAlreadyDefined);
        }
    }
    rolemap.emplace_back(RolemapEnty{std::move(name), builtin_type});
    return CapyPDF_RoleId{(int32_t)rolemap.size() - 1};
}

rvoe<NoReturnValue> PdfDocument::create_catalog() {
    std::string buf;
    auto app = std::back_inserter(buf);
    std::string outline;
    std::string name;
    std::string structure;

    if(!embedded_files.empty()) {
        ERC(names, create_name_dict());
        name = std::format("  /Names {} 0 R\n", names);
    }
    if(!outlines.items.empty()) {
        ERC(outlines, create_outlines());
        outline = std::format("  /Outlines {} 0 R\n", outlines);
    }
    if(!structure_items.empty()) {
        ERC(treeid, create_structure_parent_tree());
        structure_parent_tree_object = treeid;
        create_structure_root_dict();
        structure = std::format("  /StructTreeRoot {} 0 R\n", *structure_root_object);
    }
    std::format_to(app,
                   R"(<<
  /Type /Catalog
  /Pages {} 0 R
)",
                   pages_object);

    if(!page_labels.empty()) {
        buf += R"(  /PageLabels
    << /Nums [
)";
        for(const auto &page_label : page_labels) {
            std::format_to(app, R"(      {} <<)", page_label.start_page);
            if(page_label.style) {
                std::format_to(app, "        /S {}\n", page_label_types.at(*page_label.style));
            }
            if(page_label.prefix) {
                std::format_to(
                    app, "        /P {}\n", utf8_to_pdfutf16be(*page_label.prefix).c_str());
            }
            if(page_label.start_num) {
                std::format_to(app, "        /St {}\n", *page_label.start_num);
            }
            buf += R"(>>
)";
        }
        buf += R"(    ]
  >>
)";
    }
    if(!outline.empty()) {
        buf += outline;
    }
    if(!name.empty()) {
        buf += name;
    }
    if(!structure.empty()) {
        buf += structure;
    }
    if(!docprops.lang.empty()) {
        std::format_to(app, "  /Lang ({})\n", docprops.lang.c_str());
    }
    if(docprops.is_tagged) {
        std::format_to(app, "  /MarkInfo << /Marked true >>\n");
    }
    if(output_intent_object) {
        std::format_to(app, "  /OutputIntents [ {} 0 R ]\n", *output_intent_object);
    }
    if(!form_use.empty()) {
        buf += R"(  /AcroForm <<
    /Fields [
)";
        for(const auto &i : form_widgets) {
            std::format_to(app, "      {} 0 R\n", i);
        }
        buf += "      ]\n  >>\n";
        buf += "  /NeedAppearances true\n";
    }
    if(!ocg_items.empty()) {
        buf += R"(  /OCProperties <<
    /OCGs [
)";
        for(const auto &o : ocg_items) {
            std::format_to(app, "      {} 0 R\n", o);
        }
        buf += "    ]\n";
        buf += "    /D << /BaseState /ON >>\n";
        buf += "  >>\n";
    }
    if(pdfa_md_object) {
        std::format_to(app, "  /Metadata {} 0 R\n", *pdfa_md_object);
    }
    buf += ">>\n";
    add_object(FullPDFObject{buf, {}});
    RETOK;
}

void PdfDocument::create_output_intent() {
    std::string buf;
    assert(output_profile);
    assert(!std::holds_alternative<std::monostate>(docprops.subtype));
    const char *gts =
        std::holds_alternative<CapyPDF_PDFX_Type>(docprops.subtype) ? "/GTS_PDFX" : "/GTS_PDFA1";
    buf = std::format(R"(<<
  /Type /OutputIntent
  /S {}
  /OutputConditionIdentifier {}
  /DestOutputProfile {} 0 R
>>
)",
                      gts,
                      pdfstring_quote(docprops.intent_condition_identifier),
                      get(*output_profile).stream_num);
    output_intent_object = add_object(FullPDFObject{buf, {}});
}

template<typename T1, typename T2> static void append_value_or_null(T1 &app, const T2 &val) {
    if(val) {
        std::format_to(app, "{:f} ", val.value());
    } else {
        std::format_to(app, "{} ", "null");
    }
}

rvoe<int32_t> PdfDocument::create_outlines() {
    int32_t first_obj_num = (int32_t)document_objects.size();
    int32_t catalog_obj_num = first_obj_num + (int32_t)outlines.items.size();
    for(int32_t cur_id = 0; cur_id < (int32_t)outlines.items.size(); ++cur_id) {
        const auto &cur_obj = outlines.items[cur_id];
        auto titlestr = utf8_to_pdfutf16be(cur_obj.title);
        auto parent_id = outlines.parent.at(cur_id);
        const auto &siblings = outlines.children.at(parent_id);
        std::string oitem = std::format(R"(<<
  /Title {}
)",
                                        titlestr);
        auto app = std::back_inserter(oitem);
        if(cur_obj.dest) {
            const auto &dest = cur_obj.dest.value();
            const auto physical_page = dest.page;
            if(physical_page < 0 || physical_page >= (int32_t)pages.size()) {
                RETERR(InvalidPageNumber);
            }
            const auto page_object_number = pages.at(physical_page).page_obj_num;
            std::format_to(app, "  /Dest [ {} 0 R ", page_object_number);
            if(auto xyz = std::get_if<DestinationXYZ>(&dest.loc)) {
                std::format_to(app, "/XYZ ");
                append_value_or_null(app, xyz->x);
                append_value_or_null(app, xyz->y);
                append_value_or_null(app, xyz->z);
            } else if(std::holds_alternative<DestinationFit>(dest.loc)) {
                std::format_to(app, "/Fit ");
            } else if(auto r = std::get_if<DestinationFitR>(&dest.loc)) {
                std::format_to(app, "/FitR {} {} {} {} ", r->left, r->bottom, r->right, r->top);
            } else {
                std::abort();
            }
            oitem += "]\n";
        }
        if(siblings.size() > 1) {
            auto loc = std::find(siblings.begin(), siblings.end(), cur_id);
            assert(loc != siblings.end());
            if(loc != siblings.begin()) {
                auto prevloc = loc;
                --prevloc;
                std::format_to(app, "  /Prev {} 0 R\n", first_obj_num + *prevloc);
            }
            auto nextloc = loc;
            nextloc++;
            if(nextloc != siblings.end()) {
                std::format_to(app, "  /Next {} 0 R\n", first_obj_num + *nextloc);
            }
        }
        auto childs = outlines.children.find(cur_id);
        if(childs != outlines.children.end()) {
            const auto &children = childs->second;
            std::format_to(app, "  /First {} 0 R\n", first_obj_num + children.front());
            std::format_to(app, "  /Last {} 0 R\n", first_obj_num + children.back());
            std::format_to(app, "  /Count {}\n", -(int32_t)children.size());
        }
        /*
        if(node_counts[cur_id] > 0) {
            std::format_to(app, "  /Count {}\n", -node_counts[cur_id]);
        }*/
        std::format_to(app,
                       "  /Parent {} 0 R\n",
                       parent_id >= 0 ? first_obj_num + parent_id : catalog_obj_num);
        if(cur_obj.F != 0) {
            std::format_to(app, "  /F {}\n", cur_obj.F);
        }
        if(cur_obj.C) {
            std::format_to(app,
                           "  /C [ {:f} {:f} {:f} ]\n",
                           cur_obj.C.value().r.v(),
                           cur_obj.C.value().g.v(),
                           cur_obj.C.value().b.v());
        }
        oitem += ">>\n";
        add_object(FullPDFObject{std::move(oitem), {}});
    }
    const auto &top_level = outlines.children.at(-1);
    std::string buf = std::format(R"(<<
  /Type /Outlines
  /First {} 0 R
  /Last {} 0 R
  /Count {}
>>
)",
                                  first_obj_num + top_level.front(),
                                  first_obj_num + top_level.back(),
                                  outlines.children.at(-1).size());

    assert(catalog_obj_num == (int32_t)document_objects.size());
    // FIXME: add output intents here. PDF spec 14.11.5
    return add_object(FullPDFObject{std::move(buf), {}});
}

void PdfDocument::create_structure_root_dict() {
    std::string buf;
    auto app = std::back_inserter(buf);
    std::optional<CapyPDF_StructureItemId> rootobj;

    if(!structure_parent_tree_object) {
        fprintf(stderr, "Internal error!\n");
        std::abort();
    }
    for(int32_t i = 0; i < (int32_t)structure_items.size(); ++i) {
        if(!structure_items[i].parent) {
            rootobj = CapyPDF_StructureItemId{i};
            break;
        }
        // FIXME, check that there is only one.
    }
    assert(rootobj);
    // /ParentTree
    std::format_to(app,
                   R"(<<
  /Type /StructTreeRoot
  /K [ {} 0 R ]
  /ParentTree {} 0 R
  /ParentTreeNextKey {}
)",
                   structure_items[rootobj->id].obj_id,
                   structure_parent_tree_object.value(),
                   structure_parent_tree_items.size());
    if(!rolemap.empty()) {
        buf += "  /RoleMap <<\n";
        for(const auto &i : rolemap) {
            std::format_to(app,
                           "    {} /{}\n",
                           bytes2pdfstringliteral(i.name),
                           structure_type_names.at(i.builtin));
        }
        buf += "  >>\n";
    }
    buf += ">>\n";
    structure_root_object = add_object(FullPDFObject{buf, {}});
}

void PdfDocument::pad_subset_until_space(std::vector<TTGlyphs> &subset_glyphs) {
    const size_t max_count = 100;
    const uint32_t SPACE = ' ';
    if(subset_glyphs.size() > SPACE) {
        return;
    }
    bool padding_succeeded = false;
    for(uint32_t i = 0; i < max_count; ++i) {
        if(subset_glyphs.size() == SPACE) {
            padding_succeeded = true;
            break;
        }
        // Yes, this is O(n^2), but n is at most 31.
        const auto cur_glyph_codepoint = '!' + i;
        auto glfind = [cur_glyph_codepoint](const TTGlyphs &g) {
            return std::holds_alternative<RegularGlyph>(g) &&
                   std::get<RegularGlyph>(g).unicode_codepoint == cur_glyph_codepoint;
        };
        if(std::find_if(subset_glyphs.cbegin(), subset_glyphs.cend(), glfind) !=
           subset_glyphs.cend()) {
            continue;
        }
        subset_glyphs.emplace_back(RegularGlyph{cur_glyph_codepoint, (uint32_t)-1});
    }
    if(!padding_succeeded) {
        fprintf(stderr, "Font subset space padding failed.n");
        std::abort();
    }
    subset_glyphs.emplace_back(RegularGlyph{SPACE, (uint32_t)-1});
    assert(subset_glyphs.size() == SPACE + 1);
}

int32_t PdfDocument::add_pdfa_metadata_object(CapyPDF_PDFA_Type atype) {
    auto stream = std::format(
        pdfa_rdf_template, (char *)rdf_magic, pdfa_part.at(atype), pdfa_conformance.at(atype));
    auto dict = std::format(R"(<<
  /Type /Metadata
  /Subtype /XML
  /Length {}
>>
)",
                            stream.length());
    return add_object(FullPDFObject{std::move(dict), std::move(stream)});
}

std::optional<CapyPDF_IccColorSpaceId> PdfDocument::find_icc_profile(std::string_view contents) {
    for(size_t i = 0; i < icc_profiles.size(); ++i) {
        const auto &stream_obj = document_objects.at(icc_profiles.at(i).stream_num);
        assert(std::holds_alternative<DeflatePDFObject>(stream_obj));
        const auto &stream_data = std::get<DeflatePDFObject>(stream_obj);
        if(stream_data.stream == contents) {
            return CapyPDF_IccColorSpaceId{(int32_t)i};
        }
    }
    return {};
}

rvoe<CapyPDF_IccColorSpaceId> PdfDocument::add_icc_profile(std::string_view contents,
                                                           int32_t num_channels) {
    auto existing = find_icc_profile(contents);
    assert(!existing);
    if(contents.empty()) {
        return CapyPDF_IccColorSpaceId{-1};
    }
    std::string buf;
    std::format_to(std::back_inserter(buf),
                   R"(<<
  /N {}
)",
                   num_channels);
    auto stream_obj_id = add_object(DeflatePDFObject{std::move(buf), std::string{contents}});
    auto obj_id =
        add_object(FullPDFObject{std::format("[ /ICCBased {} 0 R ]\n", stream_obj_id), {}});
    icc_profiles.emplace_back(IccInfo{stream_obj_id, obj_id, num_channels});
    return CapyPDF_IccColorSpaceId{(int32_t)icc_profiles.size() - 1};
}

rvoe<NoReturnValue> PdfDocument::generate_info_object() {
    FullPDFObject obj_data;
    obj_data.dictionary = "<<\n";
    if(!docprops.title.empty()) {
        obj_data.dictionary += "  /Title ";
        auto titlestr = utf8_to_pdfutf16be(docprops.title);
        obj_data.dictionary += titlestr;
        obj_data.dictionary += "\n";
    }
    if(!docprops.author.empty()) {
        obj_data.dictionary += "  /Author ";
        auto authorstr = utf8_to_pdfutf16be(docprops.author);
        obj_data.dictionary += authorstr;
        obj_data.dictionary += "\n";
    }
    if(!docprops.creator.empty()) {
        obj_data.dictionary += "  /Creator ";
        auto creatorstr = utf8_to_pdfutf16be(docprops.creator);
        obj_data.dictionary += creatorstr;
        obj_data.dictionary += "\n";
    }
    const auto current_date = current_date_string();
    obj_data.dictionary += "  /Producer (CapyPDF " CAPYPDF_VERSION_STR ")\n";
    obj_data.dictionary += "  /CreationDate ";
    obj_data.dictionary += current_date;
    obj_data.dictionary += '\n';
    obj_data.dictionary += "  /ModDate ";
    obj_data.dictionary += current_date;
    obj_data.dictionary += '\n';
    obj_data.dictionary += "  /Trapped /False\n";
    if(auto xptr = std::get_if<CapyPDF_PDFX_Type>(&docprops.subtype)) {
        obj_data.dictionary += "  /GTS_PDFXVersion (";
        obj_data.dictionary += pdfx_names.at(*xptr);
        obj_data.dictionary += ")\n";
    }
    obj_data.dictionary += ">>\n";
    add_object(std::move(obj_data));
    RETOK;
}

CapyPDF_FontId PdfDocument::get_builtin_font_id(CapyPDF_Builtin_Fonts font) {
    auto it = builtin_fonts.find(font);
    if(it != builtin_fonts.end()) {
        return it->second;
    }
    std::string font_dict;
    std::format_to(std::back_inserter(font_dict),
                   R"(<<
  /Type /Font
  /Subtype /Type1
  /BaseFont /{}
>>
)",
                   font_names[font]);
    font_objects.push_back(FontInfo{-1, -1, add_object(FullPDFObject{font_dict, {}}), size_t(-1)});
    auto fontid = CapyPDF_FontId{(int32_t)font_objects.size() - 1};
    builtin_fonts[font] = fontid;
    return fontid;
}

uint32_t PdfDocument::glyph_for_codepoint(FT_Face face, uint32_t ucs4) {
    assert(face);
    return FT_Get_Char_Index(face, ucs4);
}

bool PdfDocument::font_has_character(CapyPDF_FontId fid, uint32_t codepoint) {
    return font_has_character(fonts.at(fid.id).fontdata.face.get(), codepoint);
}

bool PdfDocument::font_has_character(FT_Face face, uint32_t codepoint) {
    return FT_Get_Char_Index(face, codepoint) != 0;
}

rvoe<SubsetGlyph> PdfDocument::get_subset_glyph(CapyPDF_FontId fid,
                                                uint32_t codepoint,
                                                const std::optional<uint32_t> glyph_id) {
    if(!glyph_id && !font_has_character(fid, codepoint)) {
        RETERR(MissingGlyph);
    }
    ERC(blub, fonts.at(fid.id).subsets.get_glyph_subset(codepoint, glyph_id));
    SubsetGlyph fss;
    fss.ss.fid = fid;
    fss.ss.subset_id = blub.subset;
    fss.glyph_id = blub.offset;
    return fss;
}

rvoe<SubsetGlyph>
PdfDocument::get_subset_glyph(CapyPDF_FontId fid, const u8string &text, uint32_t glyph_id) {
    ERC(blub, fonts.at(fid.id).subsets.get_glyph_subset(text, glyph_id));
    SubsetGlyph fss;
    fss.ss.fid = fid;
    fss.ss.subset_id = blub.subset;
    fss.glyph_id = blub.offset;
    return fss;
}

rvoe<CapyPDF_ImageId> PdfDocument::add_mask_image(RawPixelImage image,
                                                  const ImagePDFProperties &params) {
    if(image.md.cs != CAPY_IMAGE_CS_GRAY || image.md.pixel_depth != 1) {
        RETERR(UnsupportedFormat);
    }
    if(!params.as_mask) {
        std::abort();
    }
    return add_image_object(image.md.w,
                            image.md.h,
                            image.md.pixel_depth,
                            image.md.cs,
                            std::optional<int32_t>{},
                            params,
                            image.pixels,
                            image.md.compression);
}

rvoe<CapyPDF_ImageId> PdfDocument::add_image(RawPixelImage image,
                                             const ImagePDFProperties &params) {
    if(image.md.w < 1 || image.md.h < 1) {
        RETERR(InvalidImageSize);
    }
    if(image.pixels.empty()) {
        RETERR(MissingPixels);
    }
    ERCV(validate_format(image));
    std::optional<int32_t> smask_id;
    if(params.as_mask && !image.alpha.empty()) {
        RETERR(MaskAndAlpha);
    }
    if(!image.alpha.empty()) {
        ERC(imobj,
            add_image_object(image.md.w,
                             image.md.h,
                             image.md.alpha_depth,
                             CAPY_IMAGE_CS_GRAY,
                             {},
                             params,
                             image.alpha,
                             image.md.compression));
        smask_id = get(imobj).obj;
    }
    if(!image.icc_profile.empty()) {
        ERC(icc_id, add_icc_profile(image.icc_profile, num_channels_for(image.md.cs)));
        return add_image_object(image.md.w,
                                image.md.h,
                                image.md.pixel_depth,
                                icc_id,
                                smask_id,
                                params,
                                image.pixels,
                                image.md.compression);
    } else {
        return add_image_object(image.md.w,
                                image.md.h,
                                image.md.pixel_depth,
                                image.md.cs,
                                smask_id,
                                params,
                                image.pixels,
                                image.md.compression);
    }
}

rvoe<CapyPDF_ImageId> PdfDocument::add_image_object(uint32_t w,
                                                    uint32_t h,
                                                    uint32_t bits_per_component,
                                                    ImageColorspaceType colorspace,
                                                    std::optional<int32_t> smask_id,
                                                    const ImagePDFProperties &params,
                                                    std::string_view original_bytes,
                                                    CapyPDF_Compression compression) {
    std::string buf;
    std::string compression_buffer;
    std::string_view compressed_bytes;
    switch(compression) {
    case CAPY_COMPRESSION_NONE: {
        ERC(trial_compressed, flate_compress(original_bytes));
        compression_buffer = std::move(trial_compressed);
        compressed_bytes = compression_buffer;
        break;
    }
    case CAPY_COMPRESSION_DEFLATE:
        compressed_bytes = original_bytes;
        break;
    }

    auto app = std::back_inserter(buf);
    std::format_to(app,
                   R"(<<
  /Type /XObject
  /Subtype /Image
  /Width {}
  /Height {}
  /BitsPerComponent {}
  /Length {}
  /Filter /FlateDecode
)",
                   w,
                   h,
                   bits_per_component,
                   compressed_bytes.size());

    // Auto means don't specify the interpolation
    if(params.interp == CAPY_INTERPOLATION_PIXELATED) {
        buf += "  /Interpolate false\n";
    } else if(params.interp == CAPY_INTERPOLATION_SMOOTH) {
        buf += "  /Interpolate true\n";
    }

    // An image may only have ImageMask or ColorSpace key, not both.
    if(params.as_mask) {
        buf += "  /ImageMask true\n";
    } else {
        if(auto cs = std::get_if<CapyPDF_Image_Colorspace>(&colorspace)) {
            std::format_to(app, "  /ColorSpace {}\n", colorspace_names.at(*cs));
        } else if(auto icc = std::get_if<CapyPDF_IccColorSpaceId>(&colorspace)) {
            const auto icc_obj = get(*icc).object_num;
            std::format_to(app, "  /ColorSpace {} 0 R\n", icc_obj);
        } else {
            fprintf(stderr, "Unknown colorspace.");
            std::abort();
        }
    }
    if(smask_id) {
        std::format_to(app, "  /SMask {} 0 R\n", smask_id.value());
    }
    buf += ">>\n";
    int32_t im_id;
    if(compression == CAPY_COMPRESSION_NONE) {
        im_id = add_object(FullPDFObject{std::move(buf), std::move(compression_buffer)});
    } else {
        // FIXME. Makes a copy. Fix to grab original data instead.
        im_id = add_object(FullPDFObject{std::move(buf), std::string{original_bytes}});
    }
    image_info.emplace_back(ImageInfo{{w, h}, im_id});
    return CapyPDF_ImageId{(int32_t)image_info.size() - 1};
}

rvoe<CapyPDF_ImageId> PdfDocument::embed_jpg(jpg_image jpg, const ImagePDFProperties &props) {
    std::string buf;
    std::format_to(std::back_inserter(buf),
                   R"(<<
  /Type /XObject
  /Subtype /Image
  /ColorSpace {}
  /Width {}
  /Height {}
  /BitsPerComponent {}
  /Length {}
  /Filter /DCTDecode
>>
)",
                   colorspace_names.at(jpg.cs),
                   jpg.w,
                   jpg.h,
                   jpg.depth,
                   jpg.file_contents.length());

    // Auto means don't specify the interpolation
    if(props.interp == CAPY_INTERPOLATION_PIXELATED) {
        buf += "  /Interpolate false\n";
    } else if(props.interp == CAPY_INTERPOLATION_SMOOTH) {
        buf += "  /Interpolate true\n";
    }
    // FIXME, add other properties too?

    auto im_id = add_object(FullPDFObject{std::move(buf), std::move(jpg.file_contents)});
    image_info.emplace_back(ImageInfo{{jpg.w, jpg.h}, im_id});
    return CapyPDF_ImageId{(int32_t)image_info.size() - 1};
}

rvoe<CapyPDF_GraphicsStateId> PdfDocument::add_graphics_state(const GraphicsState &state) {
    const int32_t id = (int32_t)document_objects.size();
    std::string buf{
        R"(<<
  /Type /ExtGState
)"};
    auto resource_appender = std::back_inserter(buf);
    if(state.LW) {
        std::format_to(resource_appender, "  /LW {:f}\n", *state.LW);
    }
    if(state.LC) {
        std::format_to(resource_appender, "  /LC {}\n", (int)*state.LC);
    }
    if(state.LJ) {
        std::format_to(resource_appender, "  /LJ {}\n", (int)*state.LJ);
    }
    if(state.ML) {
        std::format_to(resource_appender, "  /ML {:f}\n", *state.ML);
    }
    if(state.RI) {
        std::format_to(
            resource_appender, "  /RenderingIntent /{}\n", rendering_intent_names.at(*state.RI));
    }
    if(state.OP) {
        std::format_to(resource_appender, "  /OP {}\n", *state.OP ? "true" : "false");
    }
    if(state.op) {
        std::format_to(resource_appender, "  /op {}\n", *state.op ? "true" : "false");
    }
    if(state.OPM) {
        std::format_to(resource_appender, "  /OPM {}\n", *state.OPM);
    }
    if(state.FL) {
        std::format_to(resource_appender, "  /FL {:f}\n", *state.FL);
    }
    if(state.SM) {
        std::format_to(resource_appender, "  /SM {:f}\n", *state.SM);
    }
    if(state.BM) {
        std::format_to(resource_appender, "  /BM /{}\n", blend_mode_names.at(*state.BM));
    }
    if(state.SMask) {
        auto objnum = soft_masks.at(state.SMask->id);
        std::format_to(resource_appender, "  /SMask {} 0 R\n", objnum);
    }
    if(state.CA) {
        std::format_to(resource_appender, "  /CA {:f}\n", state.CA->v());
    }
    if(state.ca) {
        std::format_to(resource_appender, "  /ca {:f}\n", state.ca->v());
    }
    if(state.AIS) {
        std::format_to(resource_appender, "  /AIS {}\n", *state.AIS ? "true" : "false");
    }
    if(state.TK) {
        std::format_to(resource_appender, "  /TK {}\n", *state.TK ? "true" : "false");
    }
    buf += ">>\n";
    add_object(FullPDFObject{std::move(buf), {}});
    return CapyPDF_GraphicsStateId{id};
}

rvoe<int32_t> PdfDocument::serialize_function(const FunctionType2 &func) {
    const int functiontype = 2;
    if(func.C0.index() != func.C1.index()) {
        RETERR(ColorspaceMismatch);
    }
    std::string buf = std::format(
        R"(<<
  /FunctionType {}
  /N {}
)",
        functiontype,
        func.n);
    auto resource_appender = std::back_inserter(buf);

    buf += "  /Domain [ ";
    for(const auto d : func.domain) {
        std::format_to(resource_appender, "{} ", d);
    }
    buf += "]\n";
    buf += "  /C0 [ ";
    color2numbers(resource_appender, func.C0);
    buf += "]\n";
    buf += "  /C1 [ ";
    color2numbers(resource_appender, func.C1);
    buf += "]\n";
    buf += ">>\n";

    return add_object(FullPDFObject{std::move(buf), {}});
}

rvoe<int32_t> PdfDocument::serialize_function(const FunctionType3 &func) {
    const int functiontype = 3;
    if(!func.functions.size()) {
        RETERR(EmptyFunctionList);
    }
    std::string buf = std::format(
        R"(<<
  /FunctionType {}
)",
        functiontype);
    auto resource_appender = std::back_inserter(buf);

    buf += "  /Domain [ ";
    for(const auto d : func.domain) {
        std::format_to(resource_appender, "{} ", d);
    }
    buf += "]\n";

    buf += "  /Functions [ ";
    for(const auto f : func.functions) {
        std::format_to(resource_appender, "{} 0 R ", functions.at(f.id).object_number);
    }
    buf += "]\n";

    buf += "  /Bounds [ ";
    for(const auto b : func.bounds) {
        std::format_to(resource_appender, "{} ", b);
    }
    buf += "]\n";

    buf += "  /Encode [ ";
    for(const auto e : func.encode) {
        std::format_to(resource_appender, "{} ", e);
    }
    buf += "]\n";

    buf += ">>\n";

    return add_object(FullPDFObject{std::move(buf), {}});
}

rvoe<int32_t> PdfDocument::serialize_function(const FunctionType4 &func) {
    std::string buf{"<<\n  /FunctionType 4\n  /Domain ["};
    auto app = std::back_inserter(buf);
    for(const auto d : func.domain) {
        std::format_to(app, " {:f}", d);
    }
    buf += " ]\n  /Range [";
    for(const auto r : func.range) {
        std::format_to(app, " {:f}", r);
    }
    buf += " ]\n";
    return add_object(DeflatePDFObject{std::move(buf), func.code});
}

rvoe<CapyPDF_FunctionId> PdfDocument::add_function(PdfFunction f) {
    int32_t object_number;
    if(auto *f2 = std::get_if<FunctionType2>(&f)) {
        ERC(rc, serialize_function(*f2));
        object_number = rc;
    } else if(auto *f3 = std::get_if<FunctionType3>(&f)) {
        ERC(rc, serialize_function(*f3));
        object_number = rc;
    } else if(auto *f4 = std::get_if<FunctionType4>(&f)) {
        ERC(rc, serialize_function(*f4));
        object_number = rc;
    } else {
        fprintf(stderr, "Function type not implemented yet.\n");
        std::abort();
    }
    functions.emplace_back(FunctionInfo{std::move(f), object_number});
    return CapyPDF_FunctionId{(int32_t)functions.size() - 1};
}

rvoe<FullPDFObject> PdfDocument::serialize_shading(const PdfShading &shade) {
    if(auto *sh2 = std::get_if<ShadingType2>(&shade)) {
        return serialize_shading(*sh2);
    } else if(auto *sh3 = std::get_if<ShadingType3>(&shade)) {
        return serialize_shading(*sh3);
    } else if(auto *sh4 = std::get_if<ShadingType4>(&shade)) {
        return serialize_shading(*sh4);
    } else if(auto *sh6 = std::get_if<ShadingType6>(&shade)) {
        return serialize_shading(*sh6);
    }
    fprintf(stderr, "Shading type not supported yet.\n");
    std::abort();
}

rvoe<FullPDFObject> PdfDocument::serialize_shading(const ShadingType2 &shade) {
    const int shadingtype = 2;
    auto buf = std::format(
        R"(<<
  /ShadingType {}
  /ColorSpace {}
  /Coords [ {:f} {:f} {:f} {:f} ]
  /Function {} 0 R
)",
        shadingtype,
        colorspace_names.at((int)shade.colorspace),
        shade.x0,
        shade.y0,
        shade.x1,
        shade.y1,
        functions.at(shade.function.id).object_number);
    if(shade.extend) {
        auto app = std::back_inserter(buf);
        std::format_to(app,
                       "  /Extend [ {} {} ]\n",
                       shade.extend->starting ? "true" : "false",
                       shade.extend->ending ? "true" : "false");
    }
    if(shade.domain) {
        auto app = std::back_inserter(buf);
        std::format_to(
            app, "  /Domain [ {:f} {:f} ]\n", shade.domain->starting, shade.domain->ending);
    }
    buf += ">>\n";
    return FullPDFObject{std::move(buf), {}};
}

rvoe<FullPDFObject> PdfDocument::serialize_shading(const ShadingType3 &shade) {
    const int shadingtype = 3;
    auto buf = std::format(
        R"(<<
  /ShadingType {}
  /ColorSpace {}
  /Coords [ {:f} {:f} {:f} {:f} {:f} {:f} ]
  /Function {} 0 R
)",
        shadingtype,
        colorspace_names.at((int)shade.colorspace),
        shade.x0,
        shade.y0,
        shade.r0,
        shade.x1,
        shade.y1,
        shade.r1,
        functions.at(shade.function.id).object_number);
    if(shade.extend) {
        auto app = std::back_inserter(buf);
        std::format_to(app,
                       "  /Extend [ {} {} ]\n",
                       shade.extend->starting ? "true" : "false",
                       shade.extend->ending ? "true" : "false");
    }
    if(shade.domain) {
        auto app = std::back_inserter(buf);
        std::format_to(
            app, "  /Domain [ {:f} {:f} ]\n", shade.domain->starting, shade.domain->ending);
    }
    buf += ">>\n";

    return FullPDFObject{std::move(buf), {}};
}

rvoe<FullPDFObject> PdfDocument::serialize_shading(const ShadingType4 &shade) {
    const int shadingtype = 4;
    ERC(serialized, serialize_shade4(shade));
    std::string buf = std::format(
        R"(<<
  /ShadingType {}
  /ColorSpace {}
  /BitsPerCoordinate 32
  /BitsPerComponent 16
  /BitsPerFlag 8
  /Length {}
  /Decode [
    {:f} {:f}
    {:f} {:f}
)",
        shadingtype,
        colorspace_names.at((int)shade.colorspace),
        serialized.length(),
        shade.minx,
        shade.maxx,
        shade.miny,
        shade.maxy);
    if(shade.colorspace == CAPY_DEVICE_CS_RGB) {
        buf += R"(    0 1
    0 1
    0 1
)";
    } else if(shade.colorspace == CAPY_DEVICE_CS_GRAY) {
        buf += "  0 1\n";
    } else if(shade.colorspace == CAPY_DEVICE_CS_CMYK) {
        buf += R"(    0 1
    0 1
    0 1
    0 1
)";
    } else {
        fprintf(stderr, "Color space not supported yet.\n");
        std::abort();
    }
    buf += "  ]\n>>\n";
    return FullPDFObject{std::move(buf), std::move(serialized)};
}

rvoe<FullPDFObject> PdfDocument::serialize_shading(const ShadingType6 &shade) {
    const int shadingtype = 6;
    ERC(serialized, serialize_shade6(shade));
    std::string buf = std::format(
        R"(<<
  /ShadingType {}
  /ColorSpace {}
  /BitsPerCoordinate 32
  /BitsPerComponent 16
  /BitsPerFlag 8
  /Length {}
  /Decode [
    {:f} {:f}
    {:f} {:f}
)",
        shadingtype,
        colorspace_names.at((int)shade.colorspace),
        serialized.length(),
        shade.minx,
        shade.maxx,
        shade.miny,
        shade.maxy);
    if(shade.colorspace == CAPY_DEVICE_CS_RGB) {
        buf += R"(    0 1
    0 1
    0 1
)";
    } else if(shade.colorspace == CAPY_DEVICE_CS_GRAY) {
        buf += "  0 1\n";
    } else if(shade.colorspace == CAPY_DEVICE_CS_CMYK) {
        buf += R"(    0 1
    0 1
    0 1
    0 1
)";
    } else {
        fprintf(stderr, "Color space not supported yet.\n");
        std::abort();
    }
    buf += "  ]\n>>\n";
    return FullPDFObject{std::move(buf), std::move(serialized)};
}

rvoe<CapyPDF_ShadingId> PdfDocument::add_shading(PdfShading sh) {
    ERC(pdf_obj, serialize_shading(sh));
    shadings.emplace_back(ShadingInfo{std::move(sh), add_object(pdf_obj)});
    return CapyPDF_ShadingId{(int32_t)shadings.size() - 1};
}

rvoe<CapyPDF_PatternId> PdfDocument::add_shading_pattern(const ShadingPattern &shp) {
    std::string buf = R"(<<
  /Type /Pattern
  /PatternType 2
)";
    auto app = std::back_inserter(buf);
    std::format_to(app, "  /Shading {} 0 R\n", shadings.at(shp.sid.id).object_number);
    if(shp.m) {
        std::format_to(app,
                       "  /Matrix [ {:f} {:f} {:f} {:f} {:f} {:f} ]\n",
                       shp.m->a,
                       shp.m->b,
                       shp.m->c,
                       shp.m->d,
                       shp.m->e,
                       shp.m->f);
    }
    buf += ">>\n";
    return CapyPDF_PatternId{add_object(FullPDFObject{std::move(buf), {}})};
}

rvoe<CapyPDF_PatternId> PdfDocument::add_tiling_pattern(PdfDrawContext &ctx) {
    if(&ctx.get_doc() != this) {
        RETERR(IncorrectDocumentForObject);
    }
    if(ctx.draw_context_type() != CAPY_DC_COLOR_TILING) {
        RETERR(InvalidDrawContextType);
    }
    if(ctx.marked_content_depth() != 0) {
        RETERR(UnclosedMarkedContent);
    }
    auto sc_var = ctx.serialize();
    auto &d = std::get<SerializedXObject>(sc_var);
    auto objid = add_object(FullPDFObject{std::move(d.dict), std::move(d.command_stream)});
    return CapyPDF_PatternId{objid};
}

rvoe<CapyPDF_OutlineId> PdfDocument::add_outline(const Outline &o) {
    if(o.title.empty()) {
        RETERR(EmptyTitle);
    }
    const auto cur_id = (int32_t)outlines.items.size();
    const auto par_id = o.parent.value_or(CapyPDF_OutlineId{-1}).id;
    outlines.parent[cur_id] = par_id;
    outlines.children[par_id].push_back(cur_id);
    outlines.items.emplace_back(o);
    return CapyPDF_OutlineId{cur_id};
}

rvoe<CapyPDF_FormWidgetId> PdfDocument::create_form_checkbox(PdfBox loc,
                                                             CapyPDF_FormXObjectId onstate,
                                                             CapyPDF_FormXObjectId offstate,
                                                             std::string_view partial_name) {
    CHECK_INDEXNESS_V(onstate.id, form_xobjects);
    CHECK_INDEXNESS_V(offstate.id, form_xobjects);
    DelayedCheckboxWidgetAnnotation formobj{
        {(int32_t)form_widgets.size()}, loc, onstate, offstate, std::string{partial_name}};
    auto obj_id = add_object(std::move(formobj));
    form_widgets.push_back(obj_id);
    return CapyPDF_FormWidgetId{(int32_t)form_widgets.size() - 1};
}

rvoe<CapyPDF_EmbeddedFileId> PdfDocument::embed_file(const std::filesystem::path &fname) {
    ERC(contents, load_file(fname));
    std::string dict = std::format(R"(<<
  /Type /EmbeddedFile
  /Length {}
>>)",
                                   contents.size());
    auto fileobj_id = add_object(FullPDFObject{std::move(dict), std::move(contents)});
    dict = std::format(R"(<<
  /Type /Filespec
  /F {}
  /EF << /F {} 0 R >>
>>
)",
                       pdfstring_quote(fname.filename().string()),
                       fileobj_id);
    auto filespec_id = add_object(FullPDFObject{std::move(dict), {}});
    embedded_files.emplace_back(EmbeddedFileObject{filespec_id, fileobj_id});
    return CapyPDF_EmbeddedFileId{(int32_t)embedded_files.size() - 1};
}

rvoe<CapyPDF_AnnotationId> PdfDocument::add_annotation(const Annotation &a) {
    if(!a.rect) {
        RETERR(AnnotationMissingRect);
    }
    auto annot_id = (int32_t)annotations.size();
    auto obj_id = add_object(DelayedAnnotation{{annot_id}, a});
    annotations.push_back((int32_t)obj_id);
    return CapyPDF_AnnotationId{annot_id};
}

rvoe<CapyPDF_StructureItemId>
PdfDocument::add_structure_item(const CapyPDF_Structure_Type stype,
                                std::optional<CapyPDF_StructureItemId> parent,
                                std::optional<StructItemExtraData> extra) {
    if(parent) {
        CHECK_INDEXNESS_V(parent->id, structure_items);
    }
    auto stritem_id = (int32_t)structure_items.size();
    auto obj_id = add_object(DelayedStructItem{stritem_id});
    structure_items.push_back(
        StructItem{obj_id, stype, parent, extra.value_or(StructItemExtraData())});
    return CapyPDF_StructureItemId{(int32_t)structure_items.size() - 1};
}

rvoe<CapyPDF_StructureItemId>
PdfDocument::add_structure_item(const CapyPDF_RoleId role,
                                std::optional<CapyPDF_StructureItemId> parent,
                                std::optional<StructItemExtraData> extra) {
    if(parent) {
        CHECK_INDEXNESS_V(parent->id, structure_items);
    }
    auto stritem_id = (int32_t)structure_items.size();
    auto obj_id = add_object(DelayedStructItem{stritem_id});
    structure_items.push_back(
        StructItem{obj_id, role, parent, extra.value_or(StructItemExtraData())});
    return CapyPDF_StructureItemId{(int32_t)structure_items.size() - 1};
}

rvoe<CapyPDF_OptionalContentGroupId>
PdfDocument::add_optional_content_group(const OptionalContentGroup &g) {
    auto id = add_object(FullPDFObject{std::format(R"(<<
  /Type /OCG
  /Name {}
>>
)",
                                                   pdfstring_quote(g.name)),
                                       {}});
    ocg_items.push_back(id);
    return CapyPDF_OptionalContentGroupId{(int32_t)ocg_items.size() - 1};
}

rvoe<CapyPDF_TransparencyGroupId> PdfDocument::add_transparency_group(PdfDrawContext &ctx) {
    if(ctx.draw_context_type() != CAPY_DC_TRANSPARENCY_GROUP) {
        RETERR(InvalidDrawContextType);
    }
    if(ctx.marked_content_depth() != 0) {
        RETERR(UnclosedMarkedContent);
    }
    auto sc_var = ctx.serialize();
    auto &d = std::get<SerializedXObject>(sc_var);
    auto objid = add_object(FullPDFObject{std::move(d.dict), std::move(d.command_stream)});
    transparency_groups.push_back(objid);
    return CapyPDF_TransparencyGroupId{(int32_t)transparency_groups.size() - 1};
}

rvoe<CapyPDF_SoftMaskId> PdfDocument::add_soft_mask(const SoftMask &sm) {
    auto id =
        add_object(FullPDFObject{std::format(R"(<<
  /Type /Mask
  /S /{}
  /G {} 0 R
>>
)",
                                             sm.S == CAPY_SOFT_MASK_ALPHA ? "Alpha" : "Luminosity",
                                             transparency_groups.at(sm.G.id)),
                                 {}});
    soft_masks.push_back(id);
    return CapyPDF_SoftMaskId{(int32_t)soft_masks.size() - 1};
}

std::optional<double>
PdfDocument::glyph_advance(CapyPDF_FontId fid, double pointsize, uint32_t codepoint) const {
    FT_Face face = fonts.at(fid.id).fontdata.face.get();
    FT_Set_Char_Size(face, 0, (FT_F26Dot6)(pointsize * 64), 300, 300);
    if(FT_Load_Char(face, codepoint, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0) {
        return {};
    }
    const auto font_unit_advance = face->glyph->metrics.horiAdvance;
    return (font_unit_advance / 64.0) / 300.0 * 72.0;
}

rvoe<CapyPDF_FontId> PdfDocument::load_font(FT_Library ft, const std::filesystem::path &fname) {
    ERC(fontdata, load_and_parse_truetype_font(fname));
    TtfFont ttf{std::unique_ptr<FT_FaceRec_, FT_Error (*)(FT_Face)>{nullptr, guarded_face_close},
                fname,
                std::move(fontdata)};
    FT_Face face;
    auto error = FT_New_Face(ft, fname.string().c_str(), 0, &face);
    if(error) {
        // By default Freetype is compiled without
        // error strings. Yay!
        RETERR(FreeTypeError);
    }
    ttf.face.reset(face);

    const char *font_format = FT_Get_Font_Format(face);
    if(!font_format) {
        RETERR(UnsupportedFormat);
    }
    if(strcmp(font_format,
              "TrueType")) { // != 0 &&
                             // strcmp(font_format,
                             // "CFF") != 0) {
        fprintf(stderr,
                "Only TrueType fonts are supported. %s "
                "is a %s font.",
                fname.string().c_str(),
                font_format);
        RETERR(UnsupportedFormat);
    }
    FT_Bytes base = nullptr;
    error = FT_OpenType_Validate(face, FT_VALIDATE_BASE, &base, nullptr, nullptr, nullptr, nullptr);
    if(!error) {
        fprintf(stderr,
                "Font file %s is an OpenType font. "
                "Only TrueType "
                "fonts are supported. Freetype error "
                "%d.",
                fname.string().c_str(),
                error);
        RETERR(UnsupportedFormat);
    }
    auto font_source_id = fonts.size();
    ERC(fss, FontSubsetter::construct(fname, face));
    fonts.emplace_back(FontThingy{std::move(ttf), std::move(fss)});

    const int32_t subset_num = 0;
    auto subfont_data_obj =
        add_object(DelayedSubsetFontData{CapyPDF_FontId{(int32_t)font_source_id}, subset_num});
    auto subfont_descriptor_obj = add_object(DelayedSubsetFontDescriptor{
        CapyPDF_FontId{(int32_t)font_source_id}, subfont_data_obj, subset_num});
    auto subfont_cmap_obj =
        add_object(DelayedSubsetCMap{CapyPDF_FontId{(int32_t)font_source_id}, subset_num});
    auto subfont_obj = add_object(DelayedSubsetFont{
        CapyPDF_FontId{(int32_t)font_source_id}, subfont_descriptor_obj, subfont_cmap_obj});
    (void)subfont_obj;
    CapyPDF_FontId fid{(int32_t)fonts.size() - 1};
    font_objects.push_back(
        FontInfo{subfont_data_obj, subfont_descriptor_obj, subfont_obj, fonts.size() - 1});
    return fid;
}

rvoe<NoReturnValue> PdfDocument::validate_format(const RawPixelImage &ri) const {
    // Check that the image has the correct format.
    if(std::holds_alternative<CapyPDF_PDFX_Type>(docprops.subtype)) {
        if(ri.md.cs == CAPY_IMAGE_CS_RGB) {
            // Later versions of PDFX permit rgb images with ICC colors, but let's start simple.
            RETERR(ImageFormatNotPermitted);
        }
    }
    RETOK;
}

} // namespace capypdf::internal
