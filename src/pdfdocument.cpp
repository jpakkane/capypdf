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
#include <algorithm>
#include <fmt/core.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include FT_OPENTYPE_VALIDATE_H

namespace A4PDF {

namespace {

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

std::unordered_map<int32_t, std::vector<int32_t>>
compute_children(const std::vector<A4PDF::Outline> &outlines) {
    std::unordered_map<int32_t, std::vector<int32_t>> otree;
    int32_t id = 0;
    for(const auto &o : outlines) {
        const int32_t parent_ind = o.parent ? o.parent->id : -1;
        otree[parent_ind].push_back(id);
        ++id;
    }
    return otree;
}

} // namespace

const std::array<const char *, 4> rendering_intent_names{
    "RelativeColorimetric",
    "AbsoluteColorimetric",
    "Saturation",
    "Perceptual",
};

PdfDocument::PdfDocument(const PdfGenerationData &d, PdfColorConverter cm)
    : opts{d}, cm{std::move(cm)} {
    // PDF uses 1-based indexing so add a dummy thing in this vector
    // to make PDF and vector indices are the same.
    document_objects.emplace_back(DummyIndexZero{});
    generate_info_object();
    if(d.output_colorspace == A4PDF_DEVICE_CMYK) {
        create_separation("All", DeviceCMYKColor{1.0, 1.0, 1.0, 1.0});
    }
    switch(d.output_colorspace) {
    case A4PDF_DEVICE_RGB:
        if(!cm.get_rgb().empty()) {
            output_profile_object =
                icc_profiles.at(store_icc_profile(cm.get_rgb(), 3).id).object_num;
        }
        break;
    case A4PDF_DEVICE_GRAY:
        if(!cm.get_gray().empty()) {
            output_profile_object =
                icc_profiles.at(store_icc_profile(cm.get_gray(), 1).id).object_num;
        }
        break;
    case A4PDF_DEVICE_CMYK:
        output_profile_object = icc_profiles.at(store_icc_profile(cm.get_cmyk(), 4).id).object_num;
        break;
    }
    document_objects.push_back(DelayedPages{});
    pages_object = document_objects.size() - 1;
}

rvoe<NoReturnValue>
PdfDocument::add_page(std::string resource_data,
                      std::string page_data,
                      const std::unordered_set<A4PDF_AnnotationId> &annotations) {
    for(const auto &a : annotations) {
        if(form_use.find(a) != form_use.cend()) {
            RETERR(FormWidgetReuse);
        }
    }
    const auto resource_num = add_object(FullPDFObject{std::move(resource_data), ""});
    const auto commands_num = add_object(FullPDFObject{std::move(page_data), ""});
    const auto page_num = add_object(DelayedPage{(int32_t)pages.size()});
    for(const auto &a : annotations) {
        form_use[a] = page_num;
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

rvoe<NoReturnValue> PdfDocument::write_delayed_page(int32_t page_num) {
    std::string buf;

    auto buf_append = std::back_inserter(buf);
    auto &p = pages.at(page_num);
    fmt::format_to(buf_append,
                   R"(<<
  /Type /Page
  /Parent {} 0 R
)",
                   pages_object);
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
>>
)",
                   p.commands_obj_num,
                   p.resource_obj_num);

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

rvoe<NoReturnValue> PdfDocument::create_catalog() {
    std::string buf;
    auto app = std::back_inserter(buf);
    std::string outline;
    if(!outlines.empty()) {
        ERC(outlines, create_outlines());
        outline = fmt::format("  /Outlines {} 0 R\n", outlines);
    }
    fmt::format_to(app,
                   R"(<<
  /Type /Catalog
  /Pages {} 0 R
{}
)",
                   pages_object,
                   outline);
    if(!form_annotations.empty()) {
        buf += R"(  /AcroForm <<
    /Fields [
)";
        for(const auto &i : form_annotations) {
            fmt::format_to(app, "      {} 0 R\n", annotations.at(i).id);
        }
        buf += "      ]\n  >>\n";
    }
    buf += "  /NeedAppearances true\n>>\n";
    add_object(FullPDFObject{buf, ""});
    return NoReturnValue{};
}

rvoe<int32_t> PdfDocument::create_outlines() {
    const auto otree = compute_children(outlines);
    ERC(limits, write_outline_tree(otree, -1));
    std::string buf = fmt::format(R"(<<
  /Type /Outlines
  /First {} 0 R
  /Last {} 0 R
>>
)",
                                  limits.first,
                                  limits.last);
    // FIXME: add output intents here. PDF spec 14.11.5
    return add_object(FullPDFObject{std::move(buf), ""});
}

rvoe<OutlineLimits>
PdfDocument::write_outline_tree(const std::unordered_map<int32_t, std::vector<int32_t>> &otree,
                                int32_t node_id) {
    std::optional<int32_t> first;
    std::optional<int32_t> last;
    OutlineLimits limits;
    std::string buf;
    auto app = std::back_inserter(buf);
    assert(otree.find(node_id) != otree.end());
    const auto &current_nodes = otree.at(node_id);
    std::vector<std::optional<OutlineLimits>> child_limits;
    child_limits.reserve(current_nodes.size());

    // Serialize _all_ children before _any_ parents.
    for(size_t i = 0; i < current_nodes.size(); ++i) {
        std::optional<OutlineLimits> cur_limit;
        auto it = otree.find(current_nodes[i]);
        if(it != otree.end()) {
            ERC(limit, write_outline_tree(otree, current_nodes[i]));
            cur_limit = limit;
        }
        child_limits.emplace_back(std::move(cur_limit));
    }
    assert(child_limits.size() == current_nodes.size());
    for(size_t i = 0; i < current_nodes.size(); ++i) {
        buf.clear();
        auto &child_data = child_limits[i];
        const auto &o = outlines[current_nodes[i]];
        const int32_t current_obj_num = document_objects.size();
        ERC(titlestr, utf8_to_pdfmetastr(o.title));
        fmt::format_to(app,
                       R"(<<
  /Title {}
  /Dest [ {} 0 R /XYZ null null null]
)",
                       titlestr,
                       pages.at(o.dest.id).page_obj_num);
        if(i != 0) {
            fmt::format_to(app, "  /Prev {} 0 R\n", current_obj_num - 2);
        } else {
            first = current_obj_num;
        }
        if(i == current_nodes.size() - 1) {
            last = current_obj_num;
        } else {
            fmt::format_to(app, "  /Next {} 0 R\n", current_obj_num + 1);
        }
        if(child_data) {
            fmt::format_to(app,
                           R"(  /First {} 0 R
  /Last {} 0 R
)",
                           child_data->first,
                           child_data->last);
        }
        buf += ">>\n";
        add_object(FullPDFObject{std::move(buf), ""});
    }
    assert(first);
    assert(last);
    limits.first = *first;
    limits.last = *last;
    return limits;
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
            const auto &page = std::get<DelayedPage>(obj);
            ERCV(write_delayed_page(page.page_num));
        } else if(std::holds_alternative<DelayedCheckboxWidgetAnnotation>(obj)) {
            const auto &checkbox = std::get<DelayedCheckboxWidgetAnnotation>(obj);
            ERCV(write_checkbox_widget(i, checkbox));
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
    auto forma_id = form_annotations.at(checkbox.form_annotation_id);
    // SUPER FIXME.
    auto anno_id = A4PDF_AnnotationId{(int32_t)forma_id}; // annotations.at(forma_id);
    auto loc = form_use.find(anno_id);
    if(loc == form_use.end()) {
        std::abort();
    }

    std::string dict = fmt::format(R"(<<
  /Type /Annot
  /Subtype /Widget
  /FT /Btn
  /Rect [ {} {} {} {} ]
  /T ({})
  /AP <<
    /N <<
      /On {} 0 R
      /Off {} 0 R
    >>
  >>
  /P {} 0 R
>>
)",
                                   checkbox.rect.x,
                                   checkbox.rect.y,
                                   checkbox.rect.w,
                                   checkbox.rect.h,
                                   checkbox.T,
                                   form_xobjects.at(checkbox.on.id).xobj_num,
                                   form_xobjects.at(checkbox.off.id).xobj_num,
                                   loc->second);
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
    auto data_obj_id = add_object(DeflatePDFObject{std::move(buf), std::string{contents}});
    auto obj_id = add_object(FullPDFObject{fmt::format("[ /ICCBased {} 0 R ]\n", data_obj_id), ""});
    icc_profiles.emplace_back(IccInfo{obj_id, num_channels});
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

rvoe<A4PDF_ImageId> PdfDocument::load_image(const char *fname) {
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

rvoe<A4PDF_ImageId> PdfDocument::add_image_object(int32_t w,
                                                  int32_t h,
                                                  int32_t bits_per_component,
                                                  ColorspaceType colorspace,
                                                  std::optional<int32_t> smask_id,
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
        ERC(imobj, add_image_object(image.w, image.h, 1, A4PDF_DEVICE_GRAY, {}, *image.alpha));
        smask_id = image_info.at(imobj.id).obj;
    }
    return add_image_object(image.w, image.h, 1, A4PDF_DEVICE_GRAY, smask_id, image.pixels);
}

rvoe<A4PDF_ImageId> PdfDocument::process_rgb_image(const rgb_image &image) {
    std::optional<int32_t> smask_id;
    if(image.alpha) {
        ERC(imobj, add_image_object(image.w, image.h, 8, A4PDF_DEVICE_GRAY, {}, *image.alpha));
        smask_id = image_info.at(imobj.id).obj;
    }
    switch(opts.output_colorspace) {
    case A4PDF_DEVICE_RGB: {
        return add_image_object(image.w, image.h, 8, A4PDF_DEVICE_RGB, smask_id, image.pixels);
    }
    case A4PDF_DEVICE_GRAY: {
        std::string converted_pixels = cm.rgb_pixels_to_gray(image.pixels);
        return add_image_object(image.w, image.h, 8, A4PDF_DEVICE_RGB, smask_id, converted_pixels);
    }
    case A4PDF_DEVICE_CMYK: {
        if(cm.get_cmyk().empty()) {
            RETERR(NoCmykProfile);
        }
        ERC(converted_pixels, cm.rgb_pixels_to_cmyk(image.pixels));
        return add_image_object(image.w, image.h, 8, A4PDF_DEVICE_CMYK, smask_id, converted_pixels);
    }
    default:
        RETERR(Unreachable);
    }
}

rvoe<A4PDF_ImageId> PdfDocument::process_gray_image(const gray_image &image) {
    std::optional<int32_t> smask_id;

    // Fixme: maybe do color conversion from whatever-gray to a known gray colorspace?

    if(image.alpha) {
        ERC(imgobj, add_image_object(image.w, image.h, 8, A4PDF_DEVICE_GRAY, {}, *image.alpha));
        smask_id = image_info.at(imgobj.id).obj;
    }
    return add_image_object(image.w, image.h, 8, A4PDF_DEVICE_GRAY, smask_id, image.pixels);
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
        ERC(imobj, add_image_object(image.w, image.h, 8, A4PDF_DEVICE_GRAY, {}, *image.alpha));
        smask_id = image_info.at(imobj.id).obj;
    }
    return add_image_object(image.w, image.h, 8, cs, smask_id, image.pixels);
}

rvoe<A4PDF_ImageId> PdfDocument::embed_jpg(const char *fname) {
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
    fmt::format_to(resource_appender, ">>\n");
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
    outlines.emplace_back(Outline{std::string{title_utf8}, dest, parent});
    return OutlineId{(int32_t)outlines.size() - 1};
}

rvoe<A4PDF_AnnotationId> PdfDocument::create_form_checkbox(PdfBox loc,
                                                           A4PDF_FormXObjectId onstate,
                                                           A4PDF_FormXObjectId offstate,
                                                           std::string_view partial_name) {
    CHECK_INDEXNESS_V(onstate.id, form_xobjects);
    CHECK_INDEXNESS_V(offstate.id, form_xobjects);
    DelayedCheckboxWidgetAnnotation formobj{
        (int32_t)form_annotations.size(), loc, onstate, offstate, std::string{partial_name}};
    auto obj_id = add_object(std::move(formobj));
    annotations.push_back(A4PDF_AnnotationId{(int32_t)obj_id});
    form_annotations.push_back(annotations.size() - 1);
    return A4PDF_AnnotationId{(int32_t)form_annotations.size() - 1};
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

rvoe<A4PDF_FontId> PdfDocument::load_font(FT_Library ft, const char *fname) {
    ERC(fontdata, load_and_parse_truetype_font(fname));
    TtfFont ttf{std::unique_ptr<FT_FaceRec_, FT_Error (*)(FT_Face)>{nullptr, guarded_face_close},
                std::move(fontdata)};
    FT_Face face;
    auto error = FT_New_Face(ft, fname, 0, &face);
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
                fname,
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
                fname,
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
