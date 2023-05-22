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

#include <pdfdocument.hpp>
#include <utils.hpp>
#include <pdfdrawcontext.hpp>

#include <cassert>
#include <array>
#include <map>
#include <filesystem>
#include <algorithm>
#include <fmt/core.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include FT_OPENTYPE_VALIDATE_H

namespace A4PDF {

namespace {

std::array<const char *, 3> intentnames{"/GTS_PDFX", "/GTS_PDFA", "/ISO_PDFE"};

std::array<const char *, 12> transition_names{
    "/Split",
    "/Blinds",
    "/Box",
    "/Wipe",
    "/Dissolve",
    "/Glitter",
    "/R",
    "/Fly",
    "/Push",
    "/Cover",
    "/Uncover",
    "/Fade",
};

FT_Error guarded_face_close(FT_Face face) {
    // Freetype segfaults if you give it a null pointer.
    if(face) {
        return FT_Done_Face(face);
    }
    return 0;
}

const char PDF_header[] = "%PDF-1.7\n\xe5\xf6\xc4\xd6\n";

const std::array<const char *, 9> font_names{
    "Times-Roman",
    "Helvetica",
    "Courier",
    "Times-Roman-Bold",
    "Helvetica-Bold",
    "Courier-Bold",
    "Times-Italic",
    "Helvetica-Oblique",
    "Courier-Oblique",
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

const std::array<const char *, 3> colorspace_names{
    "/DeviceRGB",
    "/DeviceGray",
    "/DeviceCMYK",
};

void write_box(auto &appender, const char *boxname, const A4PDF::PdfBox &box) {
    fmt::format_to(appender, "  /{} [ {} {} {} {} ]\n", boxname, box.x, box.y, box.w, box.h);
}

std::string fontname2pdfname(std::string_view original) {
    std::string out;
    out.reserve(original.size());
    for(const auto c : original) {
        if(c == ' ') {
            continue;
        }
        if(c == '\\') {
            continue;
        }
        out += c;
    }
    // FIXME: might need to escape other special characters as well.
    return out;
}

std::string subsetfontname2pdfname(std::string_view original, const int32_t subset_number) {
    std::string out;
    const int bufsize = 10;
    char buf[bufsize];
    snprintf(buf, bufsize, "%06d", subset_number);
    for(int i = 0; i < 6; ++i) {
        out += 'A' + (buf[i] - '0');
    }
    out += "+";
    out += fontname2pdfname(original);
    return out;
}

rvoe<std::string> build_subset_width_array(FT_Face face,
                                           const std::vector<A4PDF::TTGlyphs> &glyphs) {
    std::string arr{"[ "};
    auto bi = std::back_inserter(arr);
    const auto load_flags = FT_LOAD_NO_SCALE | FT_LOAD_LINEAR_DESIGN | FT_LOAD_NO_HINTING;
    for(const auto glyph : glyphs) {
        const auto glyph_id = A4PDF::font_id_for_glyph(face, glyph);
        FT_Pos horiadvance = 0;
        if(glyph_id != 0) {
            auto error = FT_Load_Glyph(face, glyph_id, load_flags);
            if(error != 0) {
                RETERR(FreeTypeError);
            }
            horiadvance = face->glyph->metrics.horiAdvance;
        }
        // I don't know if this is correct or not, but it worked with all fonts I had.
        //
        // Determined via debugging empirism.
        fmt::format_to(bi, "{} ", (int32_t)(double(horiadvance) * 1000 / face->units_per_EM));
    }
    arr += "]";
    return arr;
}

std::string create_subset_cmap(const std::vector<A4PDF::TTGlyphs> &glyphs) {
    std::string buf = fmt::format(R"(/CIDInit/ProcSet findresource begin
12 dict begin
begincmap
/CIDSystemInfo<<
/Registry (Adobe)
/Ordering (UCS)
/Supplement 0
>> def
/CMapName/Adobe-Identity-UCS def
/CMapType 2 def
1 begincodespacerange
<00> <FF>
endcodespacerange
{} beginbfchar
)",
                                  glyphs.size() - 1);
    // Glyph zero is not mapped.
    auto appender = std::back_inserter(buf);
    for(size_t i = 1; i < glyphs.size(); ++i) {
        const auto &g = glyphs[i];
        uint32_t unicode_codepoint = 0;
        if(std::holds_alternative<A4PDF::RegularGlyph>(g)) {
            unicode_codepoint = std::get<A4PDF::RegularGlyph>(g).unicode_codepoint;
        }
        fmt::format_to(appender, "<{:02X}> <{:04X}>\n", i, unicode_codepoint);
    }
    buf += R"(endbfchar
endcmap
CMapName currentdict /CMap defineresource pop
end
end
)";
    return buf;
}

} // namespace

const std::array<const char *, 4> rendering_intent_names{
    "RelativeColorimetric",
    "AbsoluteColorimetric",
    "Saturation",
    "Perceptual",
};

rvoe<PdfDocument> PdfDocument::construct(const PdfGenerationData &d, PdfColorConverter cm) {
    PdfDocument newdoc(d, std::move(cm));
    ERCV(newdoc.init());
    return std::move(newdoc);
}

PdfDocument::PdfDocument(const PdfGenerationData &d, PdfColorConverter cm)
    : opts{d}, cm{std::move(cm)} {}

rvoe<NoReturnValue> PdfDocument::init() {
    // PDF uses 1-based indexing so add a dummy thing in this vector
    // to make PDF and vector indices are the same.
    document_objects.emplace_back(DummyIndexZero{});
    generate_info_object();
    if(opts.output_colorspace == A4PDF_DEVICE_CMYK) {
        create_separation("All", DeviceCMYKColor{1.0, 1.0, 1.0, 1.0});
    }
    switch(opts.output_colorspace) {
    case A4PDF_DEVICE_RGB:
        if(!cm.get_rgb().empty()) {
            output_profile = store_icc_profile(cm.get_rgb(), 3);
        }
        break;
    case A4PDF_DEVICE_GRAY:
        if(!cm.get_gray().empty()) {
            output_profile = store_icc_profile(cm.get_gray(), 1);
        }
        break;
    case A4PDF_DEVICE_CMYK:
        if(cm.get_cmyk().empty()) {
            RETERR(OutputProfileMissing);
        }
        output_profile = store_icc_profile(cm.get_cmyk(), 4);
        break;
    }
    page_group_object = create_page_group();
    document_objects.push_back(DelayedPages{});
    pages_object = document_objects.size() - 1;
    if(opts.subtype) {
        if(!output_profile) {
            RETERR(OutputProfileMissing);
        }
        if(opts.intent_condition_identifier.empty()) {
            RETERR(MissingIntentIdentifier);
        }
        create_output_intent();
    }
    return NoReturnValue{};
}

int32_t PdfDocument::create_page_group() {
    std::string buf = fmt::format(R"(<<
  /S /Transparency
  /CS {}
>>
)",
                                  colorspace_names.at((int)opts.output_colorspace));
    return add_object(FullPDFObject{std::move(buf), ""});
}

rvoe<NoReturnValue> PdfDocument::add_page(std::string resource_data,
                                          std::string page_data,
                                          const std::unordered_set<A4PDF_FormWidgetId> &fws,
                                          const std::unordered_set<A4PDF_AnnotationId> &annots,
                                          const std::unordered_set<A4PDF_StructureItemId> &structs,
                                          const std::optional<PageTransition> &transition) {
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
    const auto resource_num = add_object(FullPDFObject{std::move(resource_data), ""});
    const auto commands_num = add_object(FullPDFObject{std::move(page_data), ""});
    DelayedPage p;
    p.page_num = (int32_t)pages.size();
    for(const auto &a : fws) {
        p.used_form_widgets.push_back(a);
    }
    for(const auto &a : annots) {
        p.used_annotations.push_back(A4PDF_AnnotationId{a});
    }
    p.transition = transition;
    const auto page_num = add_object(std::move(p));
    for(const auto &fw : fws) {
        form_use[fw] = page_num;
    }
    for(const auto &a : annots) {
        annotation_use[a] = page_num;
    }
    for(const auto &s : structs) {
        structure_use[s] = page_num;
    }
    pages.emplace_back(PageOffsets{resource_num, commands_num, page_num});
    return NoReturnValue{};
}

void PdfDocument::add_form_xobject(std::string xobj_dict, std::string xobj_stream) {
    const auto xobj_num = add_object(FullPDFObject{std::move(xobj_dict), std::move(xobj_stream)});

    form_xobjects.emplace_back(FormXObjectInfo{xobj_num});
}

int32_t PdfDocument::add_object(ObjectType object) {
    auto object_num = (int32_t)document_objects.size();
    document_objects.push_back(std::move(object));
    return object_num;
}

SeparationId PdfDocument::create_separation(std::string_view name,
                                            const DeviceCMYKColor &fallback) {
    std::string stream = fmt::format(R"({{ dup {} mul
exch {} exch dup {} mul
exch {} mul
}}
)",
                                     fallback.c.v(),
                                     fallback.m.v(),
                                     fallback.y.v(),
                                     fallback.k.v());
    std::string buf = fmt::format(R"(<<
  /FunctionType 4
  /Domain [ 0.0 1.0 ]
  /Range [ 0.0 1.0 0.0 1.0 0.0 1.0 0.0 1.0 ]
  /Length {}
>>
)",
                                  stream.length(),
                                  stream);
    auto fn_num = add_object(FullPDFObject{buf, std::move(stream)});
    buf.clear();
    fmt::format_to(std::back_inserter(buf),
                   R"([
  /Separation
    /{}
    /DeviceCMYK
    {} 0 R
]
)",
                   name,
                   fn_num);
    separation_objects.push_back(add_object(FullPDFObject{buf, ""}));
    return SeparationId{(int32_t)separation_objects.size() - 1};
}

LabId PdfDocument::add_lab_colorspace(const LabColorSpace &lab) {
    std::string buf = fmt::format(
        R"([ /Lab
  <<
    /WhitePoint [ {} {} {} ]
    /Range [ {} {} {} {} ]
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
    return LabId{(int32_t)document_objects.size() - 1};
}

rvoe<A4PDF_IccColorSpaceId> PdfDocument::load_icc_file(const char *fname) {
    ERC(contents, load_file(fname));
    const auto iccid = find_icc_profile(contents);
    if(iccid) {
        return *iccid;
    }
    ERC(num_channels, cm.get_num_channels(contents));
    return store_icc_profile(contents, num_channels);
}

void PdfDocument::pad_subset_fonts() {
    const uint32_t SPACE = ' ';
    const uint32_t max_count = 100;

    for(auto &sf : fonts) {
        auto &subsetter = sf.subsets;
        assert(subsetter.num_subsets() > 0);
        const auto subset_id = subsetter.num_subsets() - 1;
        if(subsetter.get_subset(subset_id).size() > SPACE) {
            continue;
        }
        // Try to add glyphs until the subset has 32 elements.
        bool padding_succeeded = false;
        for(uint32_t i = 0; i < max_count; ++i) {
            if(subsetter.get_subset(subset_id).size() == SPACE) {
                padding_succeeded = true;
                break;
            }
            const auto cur_glyph_codepoint = '!' + i;
            subsetter.get_glyph_subset(cur_glyph_codepoint);
        }
        if(!padding_succeeded) {
            fprintf(stderr, "Font subset padding failed.\n");
            std::abort();
        }
        subsetter.unchecked_insert_glyph_to_last_subset(' ');
        assert(subsetter.get_subset(subset_id).size() == SPACE + 1);
    }
}

rvoe<NoReturnValue> PdfDocument::write_to_file(FILE *output_file) {
    assert(ofile == nullptr);
    ofile = output_file;
    try {
        auto rc = write_to_file_impl();
        ofile = nullptr;
        return rc;
    } catch(...) {
        ofile = nullptr;
        throw;
    }
}

rvoe<NoReturnValue> PdfDocument::write_to_file_impl() {
    ERCV(write_header());
    ERCV(create_catalog());
    pad_subset_fonts();
    ERC(object_offsets, write_objects());
    const int64_t xref_offset = ftell(ofile);
    ERCV(write_cross_reference_table(object_offsets));
    ERCV(write_trailer(xref_offset));
    return NoReturnValue{};
}

rvoe<NoReturnValue> PdfDocument::write_delayed_page(const DelayedPage &dp) {
    std::string buf;

    auto buf_append = std::back_inserter(buf);
    auto &p = pages.at(dp.page_num);
    fmt::format_to(buf_append,
                   R"(<<
  /Type /Page
  /Parent {} 0 R
  /Group {} 0 R
)",
                   pages_object,
                   page_group_object);
    write_box(buf_append, "MediaBox", opts.mediabox);

    if(opts.cropbox) {
        write_box(buf_append, "CropBox", *opts.cropbox);
    }
    if(opts.bleedbox) {
        write_box(buf_append, "BleedBox", *opts.bleedbox);
    }
    if(opts.trimbox) {
        write_box(buf_append, "TrimBox", *opts.trimbox);
    }
    if(opts.artbox) {
        write_box(buf_append, "ArtBox", *opts.artbox);
    }
    fmt::format_to(buf_append,
                   R"(  /Contents {} 0 R
  /Resources {} 0 R
)",
                   p.commands_obj_num,
                   p.resource_obj_num);

    if(!dp.used_form_widgets.empty() || !dp.used_annotations.empty()) {
        buf += "  /Annots [\n";
        for(const auto &a : dp.used_form_widgets) {
            fmt::format_to(buf_append, "    {} 0 R\n", form_widgets.at(a.id));
        }
        for(const auto &a : dp.used_annotations) {
            fmt::format_to(buf_append, "    {} 0 R\n", annotations.at(a.id));
        }
        buf += "  ]\n";
    }
    if(dp.transition) {
        const auto &t = *dp.transition;
        buf += "  /Trans <<\n";
        if(t.type) {
            fmt::format_to(buf_append, "    /S {}\n", transition_names.at((int32_t)*t.type));
        }
        if(t.duration) {
            fmt::format_to(buf_append, "    /D {}\n", *t.duration);
        }
        if(t.Dm) {
            fmt::format_to(buf_append, "    /Dm {}\n", *t.Dm ? "/H" : "/V");
        }
        if(t.Di) {
            fmt::format_to(buf_append, "    /Di {}\n", *t.Di);
        }
        if(t.M) {
            fmt::format_to(buf_append, "    /M {}\n", *t.M ? "/I" : "/O");
        }
        if(t.SS) {
            fmt::format_to(buf_append, "    /SS {}\n", *t.SS);
        }
        if(t.B) {
            fmt::format_to(buf_append, "    /B {}\n", *t.B ? "true" : "false");
        }
        buf += "  >>\n";
    }
    buf += ">>\n";

    return write_finished_object(p.page_obj_num, buf, "");
}

rvoe<NoReturnValue> PdfDocument::write_pages_root() {
    std::string buf;
    auto buf_append = std::back_inserter(buf);
    buf = fmt::format(R"(<<
  /Type /Pages
  /Kids [
  )");
    for(const auto &i : pages) {
        fmt::format_to(buf_append, "    {} 0 R\n", i.page_obj_num);
    }
    fmt::format_to(buf_append, "  ]\n  /Count {}\n>>\n", pages.size());
    return write_finished_object(pages_object, buf, "");
}

rvoe<int32_t> PdfDocument::create_name_dict() {
    assert(!embedded_files.empty());
    std::string buf = fmt::format(R"(<<
/EmbeddedFiles <<
  /Limits [ (embobj{:05}) (embojb{:05}) ]
  /Names [
)",
                                  0,
                                  embedded_files.size() - 1);
    auto app = std::back_inserter(buf);
    for(size_t i = 0; i < embedded_files.size(); ++i) {
        fmt::format_to(app, "    (embobj{:06}) {} 0 R\n", i, embedded_files[i].filespec_obj);
    }
    buf += "  ]\n>>\n";
    return add_object(FullPDFObject{std::move(buf), ""});
}

rvoe<NoReturnValue> PdfDocument::create_catalog() {
    std::string buf;
    auto app = std::back_inserter(buf);
    std::string outline;
    std::string name;
    std::string structure;

    if(!embedded_files.empty()) {
        ERC(names, create_name_dict());
        name = fmt::format("  /Names {} 0 R\n", names);
    }
    if(!outlines.items.empty()) {
        ERC(outlines, create_outlines());
        outline = fmt::format("  /Outlines {} 0 R\n", outlines);
    }
    if(!structure_items.empty()) {
        create_structure_root_dict();
        structure = fmt::format("  /StructureTreeRoot {} 0 R\n", *structure_root_object);
    }
    fmt::format_to(app,
                   R"(<<
  /Type /Catalog
  /Pages {} 0 R
)",
                   pages_object);
    if(!outline.empty()) {
        buf += outline;
    }
    if(!name.empty()) {
        buf += name;
    }
    if(!structure.empty()) {
        buf += structure;
    }
    if(output_intent_object) {
        fmt::format_to(app, "  /OutputIntents [ {} 0 R ]\n", *output_intent_object);
    }
    if(!form_use.empty()) {
        buf += R"(  /AcroForm <<
    /Fields [
)";
        for(const auto &i : form_widgets) {
            fmt::format_to(app, "      {} 0 R\n", i);
        }
        buf += "      ]\n  >>\n";
        buf += "  /NeedAppearances true\n";
    }
    buf += ">>\n";
    add_object(FullPDFObject{buf, ""});
    return NoReturnValue{};
}

void PdfDocument::create_output_intent() {
    std::string buf;
    assert(output_profile);
    assert(opts.subtype);
    buf = fmt::format(R"(<<
  /Type /OutputIntent
  /S {}
  /OutputConditionIdentifier {}
  /DestOutputProfile {} 0 R
>>
)",
                      intentnames.at((int)*opts.subtype),
                      pdfstring_quote(opts.intent_condition_identifier),
                      icc_profiles.at(output_profile->id).stream_num);
    output_intent_object = add_object(FullPDFObject{buf, ""});
}

rvoe<int32_t> PdfDocument::create_outlines() {
    int32_t first_obj_num = (int32_t)document_objects.size();
    int32_t catalog_obj_num = first_obj_num + (int32_t)outlines.items.size();
    for(int32_t cur_id = 0; cur_id < (int32_t)outlines.items.size(); ++cur_id) {
        const auto &cur_obj = outlines.items[cur_id];
        ERC(titlestr, utf8_to_pdfmetastr(cur_obj.title));
        auto parent_id = outlines.parent.at(cur_id);
        const auto &siblings = outlines.children.at(parent_id);
        std::string oitem = fmt::format(R"(<<
  /Title {}
  /Dest [ {} 0 R /XYZ null null null]
)",
                                        titlestr,
                                        pages.at(cur_obj.dest.id).page_obj_num);
        auto app = std::back_inserter(oitem);
        if(siblings.size() > 1) {
            auto loc = std::find(siblings.begin(), siblings.end(), cur_id);
            assert(loc != siblings.end());
            if(loc != siblings.begin()) {
                auto prevloc = loc;
                --prevloc;
                fmt::format_to(app, "  /Prev {} 0 R\n", first_obj_num + *prevloc);
            }
            auto nextloc = loc;
            nextloc++;
            if(nextloc != siblings.end()) {
                fmt::format_to(app, "  /Next {} 0 R\n", first_obj_num + *nextloc);
            }
        }
        auto childs = outlines.children.find(cur_id);
        if(childs != outlines.children.end()) {
            const auto &children = childs->second;
            fmt::format_to(app, "  /First {} 0 R\n", first_obj_num + children.front());
            fmt::format_to(app, "  /Last {} 0 R\n", first_obj_num + children.back());
            fmt::format_to(app, "  /Count {}\n", -(int32_t)children.size());
        }
        /*
        if(node_counts[cur_id] > 0) {
            fmt::format_to(app, "  /Count {}\n", -node_counts[cur_id]);
        }*/
        fmt::format_to(app,
                       "  /Parent {} 0 R\n>>",
                       parent_id >= 0 ? first_obj_num + parent_id : catalog_obj_num);
        add_object(FullPDFObject{std::move(oitem), ""});
    }
    const auto &top_level = outlines.children.at(-1);
    std::string buf = fmt::format(R"(<<
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
    return add_object(FullPDFObject{std::move(buf), ""});
}

void PdfDocument::create_structure_root_dict() {
    std::string buf;
    std::optional<A4PDF_StructureItemId> rootobj;

    for(int32_t i = 0; i < (int32_t)structure_items.size(); ++i) {
        if(!structure_items[i].parent) {
            rootobj = A4PDF_StructureItemId{i};
            break;
        }
        // FIXME, check that there is only one.
    }
    assert(rootobj);
    // /ParentTree
    buf = fmt::format(R"(<<
  /Type /StructTreeRoot
  /K [ {} 0 R ]
>>
)",
                      structure_items[rootobj->id].obj_id);
    structure_root_object = add_object(FullPDFObject{buf, ""});
}

rvoe<NoReturnValue>
PdfDocument::write_cross_reference_table(const std::vector<uint64_t> &object_offsets) {
    std::string buf;
    auto app = std::back_inserter(buf);
    fmt::format_to(app,
                   R"(xref
0 {}
)",
                   object_offsets.size());
    bool first = true;
    for(const auto &i : object_offsets) {
        if(first) {
            buf += "0000000000 65535 f \n"; // The end of line whitespace is significant.
            first = false;
        } else {
            fmt::format_to(app, "{:010} 00000 n \n", i);
        }
    }
    return write_bytes(buf);
}

rvoe<NoReturnValue> PdfDocument::write_trailer(int64_t xref_offset) {
    const int32_t info = 1;                           // Info object is the first printed.
    const int32_t root = document_objects.size() - 1; // Root object is the last one printed.
    std::string buf;
    fmt::format_to(std::back_inserter(buf),
                   R"(trailer
<<
  /Size {}
  /Root {} 0 R
  /Info {} 0 R
>>
startxref
{}
%%EOF
)",
                   document_objects.size(),
                   root,
                   info,
                   xref_offset);
    return write_bytes(buf);
}

rvoe<std::vector<uint64_t>> PdfDocument::write_objects() {
    std::vector<uint64_t> object_offsets;
    for(size_t i = 0; i < document_objects.size(); ++i) {
        const auto &obj = document_objects[i];
        object_offsets.push_back(ftell(ofile));
        if(std::holds_alternative<DummyIndexZero>(obj)) {
            // Skip.
        } else if(std::holds_alternative<FullPDFObject>(obj)) {
            const auto &pobj = std::get<FullPDFObject>(obj);
            write_finished_object(i, pobj.dictionary, pobj.stream);
        } else if(std::holds_alternative<DeflatePDFObject>(obj)) {
            const auto &pobj = std::get<DeflatePDFObject>(obj);
            ERC(compressed, flate_compress(pobj.stream));
            std::string dict = fmt::format("{}  /Filter /FlateDecode\n  /Length {}\n>>\n",
                                           pobj.unclosed_dictionary,
                                           compressed.size());
            write_finished_object(i, dict, compressed);
        } else if(std::holds_alternative<DelayedSubsetFontData>(obj)) {
            const auto &ssfont = std::get<DelayedSubsetFontData>(obj);
            ERCV(write_subset_font_data(i, ssfont));
        } else if(std::holds_alternative<DelayedSubsetFontDescriptor>(obj)) {
            const auto &ssfontd = std::get<DelayedSubsetFontDescriptor>(obj);
            write_subset_font_descriptor(
                i, fonts.at(ssfontd.fid.id).fontdata, ssfontd.subfont_data_obj, ssfontd.subset_num);
        } else if(std::holds_alternative<DelayedSubsetCMap>(obj)) {
            const auto &sscmap = std::get<DelayedSubsetCMap>(obj);
            write_subset_cmap(i, fonts.at(sscmap.fid.id), sscmap.subset_id);
        } else if(std::holds_alternative<DelayedSubsetFont>(obj)) {
            const auto &ssfont = std::get<DelayedSubsetFont>(obj);
            ERCV(write_subset_font(i,
                                   fonts.at(ssfont.fid.id),
                                   0,
                                   ssfont.subfont_descriptor_obj,
                                   ssfont.subfont_cmap_obj));
        } else if(std::holds_alternative<DelayedPages>(obj)) {
            // const auto &pages = std::get<DelayedPages>(obj);
            write_pages_root();
        } else if(std::holds_alternative<DelayedPage>(obj)) {
            const auto &dp = std::get<DelayedPage>(obj);
            ERCV(write_delayed_page(dp));
        } else if(std::holds_alternative<DelayedCheckboxWidgetAnnotation>(obj)) {
            const auto &checkbox = std::get<DelayedCheckboxWidgetAnnotation>(obj);
            ERCV(write_checkbox_widget(i, checkbox));
        } else if(std::holds_alternative<DelayedAnnotation>(obj)) {
            const auto &annotation = std::get<DelayedAnnotation>(obj);
            ERCV(write_annotation(i, annotation));
        } else if(std::holds_alternative<DelayedStructItem>(obj)) {
            const auto &si = std::get<DelayedStructItem>(obj);
            ERCV(write_delayed_structure_item(i, si));
        } else {
            RETERR(Unreachable);
        }
    }
    return object_offsets;
}

rvoe<NoReturnValue> PdfDocument::write_subset_font_data(int32_t object_num,
                                                        const DelayedSubsetFontData &ssfont) {
    const auto &font = fonts.at(ssfont.fid.id);
    ERC(subset_font,
        font.subsets.generate_subset(
            font.fontdata.face.get(), font.fontdata.fontdata, ssfont.subset_id));

    ERC(compressed_bytes, flate_compress(subset_font));
    std::string dictbuf = fmt::format(R"(<<
  /Length {}
  /Length1 {}
  /Filter /FlateDecode
>>
)",
                                      compressed_bytes.size(),
                                      subset_font.size());
    ERCV(write_finished_object(object_num, dictbuf, compressed_bytes));
    return NoReturnValue{};
}

void PdfDocument::write_subset_font_descriptor(int32_t object_num,
                                               const TtfFont &font,
                                               int32_t font_data_obj,
                                               int32_t subset_number) {
    auto face = font.face.get();
    const uint32_t fflags = 4;
    auto objbuf = fmt::format(R"(<<
  /Type /FontDescriptor
  /FontName /{}
  /Flags {}
  /FontBBox [ {} {} {} {} ]
  /ItalicAngle {}
  /Ascent {}
  /Descent {}
  /CapHeight {}
  /StemV {}
  /FontFile2 {} 0 R
>>
)",
                              subsetfontname2pdfname(FT_Get_Postscript_Name(face), subset_number),
                              // face->family_name,
                              fflags,
                              face->bbox.xMin,
                              face->bbox.yMin,
                              face->bbox.xMax,
                              face->bbox.yMax,
                              0,               // Cairo always sets this to zero.
                              0,               // face->ascender,
                              0,               // face->descender,
                              face->bbox.yMax, // Copying what Cairo does.
                              80,              // Cairo always sets these to 80.
                              font_data_obj);
    write_finished_object(object_num, objbuf, "");
}

void PdfDocument::write_subset_cmap(int32_t object_num,
                                    const FontThingy &font,
                                    int32_t subset_number) {
    auto cmap = create_subset_cmap(font.subsets.get_subset(subset_number));
    auto dict = fmt::format(R"(<<
  /Length {}
>>
)",
                            cmap.length());
    write_finished_object(object_num, dict, cmap);
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
        subset_glyphs.emplace_back(RegularGlyph{cur_glyph_codepoint});
    }
    if(!padding_succeeded) {
        fprintf(stderr, "Font subset padding failed.\n");
        std::abort();
    }
    subset_glyphs.emplace_back(RegularGlyph{SPACE});
    assert(subset_glyphs.size() == SPACE + 1);
}

rvoe<NoReturnValue> PdfDocument::write_subset_font(int32_t object_num,
                                                   const FontThingy &font,
                                                   int32_t subset,
                                                   int32_t font_descriptor_obj,
                                                   int32_t tounicode_obj) {
    auto face = font.fontdata.face.get();
    const std::vector<TTGlyphs> &subset_glyphs = font.subsets.get_subset(subset);
    int32_t start_char = 0;
    int32_t end_char = subset_glyphs.size() - 1;
    ERC(width_arr, build_subset_width_array(face, subset_glyphs));
    auto objbuf = fmt::format(R"(<<
  /Type /Font
  /Subtype /TrueType
  /BaseFont /{}
  /FirstChar {}
  /LastChar {}
  /Widths {}
  /FontDescriptor {} 0 R
  /ToUnicode {} 0 R
>>
)",
                              subsetfontname2pdfname(FT_Get_Postscript_Name(face), subset),
                              start_char,
                              end_char,
                              width_arr,
                              font_descriptor_obj,
                              tounicode_obj);
    ERCV(write_finished_object(object_num, objbuf, ""));
    return NoReturnValue{};
}

rvoe<NoReturnValue>
PdfDocument::write_checkbox_widget(int obj_num, const DelayedCheckboxWidgetAnnotation &checkbox) {
    auto loc = form_use.find(checkbox.widget);
    if(loc == form_use.end()) {
        std::abort();
    }

    std::string dict = fmt::format(R"(<<
  /Type /Annot
  /Subtype /Widget
  /Rect [ {} {} {} {} ]
  /FT /Btn
  /P {} 0 R
  /T {}
  /V /Off
  /MK<</CA(8)>>
  /AP <<
    /N <<
      /Yes {} 0 R
      /Off {} 0 R
    >>
  >>
  /AS /Off
>>
)",
                                   checkbox.rect.x,
                                   checkbox.rect.y,
                                   checkbox.rect.w,
                                   checkbox.rect.h,
                                   loc->second,
                                   pdfstring_quote(checkbox.T),
                                   form_xobjects.at(checkbox.on.id).xobj_num,
                                   form_xobjects.at(checkbox.off.id).xobj_num);
    ERCV(write_finished_object(obj_num, dict, ""));
    return NoReturnValue{};
}

rvoe<NoReturnValue> PdfDocument::write_annotation(int obj_num,
                                                  const DelayedAnnotation &annotation) {
    auto loc = annotation_use.find(annotation.id);
    // It is ok for an annotation not to be used.

    std::string dict = fmt::format(R"(<<
  /Type /Annot
  /Rect [ {} {} {} {} ]
)",
                                   annotation.rect.x1,
                                   annotation.rect.y1,
                                   annotation.rect.x2,
                                   annotation.rect.y2);
    auto app = std::back_inserter(dict);
    if(loc != annotation_use.end()) {
        fmt::format_to(app, "  /P {} 0 R\n", loc->second);
    }
    if(std::holds_alternative<TextAnnotation>(annotation.sub)) {
        const auto &ta = std::get<TextAnnotation>(annotation.sub);
        fmt::format_to(app,
                       R"(  /Subtype /Text
  /Contents {}
)",
                       pdfstring_quote(ta.content));
    } else if(std::holds_alternative<FileAttachmentAnnotation>(annotation.sub)) {
        auto &faa = std::get<FileAttachmentAnnotation>(annotation.sub);

        fmt::format_to(app,
                       R"(  /Subtype /FileAttachment
  /FS {} 0 R
)",
                       embedded_files[faa.fileid.id].filespec_obj);
    } else if(std::holds_alternative<UriAnnotation>(annotation.sub)) {
        auto &ua = std::get<UriAnnotation>(annotation.sub);
        auto uri_as_str = pdfstring_quote(ua.uri);
        fmt::format_to(app,
                       R"(  /Subtype /Link
  /Contents {}
  /A <<
    /S /URI
    /URI {}
  >>
)",
                       uri_as_str,
                       uri_as_str);
    } else {
        std::abort();
    }
    dict += ">>\n";
    ERCV(write_finished_object(obj_num, dict, ""));
    return NoReturnValue{};
}

rvoe<NoReturnValue> PdfDocument::write_delayed_structure_item(int obj_num,
                                                              const DelayedStructItem &dsi) {
    std::vector<A4PDF_StructureItemId> children;
    const auto &si = structure_items.at(dsi.sid.id);
    assert(structure_root_object);
    int32_t parent_object = *structure_root_object;
    if(si.parent) {
        parent_object = structure_items.at(si.parent->id).obj_id;
    }

    // Yes, this is O(n^2). I got lazy.
    for(int32_t i = 0; i < (int32_t)structure_items.size(); ++i) {
        if(structure_items[i].parent) {
            auto current_parent = structure_items.at(structure_items[i].parent->id).obj_id;
            if(current_parent == si.obj_id) {
                children.emplace_back(A4PDF_StructureItemId{i});
            }
        }
    }
    std::string dict = fmt::format(R"(<<
  /Type /StructElem
  /S /{}
  /P {} 0 R
)",
                                   si.stype,
                                   parent_object);
    auto app = std::back_inserter(dict);
    if(!children.empty()) {
        dict += "  /K [\n";
        for(const auto &c : children) {
            fmt::format_to(app, "    {} 0 R\n", structure_items.at(c.id).obj_id);
        }
        dict += "  ]\n";
    }
    dict += ">>\n";
    ERCV(write_finished_object(obj_num, dict, ""));
    return NoReturnValue{};
}

rvoe<NoReturnValue> PdfDocument::write_finished_object(int32_t object_number,
                                                       std::string_view dict_data,
                                                       std::string_view stream_data) {
    std::string buf;
    auto appender = std::back_inserter(buf);
    fmt::format_to(appender, "{} 0 obj\n", object_number);
    buf += dict_data;
    if(!stream_data.empty()) {
        if(buf.back() != '\n') {
            buf += '\n';
        }
        buf += "stream\n";
        buf += stream_data;
        if(buf.back() != '\n') {
            buf += '\n';
        }
        buf += "endstream\n";
    }
    if(buf.back() != '\n') {
        buf += '\n';
    }
    buf += "endobj\n";
    return write_bytes(buf);
}

std::optional<A4PDF_IccColorSpaceId> PdfDocument::find_icc_profile(std::string_view contents) {
    for(size_t i = 0; i < icc_profiles.size(); ++i) {
        const auto &obj = document_objects.at(icc_profiles.at(i).object_num);
        assert(std::holds_alternative<DeflatePDFObject>(obj));
        const auto &iccobj = std::get<DeflatePDFObject>(obj);
        if(iccobj.stream == contents) {
            return A4PDF_IccColorSpaceId{(int32_t)i};
        }
    }
    return {};
}

A4PDF_IccColorSpaceId PdfDocument::store_icc_profile(std::string_view contents,
                                                     int32_t num_channels) {
    auto existing = find_icc_profile(contents);
    assert(!existing);
    if(contents.empty()) {
        return A4PDF_IccColorSpaceId{-1};
    }
    std::string buf;
    fmt::format_to(std::back_inserter(buf),
                   R"(<<
  /N {}
)",
                   num_channels);
    auto stream_obj_id = add_object(DeflatePDFObject{std::move(buf), std::string{contents}});
    auto obj_id =
        add_object(FullPDFObject{fmt::format("[ /ICCBased {} 0 R ]\n", stream_obj_id), ""});
    icc_profiles.emplace_back(IccInfo{stream_obj_id, obj_id, num_channels});
    return A4PDF_IccColorSpaceId{(int32_t)icc_profiles.size() - 1};
}

rvoe<NoReturnValue> PdfDocument::write_bytes(const char *buf, size_t buf_size) {
    if(fwrite(buf, 1, buf_size, ofile) != buf_size) {
        perror(nullptr);
        RETERR(FileWriteError);
    }
    return NoReturnValue{};
}

rvoe<NoReturnValue> PdfDocument::write_header() {
    return write_bytes(PDF_header, strlen(PDF_header));
}

rvoe<NoReturnValue> PdfDocument::generate_info_object() {
    FullPDFObject obj_data;
    obj_data.dictionary = "<<\n";
    if(!opts.title.empty()) {
        obj_data.dictionary += "  /Title ";
        ERC(titlestr, utf8_to_pdfmetastr(opts.title));
        obj_data.dictionary += titlestr;
        obj_data.dictionary += "\n";
    }
    if(!opts.author.empty()) {
        obj_data.dictionary += "  /Author ";
        ERC(authorstr, utf8_to_pdfmetastr(opts.author));
        obj_data.dictionary += authorstr;
        obj_data.dictionary += "\n";
    }
    obj_data.dictionary += "  /Producer (A4PDF " A4PDF_VERSION_STR ")\n";
    obj_data.dictionary += "  /CreationDate ";
    obj_data.dictionary += current_date_string();
    obj_data.dictionary += '\n';
    obj_data.dictionary += ">>\n";
    add_object(std::move(obj_data));
    return NoReturnValue{};
}

A4PDF_FontId PdfDocument::get_builtin_font_id(A4PDF_Builtin_Fonts font) {
    auto it = builtin_fonts.find(font);
    if(it != builtin_fonts.end()) {
        return it->second;
    }
    std::string font_dict;
    fmt::format_to(std::back_inserter(font_dict),
                   R"(<<
  /Type /Font
  /Subtype /Type1
  /BaseFont /{}
>>
)",
                   font_names[font]);
    font_objects.push_back(FontInfo{-1, -1, add_object(FullPDFObject{font_dict, ""}), size_t(-1)});
    auto fontid = A4PDF_FontId{(int32_t)font_objects.size() - 1};
    builtin_fonts[font] = fontid;
    return fontid;
}

uint32_t PdfDocument::glyph_for_codepoint(FT_Face face, uint32_t ucs4) {
    assert(face);
    return FT_Get_Char_Index(face, ucs4);
}

rvoe<SubsetGlyph> PdfDocument::get_subset_glyph(A4PDF_FontId fid, uint32_t glyph) {
    SubsetGlyph fss;
    ERC(blub, fonts.at(fid.id).subsets.get_glyph_subset(glyph));
    fss.ss.fid = fid;
    if(true) {
        fss.ss.subset_id = blub.subset;
        fss.glyph_id = blub.offset;
    } else {
        fss.ss.subset_id = 0;
        if(glyph > 255) {
            fprintf(stderr, "Glyph ids larger than 255 not supported yet.\n");
            std::abort();
        }
        fss.glyph_id = glyph;
    }
    return fss;
}

rvoe<A4PDF_ImageId> PdfDocument::load_image(const std::filesystem::path &fname) {
    ERC(image, load_image_file(fname));
    if(std::holds_alternative<rgb_image>(image)) {
        return process_rgb_image(std::get<rgb_image>(image));
    } else if(std::holds_alternative<gray_image>(image)) {
        return process_gray_image(std::get<gray_image>(image));
    } else if(std::holds_alternative<mono_image>(image)) {
        return process_mono_image(std::get<mono_image>(image));
    } else if(std::holds_alternative<cmyk_image>(image)) {
        return process_cmyk_image(std::get<cmyk_image>(image));
    } else {
        RETERR(UnsupportedFormat);
    }
}

rvoe<A4PDF_ImageId> PdfDocument::load_mask_image(const std::filesystem::path &fname) {
    ERC(image, load_image_file(fname));
    if(!std::holds_alternative<mono_image>(image)) {
        RETERR(UnsupportedFormat);
    }
    auto &im = std::get<mono_image>(image);
    return add_image_object(
        im.w, im.h, 1, A4PDF_DEVICE_GRAY, std::optional<int32_t>{}, true, im.pixels);
}

rvoe<A4PDF_ImageId> PdfDocument::add_image_object(int32_t w,
                                                  int32_t h,
                                                  int32_t bits_per_component,
                                                  ColorspaceType colorspace,
                                                  std::optional<int32_t> smask_id,
                                                  bool is_mask,
                                                  std::string_view uncompressed_bytes) {
    std::string buf;
    auto app = std::back_inserter(buf);
    ERC(compressed, flate_compress(uncompressed_bytes));
    fmt::format_to(app,
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
                   compressed.size());
    // An image may only have ImageMask or ColorSpace key, not both.
    if(is_mask) {
        buf += "  /ImageMask true\n";
    } else {
        if(std::holds_alternative<A4PDF_Colorspace>(colorspace)) {
            const auto &cs = std::get<A4PDF_Colorspace>(colorspace);
            fmt::format_to(app, "  /ColorSpace {}\n", colorspace_names.at(cs));
        } else if(std::holds_alternative<int32_t>(colorspace)) {
            const auto icc_obj = std::get<int32_t>(colorspace);
            fmt::format_to(app, "  /ColorSpace {} 0 R\n", icc_obj);
        } else {
            fprintf(stderr, "Unknown colorspace.");
            std::abort();
        }
    }
    if(smask_id) {
        fmt::format_to(app, "  /SMask {} 0 R\n", smask_id.value());
    }
    buf += ">>\n";
    auto im_id = add_object(FullPDFObject{std::move(buf), std::move(compressed)});
    image_info.emplace_back(ImageInfo{{w, h}, im_id});
    return A4PDF_ImageId{(int32_t)image_info.size() - 1};
}

rvoe<A4PDF_ImageId> PdfDocument::process_mono_image(const mono_image &image) {
    std::optional<int32_t> smask_id;
    if(image.alpha) {
        ERC(imobj,
            add_image_object(image.w, image.h, 1, A4PDF_DEVICE_GRAY, {}, false, *image.alpha));
        smask_id = image_info.at(imobj.id).obj;
    }
    return add_image_object(image.w, image.h, 1, A4PDF_DEVICE_GRAY, smask_id, false, image.pixels);
}

rvoe<A4PDF_ImageId> PdfDocument::process_rgb_image(const rgb_image &image) {
    std::optional<int32_t> smask_id;
    if(image.alpha) {
        ERC(imobj,
            add_image_object(image.w, image.h, 8, A4PDF_DEVICE_GRAY, {}, false, *image.alpha));
        smask_id = image_info.at(imobj.id).obj;
    }
    switch(opts.output_colorspace) {
    case A4PDF_DEVICE_RGB: {
        return add_image_object(
            image.w, image.h, 8, A4PDF_DEVICE_RGB, smask_id, false, image.pixels);
    }
    case A4PDF_DEVICE_GRAY: {
        std::string converted_pixels = cm.rgb_pixels_to_gray(image.pixels);
        return add_image_object(
            image.w, image.h, 8, A4PDF_DEVICE_RGB, smask_id, false, converted_pixels);
    }
    case A4PDF_DEVICE_CMYK: {
        if(cm.get_cmyk().empty()) {
            RETERR(NoCmykProfile);
        }
        ERC(converted_pixels, cm.rgb_pixels_to_cmyk(image.pixels));
        return add_image_object(
            image.w, image.h, 8, A4PDF_DEVICE_CMYK, smask_id, false, converted_pixels);
    }
    default:
        RETERR(Unreachable);
    }
}

rvoe<A4PDF_ImageId> PdfDocument::process_gray_image(const gray_image &image) {
    std::optional<int32_t> smask_id;

    // Fixme: maybe do color conversion from whatever-gray to a known gray colorspace?

    if(image.alpha) {
        ERC(imgobj,
            add_image_object(image.w, image.h, 8, A4PDF_DEVICE_GRAY, {}, false, *image.alpha));
        smask_id = image_info.at(imgobj.id).obj;
    }
    return add_image_object(image.w, image.h, 8, A4PDF_DEVICE_GRAY, smask_id, false, image.pixels);
}

rvoe<A4PDF_ImageId> PdfDocument::process_cmyk_image(const cmyk_image &image) {
    std::optional<int32_t> smask_id;
    ColorspaceType cs;
    if(image.icc) {
        auto oldicc = find_icc_profile(*image.icc);
        if(oldicc) {
            cs = icc_profiles.at(oldicc->id).object_num;
        } else {
            auto icc_obj = store_icc_profile(*image.icc, 4);
            cs = icc_profiles.at(icc_obj.id).object_num;
        }
    } else {
        cs = A4PDF_DEVICE_CMYK;
    }
    if(image.alpha) {
        ERC(imobj,
            add_image_object(image.w, image.h, 8, A4PDF_DEVICE_GRAY, {}, false, *image.alpha));
        smask_id = image_info.at(imobj.id).obj;
    }
    return add_image_object(image.w, image.h, 8, cs, smask_id, false, image.pixels);
}

rvoe<A4PDF_ImageId> PdfDocument::embed_jpg(const std::filesystem::path &fname) {
    ERC(jpg, load_jpg(fname));
    std::string buf;
    fmt::format_to(std::back_inserter(buf),
                   R"(<<
  /Type /XObject
  /Subtype /Image
  /ColorSpace /DeviceRGB
  /Width {}
  /Height {}
  /BitsPerComponent 8
  /Length {}
  /Filter /DCTDecode
>>
)",
                   jpg.w,
                   jpg.h,
                   jpg.file_contents.length());
    auto im_id = add_object(FullPDFObject{std::move(buf), std::move(jpg.file_contents)});
    image_info.emplace_back(ImageInfo{{jpg.w, jpg.h}, im_id});
    return A4PDF_ImageId{(int32_t)image_info.size() - 1};
}

GstateId PdfDocument::add_graphics_state(const GraphicsState &state) {
    const int32_t id = (int32_t)document_objects.size();
    std::string buf{
        R"(<<
  /Type /ExtGState
)"};
    auto resource_appender = std::back_inserter(buf);
    if(state.blend_mode) {
        fmt::format_to(resource_appender, "  /BM /{}\n", blend_mode_names.at(*state.blend_mode));
    }
    if(state.intent) {
        fmt::format_to(resource_appender,
                       "  /RenderingIntent /{}\n",
                       rendering_intent_names.at(*state.intent));
    }
    if(state.OP) {
        fmt::format_to(resource_appender, "  /OP {}\n", *state.OP ? "true" : "false");
    }
    if(state.op) {
        fmt::format_to(resource_appender, "  /op {}\n", *state.op ? "true" : "false");
    }
    if(state.OPM) {
        fmt::format_to(resource_appender, "  /OPM {}\n", *state.OPM);
    }
    buf += ">>\n";
    add_object(FullPDFObject{std::move(buf), {}});
    return GstateId{id};
}

FunctionId PdfDocument::add_function(const FunctionType2 &func) {
    const int functiontype = 2;
    std::string buf = fmt::format(
        R"(<<
  /FunctionType {}
  /N {}
)",
        functiontype,
        func.n);
    auto resource_appender = std::back_inserter(buf);

    buf += "  /Domain [ ";
    for(const auto d : func.domain) {
        fmt::format_to(resource_appender, "{} ", d);
    }
    buf += "]\n";
    buf += "  /C0 [ ";
    for(const auto d : func.C0) {
        fmt::format_to(resource_appender, "{} ", d);
    }
    buf += "]\n";
    buf += "  /C1 [ ";
    for(const auto d : func.C1) {
        fmt::format_to(resource_appender, "{} ", d);
    }
    buf += "]\n";
    buf += ">>\n";
    add_object(FullPDFObject{std::move(buf), {}});

    return FunctionId{(int32_t)document_objects.size()};
}

ShadingId PdfDocument::add_shading(const ShadingType2 &shade) {
    const int shadingtype = 2;
    std::string buf = fmt::format(
        R"(<<
  /ShadingType {}
  /ColorSpace {}
  /Coords [ {} {} {} {} ]
  /Function {} 0 R
  /Extend [ {} {} ]
>>
)",
        shadingtype,
        colorspace_names.at((int)shade.colorspace),
        shade.x0,
        shade.y0,
        shade.x1,
        shade.y1,
        shade.function.id,
        shade.extend0 ? "true" : "false",
        shade.extend1 ? "true" : "false");
    add_object(FullPDFObject{std::move(buf), {}});

    return ShadingId{(int32_t)document_objects.size()};
}

ShadingId PdfDocument::add_shading(const ShadingType3 &shade) {
    const int shadingtype = 3;
    std::string buf = fmt::format(
        R"(<<
  /ShadingType {}
  /ColorSpace {}
  /Coords [ {} {} {} {} {} {}]
  /Function {} 0 R
  /Extend [ {} {} ]
>>
)",
        shadingtype,
        colorspace_names.at((int)shade.colorspace),
        shade.x0,
        shade.y0,
        shade.r0,
        shade.x1,
        shade.y1,
        shade.r1,
        shade.function.id,
        shade.extend0 ? "true" : "false",
        shade.extend1 ? "true" : "false");
    add_object(FullPDFObject{std::move(buf), {}});

    return ShadingId{(int32_t)document_objects.size()};
}

PatternId PdfDocument::add_pattern(std::string_view pattern_dict, std::string_view commands) {
    add_object(FullPDFObject{std::string(pattern_dict), std::string(commands)});
    return PatternId{(int32_t)document_objects.size()};
}

OutlineId PdfDocument::add_outline(std::string_view title_utf8,
                                   PageId dest,
                                   std::optional<OutlineId> parent) {
    const auto cur_id = (int32_t)outlines.items.size();
    const auto par_id = parent.value_or(OutlineId{-1}).id;
    outlines.parent[cur_id] = par_id;
    outlines.children[par_id].push_back(cur_id);
    outlines.items.emplace_back(Outline{std::string{title_utf8}, dest, parent});
    return OutlineId{cur_id};
}

rvoe<A4PDF_FormWidgetId> PdfDocument::create_form_checkbox(PdfBox loc,
                                                           A4PDF_FormXObjectId onstate,
                                                           A4PDF_FormXObjectId offstate,
                                                           std::string_view partial_name) {
    CHECK_INDEXNESS_V(onstate.id, form_xobjects);
    CHECK_INDEXNESS_V(offstate.id, form_xobjects);
    DelayedCheckboxWidgetAnnotation formobj{
        (int32_t)form_widgets.size(), loc, onstate, offstate, std::string{partial_name}};
    auto obj_id = add_object(std::move(formobj));
    form_widgets.push_back(obj_id);
    return A4PDF_FormWidgetId{(int32_t)form_widgets.size() - 1};
}

rvoe<A4PDF_EmbeddedFileId> PdfDocument::embed_file(const std::filesystem::path &fname) {
    ERC(contents, load_file(fname));
    std::filesystem::path p(fname);
    std::string dict = fmt::format(R"(<<
  /Type /EmbeddedFile
  /Length {}
>>)",
                                   contents.size());
    auto fileobj_id = add_object(FullPDFObject{std::move(dict), std::move(contents)});
    dict = fmt::format(R"(<<
  /Type /Filespec
  /F {}
  /EF << /F {} 0 R >>
>>
)",
                       pdfstring_quote(p.filename().string()),
                       fileobj_id);
    auto filespec_id = add_object(FullPDFObject{std::move(dict), ""});
    embedded_files.emplace_back(EmbeddedFileObject{filespec_id, fileobj_id});
    return A4PDF_EmbeddedFileId{(int32_t)embedded_files.size() - 1};
}

rvoe<A4PDF_AnnotationId> PdfDocument::create_annotation(PdfRectangle rect, AnnotationSubType sub) {
    if(std::holds_alternative<UriAnnotation>(sub)) {
        auto &u = std::get<UriAnnotation>(sub);
        if(!is_ascii(u.uri)) {
            RETERR(UriNotAscii);
        }
    }
    auto annot_id = (int32_t)annotations.size();
    auto obj_id = add_object(DelayedAnnotation{annot_id, rect, std::move(sub)});
    annotations.push_back((int32_t)obj_id);
    return A4PDF_AnnotationId{annot_id};
}

rvoe<A4PDF_StructureItemId>
PdfDocument::add_structure_item(std::string_view stype,
                                std::optional<A4PDF_StructureItemId> parent) {
    if(parent) {
        CHECK_INDEXNESS_V(parent->id, structure_items);
    }
    auto stritem_id = (int32_t)structure_items.size();
    auto obj_id = add_object(DelayedStructItem{stritem_id});
    structure_items.push_back(StructItem{obj_id, std::string(stype), parent});
    return A4PDF_StructureItemId{(int32_t)structure_items.size() - 1};
}

std::optional<double>
PdfDocument::glyph_advance(A4PDF_FontId fid, double pointsize, uint32_t codepoint) const {
    FT_Face face = fonts.at(fid.id).fontdata.face.get();
    FT_Set_Char_Size(face, 0, pointsize * 64, 300, 300);
    if(FT_Load_Char(face, codepoint, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) != 0) {
        return {};
    }
    const auto font_unit_advance = face->glyph->metrics.horiAdvance;
    return (font_unit_advance / 64.0) / 300.0 * 72.0;
}

rvoe<A4PDF_FontId> PdfDocument::load_font(FT_Library ft, const std::filesystem::path &fname) {
    ERC(fontdata, load_and_parse_truetype_font(fname));
    TtfFont ttf{std::unique_ptr<FT_FaceRec_, FT_Error (*)(FT_Face)>{nullptr, guarded_face_close},
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
        add_object(DelayedSubsetFontData{A4PDF_FontId{(int32_t)font_source_id}, subset_num});
    auto subfont_descriptor_obj = add_object(DelayedSubsetFontDescriptor{
        A4PDF_FontId{(int32_t)font_source_id}, subfont_data_obj, subset_num});
    auto subfont_cmap_obj =
        add_object(DelayedSubsetCMap{A4PDF_FontId{(int32_t)font_source_id}, subset_num});
    auto subfont_obj = add_object(DelayedSubsetFont{
        A4PDF_FontId{(int32_t)font_source_id}, subfont_descriptor_obj, subfont_cmap_obj});
    (void)subfont_obj;
    A4PDF_FontId fid{(int32_t)fonts.size() - 1};
    font_objects.push_back(
        FontInfo{subfont_data_obj, subfont_descriptor_obj, subfont_obj, fonts.size() - 1});
    return fid;
}

} // namespace A4PDF
