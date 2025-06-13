// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <document.hpp>
#include <utils.hpp>
#include <drawcontext.hpp>
#include <objectformatter.hpp>
#include <bitfiddling.hpp>

#include <cassert>
#include <array>
#include <ft2build.h>
#include <variant>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include FT_OPENTYPE_VALIDATE_H

namespace capypdf::internal {

void FontCloser::del(FT_Face f) {
    if(f) {
        auto rc = FT_Done_Face(f);
        if(rc != 0) {
            fprintf(stderr, "Warning: closing Freetype font failed.\n");
        }
    }
}

void FreetypeCloser::del(FT_LibraryRec_ *ft) {
    if(ft) {
        auto rc = FT_Done_FreeType(ft);
        if(rc != 0) {
            fprintf(stderr, "Closing Freetype failed.\n");
        }
    }
}

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
   <pdfaid:part>%c</pdfaid:part>
   <pdfaid:conformance>%c</pdfaid:conformance>
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

const std::array<const char, 10> pdfa_part{'1', '1', '2', '2', '2', '3', '3', '3', '4', '4'};
const std::array<const char, 10> pdfa_conformance{'A', 'B', 'A', 'B', 'U', 'A', 'B', 'U', 'F', 'E'};

const std::array<const char *, 14> font_names{
    "/Times-Roman",
    "/Helvetica",
    "/Courier",
    "/Symbol",
    "/Times-Roman-Bold",
    "/Helvetica-Bold",
    "/Courier-Bold",
    "/ZapfDingbats",
    "/Times-Italic",
    "/Helvetica-Oblique",
    "/Courier-Oblique",
    "/Times-BoldItalic",
    "/Helvetica-BoldOblique",
    "/Courier-BoldOblique",
};

const std::array<const char *, 16> blend_mode_names{
    "/Normal",
    "/Multiply",
    "/Screen",
    "/Overlay",
    "/Darken",
    "/Lighten",
    "/ColorDodge",
    "/ColorBurn",
    "/HardLight",
    "/SoftLight",
    "/Difference",
    "/Exclusion",
    "/Hue",
    "/Saturation",
    "/Color",
    "/Luminosity",
};

template<typename T> rvoe<NoReturnValue> append_floatvalue(pystd2025::Bytes &buf, double v) {
    if(v < 0 || v > 1.0) {
        RETERR(ColorOutOfRange);
    }
    T cval = (T)(std::numeric_limits<T>::max() * v);
    cval = byteswap(cval);
    const char *ptr = (const char *)(&cval);
    buf.append(ptr, ptr + sizeof(T));
    RETOK;
}

rvoe<pystd2025::Bytes> serialize_shade4(const ShadingType4 &shade) {
    pystd2025::Bytes s;
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
    return rvoe<pystd2025::Bytes>{std::move(s)};
}

rvoe<pystd2025::Bytes> serialize_shade6(const ShadingType6 &shade) {
    pystd2025::Bytes s;
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

void color2numbers(ObjectFormatter &fmt, const Color &c) {
    if(auto *rgb = std::get_if<DeviceRGBColor>(&c)) {
        fmt.add_token(rgb->r.v());
        fmt.add_token(rgb->g.v());
        fmt.add_token(rgb->b.v());
    } else if(auto *gray = std::get_if<DeviceGrayColor>(&c)) {
        fmt.add_token(gray->v.v());
    } else if(auto *cmyk = std::get_if<DeviceCMYKColor>(&c)) {
        fmt.add_token(cmyk->c.v());
        fmt.add_token(cmyk->m.v());
        fmt.add_token(cmyk->y.v());
        fmt.add_token(cmyk->k.v());
    } else {
        fprintf(stderr, "Colorspace not supported yet.\n");
        std::abort();
    }
}

template<typename T1, typename T2> static void append_value_or_null(T1 &fmt, const T2 &val) {
    if(val) {
        fmt.add_token(val.value());
    } else {
        fmt.add_token("null");
    }
}

struct NameProxy {
    pystd2025::CStringView name;
    int i;

    bool operator<(const NameProxy &o) const { return name < o.name; }
};

std::vector<NameProxy> sort_names(const std::vector<EmbeddedFileObject> &names) {
    std::vector<NameProxy> result;
    result.reserve(names.size());
    int num = 0;
    for(const auto &n : names) {
        result.emplace_back(NameProxy{n.ef.pdfname.sv(), num});
        ++num;
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace

const std::array<const char *, 4> rendering_intent_names{
    "RelativeColorimetric",
    "AbsoluteColorimetric",
    "Saturation",
    "Perceptual",
};

DocumentProperties::DocumentProperties() {
    default_page_properties.mediabox = PdfRectangle::a4();
    compress_streams = (getenv("CAPY_DEBUG_PDF") == nullptr);
}

PdfVersion DocumentProperties::version() const {
    if(auto *pdfa = subtype.get_if<CapyPDF_PDFA_Type>()) {
        if(*pdfa >= CAPY_PDFA_4f) {
            return PdfVersion::v20;
        }
    }
    return PdfVersion::v17;
}

bool DocumentProperties::use_rdf_metadata() const {
    if(auto *pdfa = subtype.get_if<CapyPDF_PDFA_Type>()) {
        if(*pdfa >= CAPY_PDFA_4f) {
            return true;
        }
    }
    return false;
}

bool DocumentProperties::require_embedded_files() const {
    if(auto *pdfa = subtype.get_if<CapyPDF_PDFA_Type>()) {
        if(*pdfa >= CAPY_PDFA_4f) {
            return true;
        }
    }
    return false;
}

rvoe<NoReturnValue>
serialize_destination(ObjectFormatter &fmt, const Destination &dest, int32_t page_object_number) {
    fmt.add_token("/Dest");
    fmt.begin_array();
    fmt.add_object_ref(page_object_number);
    if(auto xyz = std::get_if<DestinationXYZ>(&dest.loc)) {
        fmt.add_token("/XYZ");
        append_value_or_null(fmt, xyz->x);
        append_value_or_null(fmt, xyz->y);
        append_value_or_null(fmt, xyz->z);
    } else if(std::holds_alternative<DestinationFit>(dest.loc)) {
        fmt.add_token("/Fit");
    } else if(auto r = std::get_if<DestinationFitR>(&dest.loc)) {
        fmt.add_token("/FitR");
        fmt.add_token(r->left);
        fmt.add_token(r->bottom);
        fmt.add_token(r->right);
        fmt.add_token(r->top);
    } else {
        fprintf(stderr, "No link target specified.\n");
        std::abort();
    }
    fmt.end_array();
    RETOK;
}

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
        if(!cm.get_rgb().is_empty()) {
            ERC(retval, add_icc_profile(cm.get_rgb(), 3));
            output_profile = retval;
        }
        break;
    case CAPY_DEVICE_CS_GRAY:
        if(!cm.get_gray().is_empty()) {
            ERC(retval, add_icc_profile(cm.get_gray(), 1));
            output_profile = retval;
        }
        break;
    case CAPY_DEVICE_CS_CMYK:
        if(cm.get_cmyk().is_empty()) {
            RETERR(OutputProfileMissing);
        }
        ERC(retval, add_icc_profile(cm.get_cmyk(), 4));
        output_profile = retval;
        break;
    }
    document_objects.push_back(DelayedPages{});
    pages_object = document_objects.size() - 1;
    if(!docprops.subtype.contains<pystd2025::Monostate>()) {
        if(!output_profile) {
            RETERR(OutputProfileMissing);
        }
        if(docprops.intent_condition_identifier.empty()) {
            RETERR(MissingIntentIdentifier);
        }
        create_output_intent();
    }
    if(!docprops.metadata_xml.empty() || docprops.subtype.contains<CapyPDF_PDFA_Type>()) {
        document_md_object = add_document_metadata_object();
    }
    RETOK;
}

rvoe<NoReturnValue> PdfDocument::add_page(pystd2025::CString resource_dict,
                                          pystd2025::CString command_stream,
                                          const PageProperties &custom_props,
                                          const pystd2025::HashSet<CapyPDF_FormWidgetId> &fws,
                                          const pystd2025::HashSet<CapyPDF_AnnotationId> &annots,
                                          const pystd2025::Vector<CapyPDF_StructureItemId> &structs,
                                          const pystd2025::Optional<Transition> &transition,
                                          const std::vector<SubPageNavigation> &subnav) {
    for(const auto &a : fws) {
        if(form_use.contains(a)) {
            RETERR(AnnotationReuse);
        }
    }
    for(const auto &a : annots) {
        if(annotation_use.lookup(a)) {
            RETERR(AnnotationReuse);
        }
    }
    for(const auto &s : structs) {
        if(structure_use.lookup(s)) {
            RETERR(StructureReuse);
        }
    }
    ObjectFormatter fmt;
    fmt.begin_dict();
    const auto resource_num = add_object(FullPDFObject{std::move(resource_dict), {}});
    int32_t commands_num{-1};
    commands_num =
        add_object(DeflatePDFObject{std::move(fmt), RawData{std::move(command_stream)}, true});
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
    if(!structs.is_empty()) {
        p.structparents = (int32_t)structure_parent_tree_items.size();
        structure_parent_tree_items.push_back(structs);
    }
    const auto page_object_num = add_object(std::move(p));
    for(const auto &fw : fws) {
        form_use.insert(fw, page_object_num);
    }
    for(const auto &a : annots) {
        annotation_use.insert(a, page_object_num);
    }
    int32_t mcid_num = 0;
    for(const auto &s : structs) {
        structure_use.insert(s, StructureUsage{(int32_t)pages.size(), mcid_num++});
    }
    pages.emplace_back(PageOffsets{resource_num, commands_num, page_object_num});
    RETOK;
}

rvoe<NoReturnValue>
PdfDocument::add_page_labeling(uint32_t start_page,
                               pystd2025::Optional<CapyPDF_Page_Label_Number_Style> style,
                               pystd2025::Optional<u8string> prefix,
                               pystd2025::Optional<uint32_t> start_num) {
    if(!page_labels.empty() && page_labels.back().start_page > start_page) {
        RETERR(NonSequentialPageNumber);
    }
    page_labels.emplace_back(start_page, style, std::move(prefix), start_num);
    return NoReturnValue{};
}

// Form XObjects
void PdfDocument::add_form_xobject(ObjectFormatter xobj_dict, pystd2025::CString xobj_stream) {
    const auto xobj_num =
        add_object(DeflatePDFObject{std::move(xobj_dict), RawData(std::move(xobj_stream)), true});

    form_xobjects.emplace_back(FormXObjectInfo{xobj_num});
}

int32_t PdfDocument::create_subnavigation(const std::vector<SubPageNavigation> &subnav) {
    assert(!subnav.empty());
    const int32_t root_obj = document_objects.size();
    {
        ObjectFormatter fmt;
        fmt.begin_dict();
        fmt.add_token_pair("/Type", "/NavNode");
        fmt.add_token("/NA");
        {
            fmt.begin_dict();
            fmt.add_token_pair("/S", "/SetOCGState");
            fmt.add_token("/State");
            fmt.begin_array(1);
            fmt.add_token("/OFF");
            for(const auto &i : subnav) {
                fmt.add_object_ref(ocg_object_number(i.id));
            }
            fmt.end_array();
            fmt.end_dict();
        }
        fmt.add_token("/Next");
        fmt.add_object_ref(root_obj + 1);
        fmt.add_token("/PA");
        {
            fmt.begin_dict();
            fmt.add_token_pair("/S", "/SetOCGState");
            fmt.add_token("/State");
            fmt.begin_array(1);
            fmt.add_token("/ON");
            for(const auto &i : subnav) {
                fmt.add_object_ref(ocg_object_number(i.id));
            }
            fmt.end_array();
            fmt.end_dict();
        }
        fmt.add_token("/Prev");
        fmt.add_object_ref(root_obj + 1 + subnav.size());
        fmt.end_dict();

        add_object(FullPDFObject{fmt.steal(), {}});
    }
    int32_t first_obj = document_objects.size();

    for(size_t i = 0; i < subnav.size(); ++i) {
        const auto &sn = subnav[i];
        ObjectFormatter fmt;
        fmt.begin_dict();
        fmt.add_token_pair("/Type", "/NavNode");
        fmt.add_token("/NA");
        {
            fmt.begin_dict();
            fmt.add_token_pair("/S", "/SetOCGState");
            fmt.add_token("/State");
            fmt.begin_array(1);
            fmt.add_token("/ON");
            fmt.add_object_ref(ocg_object_number(sn.id));
            fmt.end_array();
            if(sn.tr) {
                fmt.add_token("/Next");
                fmt.begin_dict();
                fmt.add_token_pair("/S", "/Trans");
                serialize_trans(fmt, *sn.tr);
                fmt.end_dict();
            }

            fmt.end_dict();
        }
        fmt.add_token("/Next");
        fmt.add_object_ref(first_obj + i + 1);
        if(i > 0) {
            fmt.add_token("/PA");
            fmt.begin_dict();
            fmt.add_token_pair("/S", "/SetOCGState");
            fmt.add_token("/State");
            fmt.begin_array(1);
            fmt.add_token("/OFF");
            fmt.add_object_ref(ocg_object_number(subnav[i - 1].id));
            fmt.end_array();
            fmt.end_dict();
            fmt.add_token("/Prev");
            fmt.add_object_ref(first_obj + i - 1);
        }
        fmt.end_dict();
        add_object(FullPDFObject{fmt.steal(), {}});
    }
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/NavNode");
    fmt.add_token("/PA");
    fmt.begin_dict();
    fmt.add_token_pair("/S", "/SetOCGState");
    fmt.add_token("/State");
    fmt.begin_array(1);
    fmt.add_token("/OFF");
    fmt.add_object_ref(ocg_object_number(subnav.back().id));
    fmt.end_array();
    fmt.end_dict();
    fmt.add_token("/Prev");
    fmt.add_object_ref(first_obj + subnav.size() - 1);
    fmt.end_dict();
    add_object(FullPDFObject{fmt.steal(), {}});
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
    ObjectFormatter fmt;
    fmt.begin_array();
    fmt.add_token("/Separation");
    auto name_obj = bytes2pdfstringliteral(name.sv(), true);
    fmt.add_token(name_obj);
    fmt.add_token(colorspace_names.at((int)cs));
    fmt.add_object_ref(f4.object_number);
    fmt.end_array();
    separation_objects.push_back(add_object(FullPDFObject{fmt.steal(), {}}));
    return CapyPDF_SeparationId{(int32_t)separation_objects.size() - 1};
}

rvoe<CapyPDF_LabColorSpaceId> PdfDocument::add_lab_colorspace(const LabColorSpace &lab) {
    ObjectFormatter fmt;
    fmt.begin_array();
    fmt.add_token("/Lab");
    {
        fmt.begin_dict();

        fmt.add_token("/WhitePoint");
        {
            fmt.begin_array();
            fmt.add_token(lab.xw);
            fmt.add_token(lab.yw);
            fmt.add_token(lab.zw);
            fmt.end_array();
        }

        fmt.add_token("/Range");
        {
            fmt.begin_array();
            fmt.add_token(lab.amin);
            fmt.add_token(lab.amax);
            fmt.add_token(lab.bmin);
            fmt.add_token(lab.bmax);
            fmt.end_array();
        }
        fmt.end_dict();
    }
    fmt.end_array();

    add_object(FullPDFObject{fmt.steal(), {}});
    return CapyPDF_LabColorSpaceId{(int32_t)document_objects.size() - 1};
}

rvoe<CapyPDF_IccColorSpaceId> PdfDocument::load_icc_file(const pystd2025::Path &fname) {
    ERC(contents, load_file_as_bytes(fname));
    const auto iccid = find_icc_profile(contents);
    if(iccid) {
        return *iccid;
    }
    ERC(num_channels, cm.get_num_channels(contents));
    return add_icc_profile(contents, num_channels);
}

rvoe<int32_t> PdfDocument::create_name_dict() {
    ObjectFormatter fmt;
    auto sorted_names = sort_names(embedded_files);

    fmt.begin_dict();
    fmt.add_token("/EmbeddedFiles");
    fmt.begin_dict();
    // PDF/A 4 mandates that you have an EmbeddedFiles key, even if it is empty.
    // At least that is what the VeraPDF validator says.
    if(!embedded_files.empty()) {

        fmt.add_token("/Limits");

        fmt.begin_array(2);
        fmt.add_token(pdfstring_quote(sorted_names.front().name));
        fmt.add_token(pdfstring_quote(sorted_names.back().name));
        fmt.end_array();

        fmt.add_token("/Names");

        fmt.begin_array(2);
        for(const auto &e : sorted_names) {
            fmt.add_token(pdfstring_quote(e.name));
            fmt.add_object_ref(embedded_files[e.i].filespec_obj);
        }
        fmt.end_array();
    }
    fmt.end_dict();
    fmt.end_dict();
    return add_object(FullPDFObject{fmt.steal(), {}});
}

rvoe<int32_t> PdfDocument::create_AF_dict() {
    ObjectFormatter fmt;
    fmt.begin_array(1);
    for(const auto &e : embedded_files) {
        fmt.begin_dict();
        fmt.add_token_pair("/Type", "/Filespec");
        if(!e.ef.description.empty()) {
            fmt.add_token("/Desc");
            fmt.add_token(u8str2u8textstring(e.ef.description));
        }
        fmt.add_token_pair("/AFRelationship", "/Data");
        // Yes, these two are the same. For BW reasons I guess?
        fmt.add_token("/F");
        fmt.add_token(u8str2filespec(e.ef.pdfname));
        fmt.add_token("/UF");
        fmt.add_token(u8str2filespec(e.ef.pdfname));
        fmt.add_token("/EF");
        {
            fmt.begin_dict();
            fmt.add_token("/F");
            fmt.add_object_ref(e.contents_obj);
            fmt.add_token("/UF");
            fmt.add_object_ref(e.contents_obj);
            fmt.end_dict();
        }
        fmt.end_dict();
    }
    fmt.end_array();
    return add_object(FullPDFObject{fmt.steal(), {}});
}

rvoe<int32_t> PdfDocument::create_structure_parent_tree() {
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token("/Nums");
    fmt.begin_array();

    for(size_t i = 0; i < structure_parent_tree_items.size(); ++i) {
        const auto &entry = structure_parent_tree_items[i];
        fmt.add_token(i);
        fmt.begin_array(1);
        for(const auto &sitem : entry) {
            fmt.add_object_ref(structure_items.at(sitem.id).obj_id);
        }
        fmt.end_array();
    }
    fmt.end_array();
    fmt.end_dict();
    return add_object(FullPDFObject{fmt.steal(), {}});
}

rvoe<CapyPDF_RoleId> PdfDocument::add_rolemap_entry(pystd2025::CString name,
                                                    CapyPDF_Structure_Type builtin_type) {
    if(name.is_empty() || name.front() == '/') {
        RETERR(SlashStart);
    }
    for(const auto &i : rolemap) {
        if(strcmp(i.name.c_str(), name.c_str()) == 0) {
            RETERR(RoleAlreadyDefined);
        }
    }
    rolemap.emplace_back(RolemapEnty{pystd2025::CString(name.c_str()), builtin_type});
    return CapyPDF_RoleId{(int32_t)rolemap.size() - 1};
}

rvoe<NoReturnValue> PdfDocument::create_catalog() {
    ObjectFormatter fmt;
    pystd2025::Optional<int32_t> outline_object;
    pystd2025::Optional<int32_t> structure_object;
    pystd2025::Optional<int32_t> AF_object;
    pystd2025::Optional<int32_t> names_object;

    fmt.begin_dict();

    if(docprops.require_embedded_files() || !embedded_files.empty()) {
        ERC(names, create_name_dict());
        names_object = names;
        ERC(afnum, create_AF_dict());
        AF_object = afnum;
    }
    if(!outlines.items.empty()) {
        ERC(outlines, create_outlines());
        outline_object = outlines;
    }
    if(!structure_items.empty()) {
        ERC(treeid, create_structure_parent_tree());
        structure_parent_tree_object = treeid;
        create_structure_root_dict();
        structure_object = *structure_root_object;
    }
    fmt.add_token_pair("/Type", "/Catalog");
    fmt.add_token("/Pages");
    fmt.add_object_ref(pages_object);

    if(!page_labels.empty()) {
        fmt.add_token("/PageLabels");
        fmt.begin_dict();
        fmt.add_token("/Nums");
        fmt.begin_array(2);
        for(const auto &page_label : page_labels) {
            fmt.add_token(page_label.start_page);
            fmt.begin_dict();
            if(page_label.style) {
                fmt.add_token("/S");
                fmt.add_token(page_label_types.at(*page_label.style));
            }
            if(page_label.prefix) {
                fmt.add_token("/P");
                fmt.add_token(utf8_to_pdfutf16be(*page_label.prefix).c_str());
            }
            if(page_label.start_num) {
                fmt.add_token("/St");
                fmt.add_token(*page_label.start_num);
            }
            fmt.end_dict();
        }
        fmt.end_array();
        fmt.end_dict();
    }
    if(outline_object) {
        fmt.add_token("/Outlines");
        fmt.add_token(*outline_object);
    }
    if(names_object) {
        fmt.add_token("/Names");
        fmt.add_object_ref(*names_object);
    }
    if(AF_object) {
        fmt.add_token("/AF");
        fmt.add_object_ref(*AF_object);
    }
    if(structure_object) {
        fmt.add_token("/StructTreeRoot");
        fmt.add_object_ref(*structure_object);
    }
    if(!docprops.lang.empty()) {
        fmt.add_token("/Lang");
        fmt.add_pdfstring(docprops.lang);
    }
    if(docprops.is_tagged) {
        fmt.add_token("/MarkInfo");
        fmt.begin_dict();
        fmt.add_token_pair("/Marked", "true");
        fmt.end_dict();
    }
    if(output_intent_object) {
        fmt.add_token("/OutputIntents");
        fmt.begin_array();
        fmt.add_object_ref(*output_intent_object);
        fmt.end_array();
    }
    if(!form_use.is_empty()) {
        fmt.add_token("/AcroForm");
        fmt.begin_dict();
        fmt.add_token("/Fields");
        fmt.begin_array(1);
        for(const auto &i : form_widgets) {
            fmt.add_object_ref(i);
        }
        fmt.end_array();
        fmt.end_dict();
        fmt.add_token_pair("/NeedAppearances", "true");
    }
    if(!ocg_items.is_empty()) {
        fmt.add_token("/OCProperties");
        fmt.begin_dict();
        fmt.add_token("/OCGs");
        fmt.begin_array(1);
        for(const auto &o : ocg_items) {
            fmt.add_object_ref(o);
        }
        fmt.end_array();
        fmt.add_token("/D");
        fmt.begin_dict();
        fmt.add_token_pair("/BaseState", "/ON");
        fmt.end_dict();
        fmt.end_dict();
    }
    if(document_md_object) {
        fmt.add_token("/Metadata");
        fmt.add_object_ref(*document_md_object);
    }
    fmt.end_dict();
    add_object(FullPDFObject{fmt.steal(), {}});
    RETOK;
}

void PdfDocument::create_output_intent() {
    assert(output_profile);
    ObjectFormatter fmt;

    assert(!docprops.subtype.contains<pystd2025::Monostate>());
    const char *gts = docprops.subtype.contains<CapyPDF_PDFX_Type>() ? "/GTS_PDFX" : "/GTS_PDFA1";
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "OutputIntent");
    fmt.add_token_pair("/S", gts);
    fmt.add_token("/OutputConditionIdentifier");
    fmt.add_token(utf8_to_pdfutf16be(docprops.intent_condition_identifier));
    fmt.add_token("/DestOutputProfile");
    fmt.add_object_ref(get(*output_profile).stream_num);
    fmt.end_dict();
    output_intent_object = add_object(FullPDFObject{fmt.steal(), {}});
}

rvoe<int32_t> PdfDocument::create_outlines() {
    int32_t first_obj_num = (int32_t)document_objects.size();
    int32_t catalog_obj_num = first_obj_num + (int32_t)outlines.items.size();
    for(int32_t cur_id = 0; cur_id < (int32_t)outlines.items.size(); ++cur_id) {
        const auto &cur_obj = outlines.items[cur_id];
        auto titlestr = utf8_to_pdfutf16be(cur_obj.title);
        auto parent_id = outlines.parent.at(cur_id);
        const auto &siblings = outlines.children.at(parent_id);
        ObjectFormatter fmt;
        fmt.begin_dict();
        fmt.add_token("/Title");
        fmt.add_token(titlestr);
        if(cur_obj.dest) {
            const auto &dest = cur_obj.dest.value();
            const auto physical_page = dest.page;
            if(physical_page < 0 || physical_page >= (int32_t)pages.size()) {
                RETERR(InvalidPageNumber);
            }
            const auto page_object_number = pages.at(physical_page).page_obj_num;
            serialize_destination(fmt, dest, page_object_number);
        }
        if(siblings.size() > 1) {
            auto loc = std::find(siblings.begin(), siblings.end(), cur_id);
            assert(loc != siblings.end());
            if(loc != siblings.begin()) {
                auto prevloc = loc;
                --prevloc;
                fmt.add_token("/Prev");
                fmt.add_object_ref(first_obj_num + *prevloc);
            }
            auto nextloc = loc;
            nextloc++;
            if(nextloc != siblings.end()) {
                fmt.add_token("/Next");
                fmt.add_token(first_obj_num + *nextloc);
            }
        }
        auto childs = outlines.children.find(cur_id);
        if(childs != outlines.children.end()) {
            const auto &children = childs->second;
            fmt.add_token("/First");
            fmt.add_object_ref(first_obj_num + children.front());
            fmt.add_token("/Last");
            fmt.add_object_ref(first_obj_num + children.back());
            fmt.add_token("/Count");
            fmt.add_token(-(int32_t)children.size());
        }
        fmt.add_token("/Parent");
        fmt.add_object_ref(parent_id >= 0 ? first_obj_num + parent_id : catalog_obj_num);
        if(cur_obj.F != 0) {
            fmt.add_token("/F");
            fmt.add_token(cur_obj.F);
        }
        if(cur_obj.C) {
            fmt.add_token("/C");
            fmt.begin_array();
            fmt.add_token(cur_obj.C.value().r.v());
            fmt.add_token(cur_obj.C.value().g.v());
            fmt.add_token(cur_obj.C.value().b.v());
            fmt.end_array();
        }
        fmt.end_dict();
        add_object(FullPDFObject{fmt.steal(), {}});
    }
    const auto &top_level = outlines.children.at(-1);

    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/Outlines");
    fmt.add_token("/First");
    fmt.add_object_ref(first_obj_num + top_level.front());
    fmt.add_token("/Last");
    fmt.add_object_ref(first_obj_num + top_level.back());
    fmt.add_token("/Count");
    fmt.add_token(outlines.children.at(-1).size());
    fmt.end_dict();

    assert(catalog_obj_num == (int32_t)document_objects.size());
    // FIXME: add output intents here. PDF spec 14.11.5
    return add_object(FullPDFObject{fmt.steal(), {}});
}

void PdfDocument::create_structure_root_dict() {
    pystd2025::Optional<CapyPDF_StructureItemId> rootobj;

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
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/StructTreeRoot");
    fmt.add_token("/K");
    fmt.begin_array();
    fmt.add_object_ref(structure_items[rootobj->id].obj_id);
    fmt.end_array();
    fmt.add_token_pair("/ParentTree", structure_parent_tree_object.value());
    fmt.add_token_pair("/ParentTreeNextKey", structure_parent_tree_items.size());
    if(!rolemap.is_empty()) {
        fmt.add_token("/RoleMap");
        fmt.begin_dict();
        for(const auto &i : rolemap) {
            fmt.add_token_pair(bytes2pdfstringliteral(i.name.view()),
                               structure_type_names.at(i.builtin));
        }
        fmt.end_dict();
    }
    fmt.end_dict();
    structure_root_object = add_object(FullPDFObject{fmt.steal(), {}});
}

int32_t PdfDocument::add_document_metadata_object() {
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/Metadata");
    fmt.add_token_pair("/Subtype", "/XML");
    if(docprops.metadata_xml.empty()) {
        auto *aptr = docprops.subtype.get_if<CapyPDF_PDFA_Type>();
        if(!aptr) {
            std::abort();
        }
        auto stream = pystd2025::format(
            pdfa_rdf_template, (char *)rdf_magic, pdfa_part.at(*aptr), pdfa_conformance.at(*aptr));
        fmt.add_token_pair("/Length", stream.size());
        fmt.end_dict();
        return add_object(FullPDFObject{fmt.steal(), RawData(pystd2025::CString(stream.c_str()))});
    } else {
        fmt.add_token_pair("/Length", docprops.metadata_xml.length());
        fmt.end_dict();
        return add_object(FullPDFObject{fmt.steal(), RawData(docprops.metadata_xml.sv())});
    }
}

pystd2025::Optional<CapyPDF_IccColorSpaceId>
PdfDocument::find_icc_profile(pystd2025::BytesView contents) {
    for(size_t i = 0; i < icc_profiles.size(); ++i) {
        const auto &stream_obj = document_objects.at(icc_profiles.at(i).stream_num);
        if(const auto stream_data = stream_obj.get_if<DeflatePDFObject>()) {
            if(stream_data->stream == contents) {
                return CapyPDF_IccColorSpaceId{(int32_t)i};
            }
        } else {
            fprintf(stderr, "Bad type for icc profile dnta.\n");
            std::abort();
        }
    }
    return {};
}

rvoe<CapyPDF_IccColorSpaceId> PdfDocument::add_icc_profile(pystd2025::BytesView contents,
                                                           int32_t num_channels) {
    auto existing = find_icc_profile(contents);
    if(existing) {
        return existing.value();
    }
    if(contents.is_empty()) {
        return CapyPDF_IccColorSpaceId{-1};
    }
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/N", num_channels);
    auto stream_obj_id = add_object(DeflatePDFObject{std::move(fmt), RawData(contents)});
    auto str = pystd2025::format("[ /ICCBased %d 0 R ]\n", stream_obj_id);
    auto obj_id = add_object(FullPDFObject{pystd2025::CString(str.c_str()), {}});
    icc_profiles.emplace_back(IccInfo{stream_obj_id, obj_id, num_channels});
    return CapyPDF_IccColorSpaceId{(int32_t)icc_profiles.size() - 1};
}

rvoe<NoReturnValue> PdfDocument::generate_info_object() {
    ObjectFormatter fmt;
    const auto current_date = current_date_string();
    fmt.begin_dict();
    fmt.add_token_pair("/ModDate", current_date);
    if(!docprops.use_rdf_metadata()) {
        if(!docprops.title.empty()) {
            fmt.add_token_pair("/Title ", utf8_to_pdfutf16be(docprops.title));
        }
        if(!docprops.author.empty()) {
            fmt.add_token_pair("/Author ", utf8_to_pdfutf16be(docprops.author));
        }
        if(!docprops.creator.empty()) {
            fmt.add_token_pair("/Creator ", utf8_to_pdfutf16be(docprops.creator));
        }
        fmt.add_token_pair("/Producer", "(CapyPDF " CAPYPDF_VERSION_STR ")");
        fmt.add_token_pair("/CreationDate", current_date);
    }
    if(auto xptr = docprops.subtype.get_if<CapyPDF_PDFX_Type>()) {
        pystd2025::CString parname("(");
        parname += pdfx_names.at(*xptr);
        parname += ")\n";
        fmt.add_token_pair("/GTS_PDFXVersion", parname);
        fmt.add_token_pair("/Trapped", "/False");
    }
    fmt.end_dict();
    add_object(FullPDFObject{fmt.steal(), {}});
    RETOK;
}

CapyPDF_FontId PdfDocument::get_builtin_font_id(CapyPDF_Builtin_Fonts font) {
    auto it = builtin_fonts.lookup(font);
    if(it) {
        return *it;
    }
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/Font");
    fmt.add_token_pair("/Subtype", "/Type1");
    fmt.add_token_pair("/BaseFont", font_names[font]);
    fmt.end_dict();
    font_objects.push_back(
        FontPDFObjects{-1, -1, add_object(FullPDFObject{fmt.steal(), {}}), {}, size_t(-1)});
    auto fontid = CapyPDF_FontId{(int32_t)font_objects.size() - 1};
    builtin_fonts.insert(font, fontid);
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
    auto glyphid = glyph_for_codepoint(face, codepoint);
    return glyphid != 0;
}

rvoe<SubsetGlyph> PdfDocument::get_subset_glyph(CapyPDF_FontId fid,
                                                uint32_t codepoint,
                                                const pystd2025::Optional<uint32_t> glyph_id) {
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
                            pystd2025::Optional<int32_t>{},
                            params,
                            image.pixels,
                            image.md.compression);
}

rvoe<CapyPDF_ImageId> PdfDocument::add_image(RawPixelImage image,
                                             const ImagePDFProperties &params) {
    if(image.md.w < 1 || image.md.h < 1) {
        RETERR(InvalidImageSize);
    }
    if(image.pixels.is_empty()) {
        RETERR(MissingPixels);
    }
    ERCV(validate_format(image));
    pystd2025::Optional<int32_t> smask_id;
    if(params.as_mask && !image.alpha.is_empty()) {
        RETERR(MaskAndAlpha);
    }
    if(!image.alpha.is_empty()) {
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
    if(!image.icc_profile.is_empty()) {
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
                                                    pystd2025::Optional<int32_t> smask_id,
                                                    const ImagePDFProperties &params,
                                                    pystd2025::BytesView original_bytes,
                                                    CapyPDF_Compression compression) {
    pystd2025::Bytes compression_buffer;
    pystd2025::BytesView compressed_bytes;
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

    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/XObject");
    fmt.add_token_pair("/Subtype", "/Image");
    fmt.add_token_pair("/Width", w);
    fmt.add_token_pair("/Height", h);
    fmt.add_token_pair("/BitsPerComponent", bits_per_component);
    fmt.add_token_pair("/Length", compressed_bytes.size());
    fmt.add_token_pair("/Filter", "/FlateDecode");

    // Auto means don't specify the interpolation
    if(params.interp == CAPY_INTERPOLATION_PIXELATED) {
        fmt.add_token_pair("/Interpolate", "false");
    } else if(params.interp == CAPY_INTERPOLATION_SMOOTH) {
        fmt.add_token_pair("/Interpolate", "true");
    }

    // An image may only have ImageMask or ColorSpace key, not both.
    if(params.as_mask) {
        fmt.add_token_pair("/ImageMask", " true");
    } else {
        if(auto cs = colorspace.get_if<CapyPDF_Image_Colorspace>()) {
            fmt.add_token_pair("/ColorSpace", colorspace_names.at(*cs));
        } else if(auto icc = colorspace.get_if<CapyPDF_IccColorSpaceId>()) {
            const auto icc_obj = get(*icc).object_num;
            fmt.add_token("/ColorSpace");
            fmt.add_object_ref(icc_obj);
        } else {
            fprintf(stderr, "Unknown colorspace.");
            std::abort();
        }
    }
    if(smask_id) {
        fmt.add_token("/SMask");
        fmt.add_object_ref(smask_id.value());
    }
    fmt.end_dict();
    int32_t im_id;
    if(compression == CAPY_COMPRESSION_NONE) {
        im_id = add_object(FullPDFObject{fmt.steal(), RawData(std::move(compression_buffer))});
    } else {
        // FIXME. Makes a copy. Fix to grab original data instead.
        im_id = add_object(FullPDFObject{fmt.steal(), RawData(original_bytes)});
    }
    image_info.emplace_back(ImageInfo{{w, h}, im_id});
    return CapyPDF_ImageId{(int32_t)image_info.size() - 1};
}

rvoe<CapyPDF_ImageId> PdfDocument::embed_jpg(jpg_image jpg, const ImagePDFProperties &props) {
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/XObject");
    fmt.add_token_pair("/Subtype", "/Image");
    fmt.add_token_pair("/Width", jpg.w);
    fmt.add_token_pair("/Height", jpg.h);
    fmt.add_token_pair("/BitsPerComponent", jpg.depth);
    fmt.add_token_pair("/Length", jpg.file_contents.size());
    fmt.add_token_pair("/Filter", "/DCTDecode");

    if(!jpg.domain.empty()) {
        assert(jpg.cs == CAPY_DEVICE_CS_CMYK);
        fmt.add_token("/Decode");
        fmt.add_array(jpg.domain);
    }
    if(jpg.icc_profile.is_empty()) {
        fmt.add_token_pair("/ColorSpace", colorspace_names.at(jpg.cs));
    } else {
        int32_t expected_channels;
        switch(jpg.cs) {
        case CAPY_DEVICE_CS_RGB:
            expected_channels = 3;
            break;
        case CAPY_DEVICE_CS_GRAY:
            expected_channels = 1;
            break;
        case CAPY_DEVICE_CS_CMYK:
            expected_channels = 4;
            break;
        default:
            std::abort();
        }

        // Note: the profile is now stored twice in the output file.
        // Once as a PDF object an a second time in the embedded image file.
        ERC(icc, add_icc_profile(jpg.icc_profile, expected_channels));
        fmt.add_token("/ColorSpace");
        fmt.add_object_ref(icc_profiles.at(icc.id).object_num);
    }
    // Auto means don't specify the interpolation
    if(props.interp == CAPY_INTERPOLATION_PIXELATED) {
        fmt.add_token_pair("/Interpolate", "false");
    } else if(props.interp == CAPY_INTERPOLATION_SMOOTH) {
        fmt.add_token_pair("/Interpolate", "true");
    }
    // FIXME, add other properties too?

    fmt.end_dict();
    auto im_id = add_object(FullPDFObject{fmt.steal(), RawData(std::move(jpg.file_contents))});
    image_info.emplace_back(ImageInfo{{jpg.w, jpg.h}, im_id});
    return CapyPDF_ImageId{(int32_t)image_info.size() - 1};
}

rvoe<CapyPDF_GraphicsStateId> PdfDocument::add_graphics_state(const GraphicsState &state) {
    const int32_t id = (int32_t)document_objects.size();
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/ExtGState");
    if(state.LW) {
        fmt.add_token_pair("/LW", *state.LW);
    }
    if(state.LC) {
        fmt.add_token_pair("/LC", (int)*state.LC);
    }
    if(state.LJ) {
        fmt.add_token_pair("/LJ", (int)*state.LJ);
    }
    if(state.ML) {
        fmt.add_token_pair("/ML", *state.ML);
    }
    if(state.RI) {
        fmt.add_token_pair("/RenderingIntent", rendering_intent_names.at(*state.RI));
    }
    if(state.OP) {
        fmt.add_token_pair("/OP", *state.OP ? "true" : "false");
    }
    if(state.op) {
        fmt.add_token_pair("/op2", *state.op ? "true" : "false");
    }
    if(state.OPM) {
        fmt.add_token_pair("/OPM", *state.OPM);
    }
    if(state.FL) {
        fmt.add_token_pair("/FL", *state.FL);
    }
    if(state.SM) {
        fmt.add_token_pair("/SM", *state.SM);
    }
    if(state.BM) {
        fmt.add_token_pair("/BM", blend_mode_names.at(*state.BM));
    }
    if(state.SMask) {
        auto objnum = soft_masks.at(state.SMask->id);
        fmt.add_token("/SMask");
        fmt.add_object_ref(objnum);
    }
    if(state.CA) {
        fmt.add_token_pair("/CA", state.CA->v());
    }
    if(state.ca) {
        fmt.add_token_pair("/ca", state.ca->v());
    }
    if(state.AIS) {
        fmt.add_token_pair("/AIS {}", *state.AIS ? "true" : "false");
    }
    if(state.TK) {
        fmt.add_token_pair("/TK", *state.TK ? "true" : "false");
    }
    fmt.end_dict();
    add_object(FullPDFObject{fmt.steal(), {}});
    return CapyPDF_GraphicsStateId{id};
}

rvoe<int32_t> PdfDocument::serialize_function(const FunctionType2 &func) {
    const int functiontype = 2;
    if(func.C0.index() != func.C1.index()) {
        RETERR(ColorspaceMismatch);
    }
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/FunctionType", functiontype);
    fmt.add_token_pair("/N", func.n);
    fmt.add_token("/Domain");
    fmt.begin_array();

    for(const auto d : func.domain) {
        fmt.add_token(d);
    }
    fmt.end_array();

    fmt.add_token("/C0");
    fmt.begin_array();
    color2numbers(fmt, func.C0);
    fmt.end_array();
    fmt.add_token("/C1");
    fmt.begin_array();
    color2numbers(fmt, func.C1);
    fmt.end_array();
    fmt.end_dict();

    return add_object(FullPDFObject{fmt.steal(), {}});
}

rvoe<int32_t> PdfDocument::serialize_function(const FunctionType3 &func) {
    const int functiontype = 3;
    if(!func.functions.size()) {
        RETERR(EmptyFunctionList);
    }
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/FunctionType", functiontype);

    fmt.add_token("/Domain");
    fmt.begin_array();
    for(const auto d : func.domain) {
        fmt.add_token(d);
    }
    fmt.end_array();

    fmt.add_token("/Functions");
    fmt.begin_array();
    for(const auto f : func.functions) {
        fmt.add_object_ref(functions.at(f.id).object_number);
    }
    fmt.end_array();

    fmt.add_token("/Bounds");
    fmt.begin_array();
    for(const auto b : func.bounds) {
        fmt.add_token(b);
    }
    fmt.end_array();

    fmt.add_token("/Encode");
    fmt.begin_array();
    for(const auto e : func.encode) {
        fmt.add_token(e);
    }
    fmt.end_array();

    fmt.end_dict();

    return add_object(FullPDFObject{fmt.steal(), {}});
}

rvoe<int32_t> PdfDocument::serialize_function(const FunctionType4 &func) {
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/FunctionType", 4);
    fmt.add_token("/Domain");
    fmt.begin_array();
    for(const auto d : func.domain) {
        fmt.add_token(d);
    }
    fmt.end_array();
    fmt.add_token("/Range");
    fmt.begin_array();
    for(const auto r : func.range) {
        fmt.add_token(r);
    }
    fmt.end_array();
    return add_object(DeflatePDFObject{std::move(fmt), RawData{func.code}});
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

rvoe<int32_t> PdfDocument::serialize_shading(const PdfShading &shade) {
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

rvoe<int32_t> PdfDocument::serialize_shading(const ShadingType2 &shade) {
    const int shadingtype = 2;
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/ShadingType", shadingtype);
    fmt.add_token_pair("/ColorSpace", colorspace_names.at((int)shade.colorspace));
    fmt.add_token("/Coords");
    fmt.begin_array();
    fmt.add_token(shade.x0);
    fmt.add_token(shade.y0);
    fmt.add_token(shade.x1);
    fmt.add_token(shade.y1);
    fmt.end_array();
    fmt.add_token("/Function");
    fmt.add_object_ref(functions.at(shade.function.id).object_number);
    if(shade.extend) {
        fmt.add_token("/Extend");
        fmt.begin_array();
        fmt.add_token(shade.extend->starting ? "true" : "false");
        fmt.add_token(shade.extend->ending ? "true" : "false");
        fmt.end_array();
    }
    if(shade.domain) {
        fmt.add_token("/Domain");
        fmt.begin_array();
        fmt.add_token(shade.domain->starting);
        fmt.add_token(shade.domain->ending);
        fmt.end_array();
    }
    fmt.end_dict();
    return add_object(FullPDFObject{fmt.steal(), {}});
}

rvoe<int32_t> PdfDocument::serialize_shading(const ShadingType3 &shade) {
    const int shadingtype = 3;
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/ShadingType", shadingtype);
    fmt.add_token_pair("/ColorSpace", colorspace_names.at((int)shade.colorspace));
    fmt.add_token("/Coords");
    fmt.begin_array();
    fmt.add_token(shade.x0);
    fmt.add_token(shade.y0);
    fmt.add_token(shade.r0);
    fmt.add_token(shade.x1);
    fmt.add_token(shade.y1);
    fmt.add_token(shade.r1);
    fmt.end_array();
    fmt.add_token("/Function");
    fmt.add_object_ref(functions.at(shade.function.id).object_number);
    if(shade.extend) {
        fmt.add_token("/Extend");
        fmt.begin_array();
        fmt.add_token(shade.extend->starting ? "true" : "false");
        fmt.add_token(shade.extend->ending ? "true" : "false");
        fmt.end_array();
    }
    if(shade.domain) {
        fmt.add_token("/Domain");
        fmt.begin_array();
        fmt.add_token(shade.domain->starting);
        fmt.add_token(shade.domain->ending);
        fmt.end_array();
    }
    fmt.end_dict();
    return add_object(FullPDFObject{fmt.steal(), {}});
}

rvoe<int32_t> PdfDocument::serialize_shading(const ShadingType4 &shade) {
    const int shadingtype = 4;
    ERC(serialized, serialize_shade4(shade));
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/ShadingType", shadingtype);
    fmt.add_token_pair("/ColorSpace", colorspace_names.at((int)shade.colorspace));
    fmt.add_token_pair("/BitsPerCoordinate", 32);
    fmt.add_token_pair("/BitsPerComponent", 16);
    fmt.add_token_pair("/BitsPerFlag", 8);
    fmt.add_token_pair("/Length", serialized.size());
    fmt.add_token("/Decode");
    fmt.begin_array(2);
    fmt.add_token(shade.minx);
    fmt.add_token(shade.maxx);
    fmt.add_token(shade.miny);
    fmt.add_token(shade.maxy);
    if(shade.colorspace == CAPY_DEVICE_CS_RGB) {
        fmt.add_token_pair("0", "1");
        fmt.add_token_pair("0", "1");
        fmt.add_token_pair("0", "1");
    } else if(shade.colorspace == CAPY_DEVICE_CS_GRAY) {
        fmt.add_token_pair("0", "1");
    } else if(shade.colorspace == CAPY_DEVICE_CS_CMYK) {
        fmt.add_token_pair("0", "1");
        fmt.add_token_pair("0", "1");
        fmt.add_token_pair("0", "1");
        fmt.add_token_pair("0", "1");
    } else {
        fprintf(stderr, "Color space not supported yet.\n");
        std::abort();
    }
    fmt.end_array();
    return add_object(DeflatePDFObject{std::move(fmt), RawData(std::move(serialized))});
}

rvoe<int32_t> PdfDocument::serialize_shading(const ShadingType6 &shade) {
    const int shadingtype = 6;
    ERC(serialized, serialize_shade6(shade));
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/ShadingType", shadingtype);
    fmt.add_token_pair("/ColorSpace", colorspace_names.at((int)shade.colorspace));
    fmt.add_token_pair("/BitsPerCoordinate", 32);
    fmt.add_token_pair("/BitsPerComponent", 16);
    fmt.add_token_pair("/BitsPerFlag", 8);
    fmt.add_token_pair("/Length", serialized.size());
    fmt.add_token("/Decode");
    fmt.begin_array(2);
    fmt.add_token(shade.minx);
    fmt.add_token(shade.maxx);
    fmt.add_token(shade.miny);
    fmt.add_token(shade.maxy);
    if(shade.colorspace == CAPY_DEVICE_CS_RGB) {
        fmt.add_token_pair("0", "1");
        fmt.add_token_pair("0", "1");
        fmt.add_token_pair("0", "1");
    } else if(shade.colorspace == CAPY_DEVICE_CS_GRAY) {
        fmt.add_token_pair("0", "1");
    } else if(shade.colorspace == CAPY_DEVICE_CS_CMYK) {
        fmt.add_token_pair("0", "1");
        fmt.add_token_pair("0", "1");
        fmt.add_token_pair("0", "1");
        fmt.add_token_pair("0", "1");
    } else {
        fprintf(stderr, "Color space not supported yet.\n");
        std::abort();
    }
    fmt.end_array();
    return add_object(DeflatePDFObject{std::move(fmt), RawData(std::move(serialized))});
}

rvoe<CapyPDF_ShadingId> PdfDocument::add_shading(PdfShading sh) {
    ERC(pdf_obj, serialize_shading(sh));
    shadings.emplace_back(ShadingInfo{std::move(sh), pdf_obj});
    return CapyPDF_ShadingId{(int32_t)shadings.size() - 1};
}

rvoe<CapyPDF_PatternId> PdfDocument::add_shading_pattern(const ShadingPattern &shp) {
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/Pattern");
    fmt.add_token_pair("/PatternType", "2");
    fmt.add_token("/Shading");
    fmt.add_object_ref(shadings.at(shp.sid.id).object_number);
    if(shp.m) {
        fmt.add_token("/Matrix");
        fmt.begin_array();
        fmt.add_token(shp.m->a);
        fmt.add_token(shp.m->b);
        fmt.add_token(shp.m->c);
        fmt.add_token(shp.m->d);
        fmt.add_token(shp.m->e);
        fmt.add_token(shp.m->f);
        fmt.end_array();
    }
    fmt.end_dict();
    return CapyPDF_PatternId{add_object(FullPDFObject{fmt.steal(), {}})};
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
    ERC(sc_var, ctx.serialize());
    auto &d = sc_var.get<SerializedXObject>();
    auto objid =
        add_object(DeflatePDFObject{std::move(d.dict), RawData(std::move(d.command_stream)), true});
    return CapyPDF_PatternId{objid};
}

rvoe<CapyPDF_OutlineId> PdfDocument::add_outline(const Outline &o) {
    if(o.title.empty()) {
        RETERR(EmptyTitle);
    }
    const auto cur_id = (int32_t)outlines.items.size();
    const auto par_id = o.parent ? o.parent.value().id : -1;
    outlines.parent.insert(cur_id, par_id);
    outlines.children[par_id].push_back(cur_id);
    outlines.items.emplace_back(o);
    return CapyPDF_OutlineId{cur_id};
}

rvoe<CapyPDF_FormWidgetId> PdfDocument::create_form_checkbox(PdfBox loc,
                                                             CapyPDF_FormXObjectId onstate,
                                                             CapyPDF_FormXObjectId offstate,
                                                             pystd2025::CStringView partial_name) {
    CHECK_INDEXNESS_V(onstate.id, form_xobjects);
    CHECK_INDEXNESS_V(offstate.id, form_xobjects);
    DelayedCheckboxWidgetAnnotation formobj{
        {(int32_t)form_widgets.size()}, loc, onstate, offstate, pystd2025::CString{partial_name}};
    auto obj_id = add_object(std::move(formobj));
    form_widgets.push_back(obj_id);
    return CapyPDF_FormWidgetId{(int32_t)form_widgets.size() - 1};
}

rvoe<CapyPDF_EmbeddedFileId> PdfDocument::embed_file(EmbeddedFile &ef) {
    for(const auto &file : embedded_files) {
        if(file.ef.pdfname == ef.pdfname) {
            RETERR(DuplicateName);
        }
    }
    ERC(contents, load_file_as_bytes(ef.path));
    int32_t fileobj_id;
    {
        ObjectFormatter fmt;
        fmt.begin_dict();
        fmt.add_token_pair("/Type", "/EmbeddedFile");
        fmt.add_token_pair("/Length", contents.size());
        if(!ef.subtype.empty()) {
            auto quoted = pdfname_quote(ef.subtype.sv());
            fmt.add_token_pair("/Subtype", quoted);
        }
        fmt.end_dict();
        fileobj_id = add_object(FullPDFObject{fmt.steal(), RawData(std::move(contents))});
    }

    {
        ObjectFormatter fmt;
        fmt.begin_dict();
        fmt.add_token_pair("/Type", "/Filespec");
        fmt.add_token_pair("/F", pdfstring_quote(ef.pdfname.sv()));
        fmt.add_token("/EF");
        fmt.begin_dict();
        fmt.add_token("/F");
        fmt.add_object_ref(fileobj_id);
        fmt.end_dict();
        fmt.end_dict();
        auto filespec_id = add_object(FullPDFObject{fmt.steal(), {}});
        embedded_files.emplace_back(EmbeddedFileObject{ef, filespec_id, fileobj_id});
    }
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
PdfDocument::add_structure_item(CapyPDF_Structure_Type stype,
                                pystd2025::Optional<CapyPDF_StructureItemId> parent,
                                pystd2025::Optional<StructItemExtraData> extra) {
    if(parent) {
        CHECK_INDEXNESS_V(parent->id, structure_items);
    }
    auto stritem_id = (int32_t)structure_items.size();
    auto obj_id = add_object(DelayedStructItem{stritem_id});
    structure_items.push_back(StructItem{
        obj_id, pystd2025::move(stype), parent, extra ? extra.value() : StructItemExtraData()});
    return CapyPDF_StructureItemId{(int32_t)structure_items.size() - 1};
}

rvoe<CapyPDF_StructureItemId>
PdfDocument::add_structure_item(CapyPDF_RoleId role,
                                pystd2025::Optional<CapyPDF_StructureItemId> parent,
                                pystd2025::Optional<StructItemExtraData> extra) {
    if(parent) {
        CHECK_INDEXNESS_V(parent->id, structure_items);
    }
    auto stritem_id = (int32_t)structure_items.size();
    auto obj_id = add_object(DelayedStructItem{stritem_id});
    structure_items.push_back(
        StructItem{obj_id, role, parent, extra ? extra.value() : StructItemExtraData()});
    return CapyPDF_StructureItemId{(int32_t)structure_items.size() - 1};
}

rvoe<CapyPDF_OptionalContentGroupId>
PdfDocument::add_optional_content_group(const OptionalContentGroup &g) {
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/OCG");
    fmt.add_token_pair("/Name", pdfstring_quote(g.name));
    fmt.end_dict();
    auto id = add_object(FullPDFObject{fmt.steal(), {}});
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
    ERC(sc_var, ctx.serialize());
    auto &d = sc_var.get<SerializedXObject>();
    auto objid =
        add_object(DeflatePDFObject{std::move(d.dict), RawData(std::move(d.command_stream)), true});
    transparency_groups.push_back(objid);
    return CapyPDF_TransparencyGroupId{(int32_t)transparency_groups.size() - 1};
}

rvoe<CapyPDF_SoftMaskId> PdfDocument::add_soft_mask(const SoftMask &sm) {
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/Mask");
    fmt.add_token_pair("/S", sm.S == CAPY_SOFT_MASK_ALPHA ? "/Alpha" : "/Luminosity");
    fmt.add_token("/G");
    fmt.add_object_ref(transparency_groups.at(sm.G.id));
    fmt.end_dict();
    auto id = add_object(FullPDFObject{fmt.steal(), {}});
    soft_masks.push_back(id);
    return CapyPDF_SoftMaskId{(int32_t)soft_masks.size() - 1};
}

pystd2025::Optional<double>
PdfDocument::glyph_advance(CapyPDF_FontId fid, double pointsize, uint32_t codepoint) const {
    FT_Face face = fonts.at(fid.id).fontdata.face.get();
    FT_Set_Char_Size(face, 0, (FT_F26Dot6)(pointsize * 64), 300, 300);
    if(FT_Load_Char(face, codepoint, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0) {
        return {};
    }
    const auto font_unit_advance = face->glyph->metrics.horiAdvance;
    return (font_unit_advance / 64.0) / 300.0 * 72.0;
}

rvoe<CapyPDF_FontId>
PdfDocument::load_font(FT_Library ft, const pystd2025::Path &fname, FontProperties props) {
    FT_Face face;
    TtfFont ttf{pystd2025::unique_ptr<FT_FaceRec_, FontCloser>{nullptr}, {}};
    auto error = FT_New_Face(ft, fname.c_str(), props.subfont, &face);
    if(error) {
        // By default Freetype is compiled without
        // error strings. Yay!
        auto *ft_message = FT_Error_String(error);
        if(ft) {
            fprintf(
                stderr, "Freetype could not open font file %s:\n%s\n", fname.c_str(), ft_message);
        } else {
            fprintf(
                stderr,
                "Freetype failed to open font %s, error code %d (FT error strings not available).",
                fname.c_str(),
                error);
        }
        RETERR(FreeTypeError);
    }
    ttf.face.reset(face);
    const char *font_format = FT_Get_Font_Format(face);
    if(!font_format) {
        RETERR(UnsupportedFormat);
    }
    ERC(fontdata, load_and_parse_font_file(fname, props));
    assert(fontdata.contains<TrueTypeFontFile>());
    ttf.fontdata = std::move(fontdata.get<TrueTypeFontFile>());

    if(!(strcmp(font_format, "TrueType") || strcmp(font_format, "CFF"))) {
        fprintf(stderr,
                "Only TrueType fonts are supported. %s "
                "is a %s font.",
                fname.c_str(),
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
                fname.c_str(),
                error);
        RETERR(UnsupportedFormat);
    }
    CapyPDF_FontId font_source_id{(int32_t)fonts.size()};
    ERC(fss, FontSubsetter::construct(fname, face, props));
    {
        pystd2025::unique_ptr<FT_FaceRec_, FontCloser> a(pystd2025::move(ttf.face));
        assert(ttf.face.get() == nullptr);
        ttf.face = pystd2025::move(a);
    }
    fonts.emplace_back(FontThingy{std::move(ttf), std::move(fss)});

    const int32_t subset_num = 0;
    auto subfont_data_obj = add_object(DelayedSubsetFontData{font_source_id, subset_num});
    auto subfont_descriptor_obj =
        add_object(DelayedSubsetFontDescriptor{font_source_id, subfont_data_obj, subset_num});
    auto subfont_cmap_obj = add_object(DelayedSubsetCMap{font_source_id, subset_num});
    auto subfont_obj =
        add_object(DelayedSubsetFont{font_source_id, subfont_descriptor_obj, subfont_cmap_obj});
    auto cid_dict_obj = add_object(DelayedCIDDictionary{font_source_id, subfont_descriptor_obj});

    CapyPDF_FontId fid{(int32_t)fonts.size() - 1};
    font_objects.push_back(FontPDFObjects{
        subfont_data_obj, subfont_descriptor_obj, subfont_obj, cid_dict_obj, fonts.size() - 1});
    return fid;
}

rvoe<NoReturnValue> PdfDocument::validate_format(const RawPixelImage &ri) const {
    // Check that the image has the correct format.
    if(docprops.subtype.contains<CapyPDF_PDFX_Type>()) {
        if(ri.md.cs == CAPY_IMAGE_CS_RGB) {
            // Later versions of PDFX permit rgb images with ICC colors, but let's start simple.
            RETERR(ImageFormatNotPermitted);
        }
    }
    RETOK;
}

} // namespace capypdf::internal
