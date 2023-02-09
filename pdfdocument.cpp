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
#include <imageops.hpp>

#include <cassert>
#include <array>
#include <map>
#include <fmt/core.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include FT_OPENTYPE_VALIDATE_H

FT_Error guarded_face_close(FT_Face face) {
    // Freetype segfaults if you give it a null pointer.
    if(face) {
        return FT_Done_Face(face);
    }
    return 0;
}

namespace {

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

void write_box(auto &appender, const char *boxname, const PdfBox &box) {
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

std::string
build_width_array(FT_Face face, const int start_char, const int one_past_the_end_end_char) {
    std::string arr{"[ "};
    assert(one_past_the_end_end_char > start_char);
    arr.reserve((one_past_the_end_end_char - start_char) * 10);
    auto bi = std::back_inserter(arr);
    const auto load_flags =
        FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;
    //    static_assert(load_flags == 522);
    for(int i = start_char; i < one_past_the_end_end_char; ++i) {
        auto glyph_index = FT_Get_Char_Index(face, i);
        auto error = FT_Load_Glyph(face, glyph_index, load_flags);
        if(error != 0) {
            throw std::runtime_error(FT_Error_String(error));
        }
        fmt::format_to(bi, "{} ", face->glyph->metrics.horiAdvance);
    }
    arr += "]";
    return arr;
}

std::string build_subset_width_array(FT_Face face, const std::vector<uint32_t> &glyphs) {
    std::string arr{"[ "};
    auto bi = std::back_inserter(arr);
    const auto load_flags =
        FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;
    //    static_assert(load_flags == 522);
    for(const auto glyph : glyphs) {
        if(glyph == 0) {
            fmt::format_to(bi, "{} ", 0);
        } else {
            auto glyph_index = FT_Get_Char_Index(face, glyph);
            auto error = FT_Load_Glyph(face, glyph_index, load_flags);
            if(error != 0) {
                throw std::runtime_error(FT_Error_String(error));
            }
            fmt::format_to(bi, "{} ", face->glyph->metrics.horiAdvance);
        }
    }
    arr += "]";
    return arr;
}

std::map<uint32_t, uint32_t> build_cmap_entries(FT_Face face) {
    const int first_id = 1;
    const int last_id = 1024;
    std::map<uint32_t, uint32_t> glyph_mapping;
    for(uint32_t i = first_id; i < last_id; ++i) {
        auto glyph_id = i != 0 ? FT_Get_Char_Index(face, i) : 0;
        if(glyph_id != i) {
            glyph_mapping[glyph_id] = i;
        }
    }
    return glyph_mapping;
}

std::string create_cmap(FT_Face face) {
    const auto mapping = build_cmap_entries(face);
    std::string cmap{R"(/CIDInit/ProcSet findresource begin
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
<0000> <FFFF>
endcodespacerange
)"};

    int num_entries = 0;
    std::string buf;
    auto cmap_app = std::back_inserter(cmap);
    auto buf_app = std::back_inserter(buf);
    for(const auto &[glyph_id, unicode_point] : mapping) {
        if(num_entries == 100) {
            fmt::format_to(cmap_app, "{} beginbfchar\n", num_entries);
            cmap += buf;
            buf.clear();
            num_entries = 0;
            cmap += "endbfchar\n";
        }
        ++num_entries;
        fmt::format_to(buf_app, "<{:04X}> <{:04X}>\n", glyph_id, unicode_point);
    }
    fmt::format_to(cmap_app,
                   R"({} beginbfchar
{}
endbfchar
)",
                   num_entries,
                   buf);

    cmap += R"(endcmap
CMapName currentdict /CMap defineresource pop
end
end
)";
    return cmap;
}

std::string create_subset_cmap(const std::vector<uint32_t> &glyphs) {
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
        fmt::format_to(appender, "<{:02X}> <{:04X}>\n", i, glyphs[i]);
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
PdfDocument::PdfDocument(const PdfGenerationData &d)
    : opts{d}, cm{d.prof.rgb_profile_file, d.prof.gray_profile_file, d.prof.cmyk_profile_file} {
    if(d.output_colorspace == PDF_DEVICE_CMYK) {
        create_separation("All", DeviceCMYKColor{1.0, 1.0, 1.0, 1.0});
    }
    rgb_profile_obj = store_icc_profile(cm.get_rgb(), 3);
    gray_profile_obj = store_icc_profile(cm.get_gray(), 1);
    cmyk_profile_obj = store_icc_profile(cm.get_cmyk(), 4);
}

void PdfDocument::add_page(std::string_view resource_data, std::string_view page_data) {
    const auto resource_num = add_object(FullPDFObject{std::string{resource_data}, ""});
    const auto page_num = add_object(FullPDFObject{std::string{page_data}, ""});
    pages.emplace_back(PageOffsets{resource_num, page_num});
}

int32_t PdfDocument::add_object(ObjectType object) {
    auto object_num = (int32_t)document_objects.size() + 1;
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

void PdfDocument::write_to_file(FILE *output_file) {
    assert(ofile == nullptr);
    ofile = output_file;
    try {
        write_header();
        write_info();
        write_pages();
        create_catalog();
        auto object_offsets = write_objects();
        const int64_t xref_offset = ftell(ofile);
        write_cross_reference_table(object_offsets);
        write_trailer(xref_offset);
    } catch(...) {
        ofile = nullptr;
        throw;
    }
    ofile = nullptr;
}

void PdfDocument::write_pages() {
    const auto pages_obj_num = (int32_t)(document_objects.size() + pages.size() + 1);

    std::string buf;

    std::vector<int32_t> page_objects;
    auto buf_append = std::back_inserter(buf);
    for(const auto &i : pages) {
        buf.clear();
        fmt::format_to(buf_append,
                       R"(<<
  /Type /Page
  /Parent {} 0 R
)",
                       pages_obj_num);
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
                       i.commands_obj_num,
                       i.resource_obj_num);

        page_objects.push_back(add_object(FullPDFObject{buf, ""}));
    }
    buf = R"(<<
  /Type /Pages
  /Kids [
)";
    for(const auto &i : page_objects) {
        fmt::format_to(buf_append, "    {} 0 R\n", i);
    }
    fmt::format_to(buf_append, "  ]\n  /Count {}\n>>\n", page_objects.size());
    const auto actual_number = add_object(FullPDFObject{buf, ""});
    if(actual_number != pages_obj_num) {
        throw std::runtime_error("Buggy McBugFace!");
    }
}

void PdfDocument::create_catalog() {
    const int32_t pages_obj_num = (int32_t)document_objects.size();
    std::string buf;
    fmt::format_to(std::back_inserter(buf),
                   R"(<<
  /Type /Catalog
  /Pages {} 0 R
>>
)",
                   pages_obj_num);
    add_object(FullPDFObject{buf, ""});
}

void PdfDocument::write_cross_reference_table(const std::vector<uint64_t> object_offsets) {
    std::string buf;
    fmt::format_to(
        std::back_inserter(buf),
        R"(xref
0 {}
0000000000 65535 f{}
)",
        object_offsets.size() + 1,
        " "); // Qt Creator removes whitespace at the end of lines but it is significant here.
    for(const auto &i : object_offsets) {
        fmt::format_to(std::back_inserter(buf), "{:010} 00000 n \n", i);
    }
    write_bytes(buf);
}

void PdfDocument::write_trailer(int64_t xref_offset) {
    const int32_t info = 1;                       // Info object is the first printed.
    const int32_t root = document_objects.size(); // Root object is the last one printed.
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
                   document_objects.size() + 1,
                   root,
                   info,
                   xref_offset);
    write_bytes(buf);
}

std::vector<uint64_t> PdfDocument::write_objects() {
    std::vector<uint64_t> object_offsets;
    for(size_t i = 0; i < document_objects.size(); ++i) {
        const auto &obj = document_objects[i];
        object_offsets.push_back(ftell(ofile));
        const int32_t object_number = (int32_t)(i + 1);
        if(std::holds_alternative<FullPDFObject>(obj)) {
            const auto &pobj = std::get<FullPDFObject>(obj);
            write_finished_object(object_number, pobj.dictionary, pobj.stream);
        } else if(std::holds_alternative<DelayedFontData>(obj)) {
            const auto &fobj = std::get<DelayedFontData>(obj);
            write_font_file(object_number, fonts.at(fobj.font_offset).fontdata);
        } else if(std::holds_alternative<DelayedFontDescriptor>(obj)) {
            const auto &fdobj = std::get<DelayedFontDescriptor>(obj);
            write_font_descriptor(
                object_number, fonts.at(fdobj.font_offset).fontdata, fdobj.font_file_object);
        } else if(std::holds_alternative<DelayedFont>(obj)) {
            const auto &fobj = std::get<DelayedFont>(obj);
            write_font(object_number,
                       fonts.at(fobj.font_offset).fontdata,
                       fobj.to_unicode_obj,
                       fobj.font_descriptor_obj);
        } else if(std::holds_alternative<DelayedCmap>(obj)) {
            const auto &cmap = std::get<DelayedCmap>(obj);
            write_cmap(object_number, fonts.at(cmap.font_offset).fontdata);
        } else if(std::holds_alternative<DelayedSubsetFontData>(obj)) {
            const auto &ssfont = std::get<DelayedSubsetFontData>(obj);
            write_subset_font_data(object_number, ssfont);
        } else if(std::holds_alternative<DelayedSubsetFontDescriptor>(obj)) {
            const auto &ssfontd = std::get<DelayedSubsetFontDescriptor>(obj);
            write_subset_font_descriptor(object_number,
                                         fonts.at(ssfontd.fid.id).fontdata,
                                         ssfontd.subfont_data_obj,
                                         ssfontd.subset_num);
        } else if(std::holds_alternative<DelayedSubsetCMap>(obj)) {
            const auto &sscmap = std::get<DelayedSubsetCMap>(obj);
            write_subset_cmap(object_number, fonts.at(sscmap.fid.id), sscmap.subset_id);
        } else if(std::holds_alternative<DelayedSubsetFont>(obj)) {
            const auto &ssfont = std::get<DelayedSubsetFont>(obj);
            write_subset_font(object_number,
                              fonts.at(ssfont.fid.id),
                              0,
                              ssfont.subfont_descriptor_obj,
                              ssfont.subfont_cmap_obj);
        } else {
            throw std::runtime_error("Unreachable code.");
        }
    }
    return object_offsets;
}

void PdfDocument::write_subset_font_data(int32_t object_num, const DelayedSubsetFontData &ssfont) {
    const auto &font = fonts.at(ssfont.fid.id);
    std::string subset_font = font.subsets.generate_subset(
        font.fontdata.face.get(), font.fontdata.fontdata, ssfont.subset_id);

    auto compressed_bytes = flate_compress(subset_font);
    std::string dictbuf = fmt::format(R"(<<
  /Length {}
  /Length1 {}
  /Filter /FlateDecode
>>
)",
                                      compressed_bytes.size(),
                                      subset_font.size());
    write_finished_object(object_num, dictbuf, compressed_bytes);
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

void PdfDocument::write_subset_font(int32_t object_num,
                                    const FontThingy &font,
                                    int32_t subset,
                                    int32_t font_descriptor_obj,
                                    int32_t tounicode_obj) {
    auto face = font.fontdata.face.get();
    const std::vector<uint32_t> &subset_glyphs = font.subsets.get_subset(subset);
    int32_t start_char = 0;
    int32_t end_char = subset_glyphs.size() - 1;
    auto width_arr = build_subset_width_array(face, subset_glyphs);
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
    write_finished_object(object_num, objbuf, "");
}

void PdfDocument::write_font_file(int32_t object_num, const TtfFont &font) {
    // Full font, not a generated subset.
    auto compressed_bytes = flate_compress(font.fontdata);
    std::string dictbuf = fmt::format(R"(<<
  /Length {}
  /Length1 {}
  /Filter /FlateDecode
>>
)",
                                      compressed_bytes.size(),
                                      font.fontdata.size());
    write_finished_object(object_num, dictbuf, compressed_bytes);
}

void PdfDocument::write_font_descriptor(int32_t object_num,
                                        const TtfFont &font,
                                        int32_t font_file_obj) {
    auto face = font.face.get();
    const uint32_t fflags = 32;
    auto objbuf = fmt::format(R"(<<
  /Type /FontDescriptor
  /FontName /{}
  /FontFamily ({})
  /Flags {}
  /FontBBox [ {} {} {} {} ]
  /ItalicAngle {}
  /Ascent {}
  /Descent {}
  /CapHeight {}
  /StemH {}
  /StemV {}
  /FontFile2 {} 0 R
>>
)",
                              fontname2pdfname(FT_Get_Postscript_Name(face)),
                              face->family_name,
                              fflags,
                              face->bbox.xMin,
                              face->bbox.yMin,
                              face->bbox.xMax,
                              face->bbox.yMax,
                              0, // Cairo always sets this to zero.
                              face->ascender,
                              face->descender,
                              face->bbox.yMax, // Copying what Cairo does.
                              80,              // Cairo always sets these to 80.
                              80,
                              font_file_obj);
    write_finished_object(object_num, objbuf, "");
}

void PdfDocument::write_font(int32_t object_num,
                             const TtfFont &font,
                             int32_t tounicode_obj,
                             int32_t font_descriptor_obj) {
    auto face = font.face.get();
    const int start_char = 0;
    const int end_char = 0xFFFD; // Unicode replacement character.
    auto width_arr = build_width_array(face, start_char, end_char + 1);
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
                              FT_Get_Postscript_Name(face),
                              start_char,
                              end_char,
                              width_arr,
                              font_descriptor_obj,
                              tounicode_obj);
    write_finished_object(object_num, objbuf, "");
}

void PdfDocument::write_cmap(int32_t object_number, const TtfFont &font) {
    auto face = font.face.get();
    auto cmap = create_cmap(face);
    auto dict = fmt::format(R"(<<
  /Length {}
>>
)",
                            cmap.length());
    write_finished_object(object_number, dict, cmap);
}

void PdfDocument::write_finished_object(int32_t object_number,
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
    write_bytes(buf);
}

int32_t PdfDocument::store_icc_profile(std::string_view contents, int32_t num_channels) {
    if(contents.empty()) {
        return -1;
    }
    std::string compressed = flate_compress(contents);
    std::string buf;
    fmt::format_to(std::back_inserter(buf),
                   R"(<<
  /Filter /FlateDecode
  /Length {}
  /N {}
>>
)",
                   compressed.size(),
                   num_channels);
    return add_object(FullPDFObject{std::move(buf), std::move(compressed)});
}

void PdfDocument::write_bytes(const char *buf, size_t buf_size) {
    if(fwrite(buf, 1, buf_size, ofile) != buf_size) {
        throw std::runtime_error(strerror(errno));
    }
}

void PdfDocument::write_header() { write_bytes(PDF_header, strlen(PDF_header)); }

void PdfDocument::write_info() {
    FullPDFObject obj_data;
    obj_data.dictionary = "<<\n";
    if(!opts.title.empty()) {
        obj_data.dictionary += "  /Title ";
        obj_data.dictionary += utf8_to_pdfmetastr(opts.title);
        obj_data.dictionary += "\n";
    }
    if(!opts.author.empty()) {
        obj_data.dictionary += "  /Author ";
        obj_data.dictionary += utf8_to_pdfmetastr(opts.author);
        obj_data.dictionary += "\n";
    }
    obj_data.dictionary += "  /Producer (PDF Testbed generator)\n";
    obj_data.dictionary += "  /CreationDate ";
    obj_data.dictionary += current_date_string();
    obj_data.dictionary += '\n';
    obj_data.dictionary += ">>\n";
    add_object(std::move(obj_data));
}

FontId PdfDocument::get_builtin_font_id(BuiltinFonts font) {
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
    auto fontid = FontId{(int32_t)font_objects.size() - 1};
    builtin_fonts[font] = fontid;
    return fontid;
}

uint32_t PdfDocument::glyph_for_codepoint(FT_Face face, uint32_t ucs4) {
    assert(face);
    return FT_Get_Char_Index(face, ucs4);
}

SubsetGlyph PdfDocument::get_subset_glyph(FontId fid, uint32_t glyph) {
    SubsetGlyph fss;
    auto blub = fonts.at(fid.id).subsets.get_glyph_subset(glyph);
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

ImageId PdfDocument::load_image(const char *fname) {
    auto image = load_image_file(fname);
    std::string buf;
    int32_t smask_id = -1;
    if(image.alpha) {
        auto compressed = flate_compress(*image.alpha);
        fmt::format_to(std::back_inserter(buf),
                       R"(<<
  /Type /XObject
  /Subtype /Image
  /Width {}
  /Height {}
  /ColorSpace /DeviceGray
  /BitsPerComponent 8
  /Length {}
  /Filter /FlateDecode
>>
)",
                       image.w,
                       image.h,
                       compressed.size());
        smask_id = add_object(FullPDFObject{std::move(buf), std::move(compressed)});
        buf.clear();
    }
    switch(opts.output_colorspace) {
    case PDF_DEVICE_RGB: {
        const auto compressed = flate_compress(image.pixels);
        // FIXME, use ICC colorspace, if one was defined.
        fmt::format_to(std::back_inserter(buf),
                       R"(<<
  /Type /XObject
  /Subtype /Image
  /ColorSpace /DeviceRGB
  /Width {}
  /Height {}
  /BitsPerComponent 8
  /Length {}
  /Filter /FlateDecode
)",
                       image.w,
                       image.h,
                       compressed.size());
        if(smask_id >= 0) {
            fmt::format_to(std::back_inserter(buf), "  /SMask {} 0 R\n", smask_id);
        }
        buf += ">>\n";
        auto im_id = add_object(FullPDFObject{std::move(buf), std::move(compressed)});
        image_info.emplace_back(ImageInfo{{image.w, image.h}, im_id});
        break;
    }
    case PDF_DEVICE_GRAY: {
        std::string converted_pixels = cm.rgb_pixels_to_gray(image.pixels);
        const auto compressed = flate_compress(converted_pixels);
        // FIXME also ICC here.
        fmt::format_to(std::back_inserter(buf),
                       R"(<<
  /Type /XObject
  /Subtype /Image
  /ColorSpace /DeviceGray
  /Width {}
  /Height {}
  /BitsPerComponent 8
  /Length {}
  /Filter /FlateDecode
)",
                       image.w,
                       image.h,
                       compressed.size());
        if(smask_id >= 0) {
            fmt::format_to(std::back_inserter(buf), "  /SMask {} 0 R\n", smask_id);
        }
        buf += ">>\n";
        auto im_id = add_object(FullPDFObject{std::move(buf), std::move(compressed)});
        image_info.emplace_back(ImageInfo{{image.w, image.h}, im_id});
        break;
    }
    case PDF_DEVICE_CMYK: {
        if(cmyk_profile_obj == -1) {
            throw std::runtime_error("Tried to convert image to CMYK without a CMYK profile.");
        }
        std::string converted_pixels = cm.rgb_pixels_to_cmyk(image.pixels);
        const auto compressed = flate_compress(converted_pixels);
        fmt::format_to(std::back_inserter(buf),
                       R"(<<
  /Type /XObject
  /Subtype /Image
  /ColorSpace [/ICCBased {} 0 R]
  /Width {}
  /Height {}
  /BitsPerComponent 8
  /Length {}
  /Filter /FlateDecode
)",
                       cmyk_profile_obj, // FIXME, maybe this should be DeviceGray?
                       image.w,
                       image.h,
                       compressed.size());
        if(smask_id >= 0) {
            fmt::format_to(std::back_inserter(buf), "  /SMask {} 0 R\n", smask_id);
        }
        buf += ">>\n";
        auto im_id = add_object(FullPDFObject{std::move(buf), std::move(compressed)});
        image_info.emplace_back(ImageInfo{{image.w, image.h}, im_id});
        break;
    }
    default:
        throw std::runtime_error("Not implemented.");
    }
    return ImageId{(int32_t)image_info.size() - 1};
}

FontId PdfDocument::load_font(FT_Library ft, const char *fname) {
    TtfFont ttf{std::unique_ptr<FT_FaceRec_, FT_Error (*)(FT_Face)>{nullptr, guarded_face_close},
                load_file(fname)};
    FT_Face face;
    auto error = FT_New_Face(ft, fname, 0, &face);
    if(error) {
        // By default Freetype is compiled without error strings. Yay!
        throw std::runtime_error(fmt::format("Freetype error {}.", error));
    }
    ttf.face.reset(face);

    const char *font_format = FT_Get_Font_Format(face);
    if(!font_format) {
        throw std::runtime_error(fmt::format("Could not determine format of font file {}.", fname));
    }
    if(strcmp(font_format, "TrueType")) { // != 0 && strcmp(font_format, "CFF") != 0) {
        throw std::runtime_error(
            fmt::format("Only TrueType fonts are supported. {} is a {} font.", fname, font_format));
    }
    FT_Bytes base = nullptr;
    error = FT_OpenType_Validate(face, FT_VALIDATE_BASE, &base, nullptr, nullptr, nullptr, nullptr);
    if(!error) {
        throw std::runtime_error(fmt::format("Font file {} is an OpenType font. Only TrueType "
                                             "fonts are supported. Freetype error {}.",
                                             fname,
                                             error));
    }
    auto font_source_id = fonts.size();
    fonts.emplace_back(FontThingy{std::move(ttf), FontSubsetter()});

    /*
    auto font_data_obj = add_object(DelayedFontData{font_source_id});
    auto tounicode_obj = add_object(DelayedCmap{font_source_id});
    auto font_descriptor_obj = add_object(DelayedFontDescriptor{font_source_id, font_data_obj});
    auto font_obj = add_object(DelayedFont{font_source_id, tounicode_obj, font_descriptor_obj});

    font_objects.push_back(
        FontInfo{font_data_obj, font_descriptor_obj, font_obj, fonts.size() - 1});
        */
    const int32_t subset_num = 0;
    auto subfont_data_obj =
        add_object(DelayedSubsetFontData{FontId{(int32_t)font_source_id}, subset_num});
    auto subfont_descriptor_obj = add_object(
        DelayedSubsetFontDescriptor{FontId{(int32_t)font_source_id}, subfont_data_obj, subset_num});
    auto subfont_cmap_obj =
        add_object(DelayedSubsetCMap{FontId{(int32_t)font_source_id}, subset_num});
    auto subfont_obj = add_object(DelayedSubsetFont{
        FontId{(int32_t)font_source_id}, subfont_descriptor_obj, subfont_cmap_obj});
    (void)subfont_obj;
    FontId fid{(int32_t)fonts.size() - 1};
    font_objects.push_back(
        FontInfo{subfont_data_obj, subfont_descriptor_obj, subfont_obj, fonts.size() - 1});
    return fid;
}
