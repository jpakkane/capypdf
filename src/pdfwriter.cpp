// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#include <pdfwriter.hpp>
#include <utils.hpp>
#include <objectformatter.hpp>

#include <format>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include FT_OPENTYPE_VALIDATE_H

#include <cassert>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace capypdf::internal {

namespace {

const std::array<const char *, 2> PDF_header_strings = {"%PDF-1.7\n%\xe5\xf6\xc4\xd6\n",
                                                        "%PDF-2.0\n%\xe5\xf6\xc4\xd6\n"};

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

void write_rectangle(ObjectFormatter &fmt, const char *boxname, const PdfRectangle &box) {
    fmt.add_token_with_slash(boxname);
    fmt.begin_array();
    fmt.add_token(box.x1);
    fmt.add_token(box.y1);
    fmt.add_token(box.x2);
    fmt.add_token(box.y2);
    fmt.end_array();
}

void write_rectangle(auto &appender, const char *boxname, const PdfRectangle &box) {
    std::format_to(
        appender, "  /{} [ {:f} {:f} {:f} {:f} ]\n", boxname, box.x1, box.y1, box.x2, box.y2);
}

std::string create_cidfont_subset_cmap(const std::vector<TTGlyphs> &glyphs) {
    std::string buf = std::format(R"(/CIDInit /ProcSet findresource begin
12 dict begin
begincmap
/CIDSystemInfo
<< /Registry (Adobe)
   /Ordering (UCS)
   /Supplement 0
>> def
/CMapName /Adobe-Identity-UCS def
/CMapType 2 def
1 begincodespacerange
<0000> <ffff>
endcodespacerange
{} beginbfchar
)",
                                  glyphs.size() - 1);
    // Glyph zero is not mapped.
    auto appender = std::back_inserter(buf);
    for(size_t i = 1; i < glyphs.size(); ++i) {
        const auto &g = glyphs[i];
        if(std::holds_alternative<LigatureGlyph>(g)) {
            const auto &lg = std::get<LigatureGlyph>(g);
            const auto u16repr = utf8_to_pdfutf16be(lg.text, false);
            std::format_to(appender, "<{:04X}> <{}>\n", i, u16repr);
        } else {
            uint32_t unicode_codepoint = 0;
            if(std::holds_alternative<RegularGlyph>(g)) {
                unicode_codepoint = std::get<RegularGlyph>(g).unicode_codepoint;
            }
            std::format_to(appender, "<{:04X}> <{:04X}>\n", i, unicode_codepoint);
        }
    }
    buf += R"(endbfchar
endcmap
CMapName currentdict /CMap defineresource pop
end
end
)";
    return buf;
}

rvoe<std::string>
build_subset_width_array(FT_Face face, const std::vector<TTGlyphs> &glyphs, bool is_cff = false) {
    std::string arr{"[ "};
    auto bi = std::back_inserter(arr);
    const auto load_flags = FT_LOAD_NO_SCALE | FT_LOAD_LINEAR_DESIGN | FT_LOAD_NO_HINTING;
    for(const auto &glyph : glyphs) {
        const auto glyph_id = font_id_for_glyph(glyph);
        FT_Pos horiadvance = 0;
        if(glyph_id != 0) {
            auto error = FT_Load_Glyph(face, glyph_id, load_flags);
            if(error != 0) {
                RETERR(FreeTypeError);
            }
            horiadvance = face->glyph->metrics.horiAdvance;
        }
        // I don't know if these are correct or not.
        // They produce the correct results with all fonts I had.
        //
        // Determined via debugging empirism.
        std::format_to(bi, "{} ", (int32_t)(double(horiadvance) * 1000 / face->units_per_EM));
    }
    arr += "]";
    return arr;
}

void serialize_time(ObjectFormatter &fmt, const char *key, double timepoint) {
    //  /B << /S /T /T << /S /S /V {:f} >> >>
    fmt.add_token(key);
    fmt.begin_dict();
    fmt.add_token_pair("/S", "/T");
    fmt.add_token("/T");
    {
        fmt.begin_dict();
        fmt.add_token_pair("/S", "/S");
        fmt.add_token("/V");
        fmt.add_token(timepoint);
        fmt.end_dict();
    }
    fmt.end_dict();
}

} // namespace

PdfWriter::PdfWriter(PdfDocument &doc) : doc(doc) {}

rvoe<NoReturnValue> PdfWriter::write_to_file(const pystd2025::Path &ofilename) {
    if(doc.pages.size() == 0) {
        RETERR(NoPages);
    }
    if(doc.write_attempted) {
        RETERR(WritingTwice);
    }
    doc.write_attempted = true;
    auto tempfname = ofilename;
    tempfname.replace_extension(".pdf~");
    FILE *out_file = fopen(tempfname.c_str(), "wb");
    if(!out_file) {
        perror(nullptr);
        RETERR(CouldNotOpenFile);
    }
    std::unique_ptr<FILE, int (*)(FILE *)> fcloser(out_file, fclose);

    ERCV(write_to_file(out_file));
    if(fflush(out_file) != 0) {
        perror(nullptr);
        RETERR(DynamicError);
    }
    if(
#ifdef _WIN32
        _commit(fileno(out_file))
#else
        fsync(fileno(out_file))
#endif
        != 0) {

        perror(nullptr);
        RETERR(FileWriteError);
    }
    // Close the file manually to verify it worked.
    fcloser.release();
    if(fclose(out_file) != 0) {
        perror(nullptr);
        RETERR(FileWriteError);
    }

    // If we made it here, the file has been fully written and fsync'd to disk. Now replace.
    auto ec = tempfname.rename_to(ofilename);
    if(!ec) {
        RETERR(FileWriteError);
    }
    RETOK;
}

rvoe<NoReturnValue> PdfWriter::write_to_file(FILE *output_file) {
    assert(ofile == nullptr);
    ofile = output_file;
#if defined(__cpp_exceptions)
    try {
        auto rc = write_to_file_impl();
        ofile = nullptr;
        return rc;
    } catch(...) {
        ofile = nullptr;
        throw;
    }
#else
    auto rc = write_to_file_impl();
    ofile = nullptr;
    return rc;
#endif
}

rvoe<NoReturnValue> PdfWriter::write_to_file_impl() {
    ERCV(write_header());
    ERCV(doc.create_catalog());
    ERC(object_offsets, write_objects());
    const int64_t xref_offset = ftell(ofile);
    ERCV(write_cross_reference_table(object_offsets));
    ERCV(write_trailer(xref_offset));
    RETOK;
}

rvoe<NoReturnValue> PdfWriter::write_bytes(const char *buf, size_t buf_size) {
    if(fwrite(buf, 1, buf_size, ofile) != buf_size) {
        perror(nullptr);
        RETERR(FileWriteError);
    }
    RETOK;
}

rvoe<NoReturnValue> PdfWriter::write_header() {
    const auto header = PDF_header_strings.at((int)doc.docprops.version());
    return write_bytes(header, strlen(header));
}

rvoe<std::vector<uint64_t>> PdfWriter::write_objects() {
    std::vector<uint64_t> object_offsets;
    for(size_t i = 0; i < doc.document_objects.size(); ++i) {
        object_offsets.push_back(ftell(ofile));
        auto &obj = doc.document_objects.at(i);
        if(std::holds_alternative<DummyIndexZero>(obj)) {

        } else if(auto *pobj = std::get_if<FullPDFObject>(&obj)) {
            ERCV(write_finished_object(i, pobj->dictionary, pobj->stream.span()));
        } else if(auto *pobj_ = std::get_if<DeflatePDFObject>(&obj)) {
            auto &pobj = *pobj_;
            ObjectFormatter &fmt = const_cast<ObjectFormatter &>(pobj.unclosed_dictionary);
            if(pobj.leave_uncompressed_in_debug && !doc.docprops.compress_streams) {
                fmt.add_token_pair("/Length", pobj.stream.size());
                fmt.end_dict();
                ERCV(write_finished_object(i, fmt.steal(), pobj.stream.span()));
            } else {
                ERC(compressed, flate_compress(pobj.stream.sv()));
                // FIXME, not great.
                fmt.add_token_pair("/Filter", "/FlateDecode");
                fmt.add_token_pair("/Length", compressed.size());
                fmt.end_dict();
                ERCV(write_finished_object(i, fmt.steal(), compressed));
            }
        } else if(auto *ssfont = std::get_if<DelayedSubsetFontData>(&obj)) {
            ERCV(write_subset_font_data(i, *ssfont));
        } else if(auto *ssfontd = std::get_if<DelayedSubsetFontDescriptor>(&obj)) {
            ERCV(write_subset_font_descriptor(i,
                                              doc.fonts.at(ssfontd->fid.id).fontdata,
                                              ssfontd->subfont_data_obj,
                                              ssfontd->subset_num));
        } else if(auto *sscmap = std::get_if<DelayedSubsetCMap>(&obj)) {
            assert(sscmap->subset_id == 0);
            ERCV(write_subset_cmap(i, doc.fonts.at(sscmap->fid.id)));
        } else if(auto *ssfont = std::get_if<DelayedSubsetFont>(&obj)) {
            ERCV(write_subset_font(i, doc.fonts.at(ssfont->fid.id), ssfont->subfont_cmap_obj));
        } else if(auto *ciddict = std::get_if<DelayedCIDDictionary>(&obj)) {
            ERCV(write_cid_dict(i, ciddict->fid, ciddict->subfont_descriptor_obj));
        } else if(std::holds_alternative<DelayedPages>(obj)) {
            ERCV(write_pages_root());
        } else if(auto *dp = std::get_if<DelayedPage>(&obj)) {
            ERCV(write_delayed_page(*dp));
        } else if(auto *checkbox = std::get_if<DelayedCheckboxWidgetAnnotation>(&obj)) {
            ERCV(write_checkbox_widget(i, *checkbox));
        } else if(auto *anno = std::get_if<DelayedAnnotation>(&obj)) {
            ERCV(write_annotation(i, *anno));
        } else if(auto *si = std::get_if<DelayedStructItem>(&obj)) {
            ERCV(write_delayed_structure_item(i, *si));
        } else {
            fprintf(stderr, "Unreachable variant visit.\n");
            std::abort();
        }
    }
    return object_offsets;
}

rvoe<NoReturnValue>
PdfWriter::write_cross_reference_table(const std::vector<uint64_t> &object_offsets) {
    std::string buf;
    auto app = std::back_inserter(buf);
    std::format_to(app,
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
            std::format_to(app, "{:010} 00000 n \n", i);
        }
    }
    return write_bytes(buf);
}

rvoe<NoReturnValue> PdfWriter::write_trailer(int64_t xref_offset) {
    const int32_t info = 1;                               // Info object is the first printed.
    const int32_t root = doc.document_objects.size() - 1; // Root object is the last one printed.
    std::string buf;
    auto documentid = create_trailer_id();
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Size", doc.document_objects.size());
    fmt.add_token("/Root");
    fmt.add_object_ref(root);
    if(add_info_key_to_trailer()) {
        fmt.add_token("/Info");
        fmt.add_object_ref(info);
    }
    fmt.add_token("/ID");
    fmt.begin_array();
    fmt.add_token(documentid);
    fmt.add_token(documentid);
    fmt.end_array();
    fmt.end_dict();
    auto ending = std::format(R"(startxref
{}
%%EOF
)",
                              xref_offset);
    ERCV(write_bytes("trailer\n"));
    ERCV(write_bytes(fmt.steal()));
    return write_bytes(ending);
}

rvoe<NoReturnValue> PdfWriter::write_finished_object(int32_t object_number,
                                                     std::string_view dict_data,
                                                     std::span<std::byte> stream_data) {
    std::string buf;
    auto appender = std::back_inserter(buf);
    std::format_to(appender, "{} 0 obj\n", object_number);
    buf += dict_data;
    if(!stream_data.empty()) {
        if(buf.back() != '\n') {
            buf += '\n';
        }
        buf += "stream\n";
        buf += span2sv(stream_data);
        // PDF spec says that there must always be a newline before "endstream".
        // It is not counted in the /Length key in the object dictionary.
        buf += "\nendstream\n";
    }
    if(buf.back() != '\n') {
        buf += '\n';
    }
    buf += "endobj\n";
    return write_bytes(buf);
}

rvoe<NoReturnValue>
PdfWriter::write_subset_font(int32_t object_num, const FontThingy &font, int32_t tounicode_obj) {
    const int32_t subset_id = 0;
    auto face = font.fontdata.face.get();
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/Font");
    fmt.add_token_pair("/Subtype", "/Type0");
    fmt.add_token("/BaseFont");
    fmt.add_token_with_slash(subsetfontname2pdfname(FT_Get_Postscript_Name(face), subset_id));
    const int32_t ciddict_obj = object_num + 1; // FIXME
    fmt.add_token_pair("/Encoding", "/Identity-H");
    fmt.add_token("/DescendantFonts");
    fmt.begin_array();
    fmt.add_object_ref(ciddict_obj);
    fmt.end_array();
    fmt.add_token("/ToUnicode");
    fmt.add_object_ref(tounicode_obj);
    fmt.end_dict();

    ERCV(write_finished_object(object_num, fmt.steal(), {}));
    RETOK;
}

rvoe<NoReturnValue>
PdfWriter::write_cid_dict(int32_t object_num, CapyPDF_FontId fid, int32_t font_descriptor_obj) {
    int32_t subset = 0; // FIXME
    const auto &font = doc.fonts.at(fid.id);
    auto face = font.fontdata.face.get();
    ERC(width_arr,
        build_subset_width_array(
            face, font.subsets.get_subset(), font.fontdata.fontdata.in_cff_format()));
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/Font");
    fmt.add_token_pair("/Subtype",
                       font.fontdata.fontdata.in_cff_format() ? "/CIDFontType0" : "/CIDFontType2");
    fmt.add_token("/BaseFont");
    fmt.add_token_with_slash(subsetfontname2pdfname(FT_Get_Postscript_Name(face), subset));
    fmt.add_token("/CIDSystemInfo");
    fmt.begin_dict();
    fmt.add_token_pair("/Registry", "(Adobe)");
    fmt.add_token_pair("/Ordering", "(Identity)");
    fmt.add_token_pair("/Supplement", "0");
    fmt.end_dict();
    fmt.add_token("/FontDescriptor");
    fmt.add_object_ref(font_descriptor_obj);
    fmt.add_token("/W");
    {
        fmt.begin_array();
        fmt.add_token("0");
        fmt.add_token(width_arr);
        fmt.end_array();
    }
    if(!font.fontdata.fontdata.in_cff_format()) {
        fmt.add_token_pair("/CIDToGIDMap", "/Identity");
    }
    fmt.end_dict();
    ERCV(write_finished_object(object_num, fmt.steal(), {}));
    RETOK;
}

rvoe<NoReturnValue> PdfWriter::write_subset_font_data(int32_t object_num,
                                                      const DelayedSubsetFontData &ssfont) {
    const auto &font = doc.fonts.at(ssfont.fid.id);
    assert(ssfont.subset_id == 0);
    ERC(subset_font, font.subsets.generate_subset(font.fontdata.fontdata));

    ERC(compressed_bytes, flate_compress(subset_font));
    if(font.fontdata.fontdata.in_cff_format()) {
        ObjectFormatter fmt;
        fmt.begin_dict();
        fmt.add_token_pair("/Length", compressed_bytes.size());
        fmt.add_token_pair("/Filter", "/FlateDecode");
        fmt.add_token_pair("/Subtype", "/CIDFontType0C");
        fmt.end_dict();
        ERCV(write_finished_object(object_num, fmt.steal(), compressed_bytes));
    } else {
        ObjectFormatter fmt;
        fmt.begin_dict();
        fmt.add_token_pair("/Length", compressed_bytes.size());
        fmt.add_token_pair("/Length1", subset_font.size());
        fmt.add_token_pair("/Filter", "/FlateDecode");
        fmt.add_token_pair("/Subtype", "/OpenType");
        fmt.end_dict();
        ERCV(write_finished_object(object_num, fmt.steal(), compressed_bytes));
    }
    RETOK;
}

rvoe<NoReturnValue> PdfWriter::write_subset_font_descriptor(int32_t object_num,
                                                            const TtfFont &font,
                                                            int32_t font_data_obj,
                                                            int32_t subset_number) {
    auto face = font.face.get();
    const uint32_t fflags = 4;
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/FontDescriptor");
    fmt.add_token("/FontName");
    fmt.add_token_with_slash(subsetfontname2pdfname(FT_Get_Postscript_Name(face), subset_number));
    fmt.add_token_pair("/Flags", fflags);
    fmt.add_token("/FontBBox");
    fmt.begin_array();
    fmt.add_token((int)face->bbox.xMin);
    fmt.add_token((int)face->bbox.yMin);
    fmt.add_token((int)face->bbox.xMax);
    fmt.add_token((int)face->bbox.yMax);
    fmt.end_array();
    fmt.add_token_pair("/ItalicAngle", "0");                // Cairo always sets this to zero.
    fmt.add_token_pair("/Ascent", "0");                     // face->ascender
    fmt.add_token_pair("/Descent", "0");                    // face->descender
    fmt.add_token_pair("/CapHeight", (int)face->bbox.yMax); // Copying what Cairo does.
    fmt.add_token_pair("/StemV", 80);                       // Cairo always sets these to 80.
    fmt.add_token_pair("/StemH", 80);
    fmt.add_token("/FontFile3");
    fmt.add_object_ref(font_data_obj);
    fmt.end_dict();
    return write_finished_object(object_num, fmt.steal(), {});
}

rvoe<NoReturnValue> PdfWriter::write_subset_cmap(int32_t object_num, const FontThingy &font) {
    auto cmap = create_cidfont_subset_cmap(font.subsets.get_subset());
    ERC(compressed_cmap, flate_compress(cmap));
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Filter", "/FlateDecode");
    fmt.add_token_pair("/Length", compressed_cmap.size());
    fmt.end_dict();
    return write_finished_object(object_num, fmt.steal(), compressed_cmap);
}

rvoe<NoReturnValue> PdfWriter::write_pages_root() {
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/Pages");
    fmt.add_token("/Kids");
    fmt.begin_array(1);

    for(const auto &i : doc.pages) {
        fmt.add_object_ref(i.page_obj_num);
    }
    fmt.end_array();
    fmt.add_token_pair("/Count", doc.pages.size());
    fmt.end_dict();
    return write_finished_object(doc.pages_object, fmt.steal(), {});
}

rvoe<NoReturnValue> PdfWriter::write_delayed_page(const DelayedPage &dp) {
    ObjectFormatter fmt;
    auto &p = doc.pages.at(dp.page_num);
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/Page");
    fmt.add_token("/Parent");
    fmt.add_object_ref(doc.pages_object);
    fmt.add_token("/LastModified");
    fmt.add_token(current_date_string());
    if(dp.custom_props.transparency_props) {
        fmt.add_token("/Group");
        dp.custom_props.transparency_props->serialize(fmt);
    } else if(doc.docprops.default_page_properties.transparency_props) {
        fmt.add_token("/Group");
        doc.docprops.default_page_properties.transparency_props->serialize(fmt);
    }
    PageProperties current_props = doc.docprops.default_page_properties.merge_with(dp.custom_props);
    write_rectangle(fmt, "MediaBox", *current_props.mediabox);

    if(current_props.cropbox) {
        write_rectangle(fmt, "CropBox", *current_props.cropbox);
    }
    if(current_props.bleedbox) {
        write_rectangle(fmt, "BleedBox", *current_props.bleedbox);
    }
    if(current_props.trimbox) {
        write_rectangle(fmt, "TrimBox", *current_props.trimbox);
    }
    if(current_props.artbox) {
        write_rectangle(fmt, "ArtBox", *current_props.artbox);
    }
    if(dp.structparents) {
        fmt.add_token("/StructParents");
        fmt.add_token(dp.structparents.value());
    }

    if(current_props.user_unit) {
        fmt.add_token("/UserUnit");
        fmt.add_token(*current_props.user_unit);
    }

    fmt.add_token("/Contents");
    fmt.add_object_ref(p.commands_obj_num);
    fmt.add_token("/Resources");
    fmt.add_object_ref(p.resource_obj_num);

    if(!dp.used_form_widgets.empty() || !dp.used_annotations.empty()) {
        fmt.add_token("/Annots");
        fmt.begin_array(1);

        for(const auto &a : dp.used_form_widgets) {
            fmt.add_object_ref(doc.form_widgets.at(a.id));
        }
        for(const auto &a : dp.used_annotations) {
            fmt.add_object_ref(doc.annotations.at(a.id));
        }
        fmt.end_array();
    }
    if(dp.transition) {
        const auto &t = *dp.transition;
        serialize_trans(fmt, t);
    }

    if(dp.subnav_root) {
        fmt.add_token("/PresSteps");
        fmt.add_object_ref(dp.subnav_root.value());
    }
    fmt.end_dict();

    return write_finished_object(p.page_obj_num, fmt.steal(), {});
}

rvoe<NoReturnValue>
PdfWriter::write_checkbox_widget(int obj_num, const DelayedCheckboxWidgetAnnotation &checkbox) {
    auto loc = doc.form_use.lookup(checkbox.widget);
    if(!loc) {
        std::abort();
    }

    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/Annot");
    fmt.add_token_pair("/Subtype", "/Widget");
    fmt.add_token("/Rect");
    {
        fmt.begin_array();
        fmt.add_token(checkbox.rect.x);
        fmt.add_token(checkbox.rect.y);
        fmt.add_token(checkbox.rect.w);
        fmt.add_token(checkbox.rect.h);
        fmt.end_array();
    }
    fmt.add_token_pair("/FT", "/Btn");
    fmt.add_token("/P");
    fmt.add_object_ref(*loc);
    fmt.add_token("/T");
    fmt.add_token(pdfstring_quote(checkbox.T));
    fmt.add_token_pair("/V", "/Off");
    fmt.add_token_pair("/MK", "<</CA(8)>>");
    {
        fmt.add_token("/AP");
        fmt.begin_dict();
        fmt.add_token("/N");
        {
            fmt.begin_dict();
            fmt.add_token("/N");
            {
                fmt.begin_dict();
                fmt.add_token("/Yes");
                fmt.add_object_ref(doc.form_xobjects.at(checkbox.on.id).xobj_num);
                fmt.add_token("/Off");
                fmt.add_object_ref(doc.form_xobjects.at(checkbox.off.id).xobj_num);
                fmt.end_dict();
            }
            fmt.end_dict();
        }
        fmt.end_dict();
        fmt.add_token_pair("/AS", "/Off");
    }
    fmt.end_dict();
    ERCV(write_finished_object(obj_num, fmt.steal(), {}));
    RETOK;
}

rvoe<NoReturnValue> PdfWriter::write_annotation(int obj_num, const DelayedAnnotation &annotation) {
    auto *loc = doc.annotation_use.lookup(annotation.id);
    // It is ok for an annotation not to be used.

    assert(annotation.a.rect);
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/Annot");
    fmt.add_token("/Rect");
    {
        fmt.begin_array();
        fmt.add_token(annotation.a.rect->x1);
        fmt.add_token(annotation.a.rect->y1);
        fmt.add_token(annotation.a.rect->x2);
        fmt.add_token(annotation.a.rect->y2);
        fmt.end_array();
    }
    fmt.add_token("/F");
    fmt.add_token((int)annotation.a.flags);

    if(loc) {
        fmt.add_token("/P");
        fmt.add_object_ref(*loc);
    }
    if(const auto ta = std::get_if<TextAnnotation>(&annotation.a.sub)) {
        fmt.add_token_pair("/Subtype", "/Text");
        fmt.add_token("/Contents");
        fmt.add_token(utf8_to_pdfutf16be(ta->content));
    } else if(auto faa = std::get_if<FileAttachmentAnnotation>(&annotation.a.sub)) {
        fmt.add_token_pair("/Subtype", "/FileAttachment");
        fmt.add_token("/FS");
        fmt.add_object_ref(doc.get(faa->fileid).filespec_obj);
    } else if(auto linkobj = std::get_if<LinkAnnotation>(&annotation.a.sub)) {
        fmt.add_token_pair("/Subtype", "/Link");
        if(linkobj->URI) {
            assert(!linkobj->Dest);
            auto uri_as_str = pdfstring_quote(linkobj->URI->sv());
            fmt.add_token("/A");
            fmt.begin_dict();
            fmt.add_token_pair("/S", "/URI");
            fmt.add_token("/URI");
            fmt.add_token(uri_as_str);
            fmt.end_dict();
        } else if(linkobj->Dest) {
            const auto physical_page = linkobj->Dest->page;
            if(physical_page < 0 || physical_page >= (int32_t)doc.pages.size()) {
                RETERR(InvalidPageNumber);
            }
            const auto page_object_number = doc.pages.at(physical_page).page_obj_num;
            serialize_destination(fmt, linkobj->Dest.value(), page_object_number);
        }

    } else if(auto sa = std::get_if<ScreenAnnotation>(&annotation.a.sub)) {
        int32_t media_filespec = doc.get(sa->mediafile).filespec_obj;
        if(!sa->times) {
            fmt.add_token_pair("/Subtype", "/Screen");
            fmt.add_token("/A");
            {
                fmt.begin_dict();
                fmt.add_token_pair("/Type", "/Action");
                fmt.add_token_pair("/S", "/Rendition");
                fmt.add_token_pair("/OP", "0");
                fmt.add_token("/AN");
                fmt.add_object_ref(obj_num);
                fmt.add_token("/R");
                {
                    fmt.begin_dict();
                    fmt.add_token_pair("/Type", "/Rendition");
                    fmt.add_token_pair("/S", "/MR");
                    fmt.add_token("/C");
                    {
                        fmt.begin_dict();
                        fmt.add_token_pair("/Type", "/MediaClip");
                        fmt.add_token("/CT");
                        fmt.add_pdfstring(sa->mimetype);
                        fmt.add_token_pair("/S", "/MCD");
                        fmt.add_token("/D");
                        fmt.add_object_ref(media_filespec);
                        fmt.add_token("/P");
                        {
                            fmt.begin_dict();
                            fmt.add_token_pair("/TF", "(TEMPALWAYS)");
                            fmt.end_dict();
                        }
                        fmt.end_dict();
                    }
                    fmt.end_dict();
                }
                fmt.end_dict();
            }
        } else {
            // NOTE! This should work but does not. Acrobat reader will error
            // out if there are any entries in the MH dictionary, regardless whether they
            // are time or frame dictionaries.
            fmt.add_token_pair("/Subtype", "/Screen");
            fmt.add_token("/A");
            {
                fmt.begin_dict();
                fmt.add_token_pair("/Type", "/Action");
                fmt.add_token_pair("/S", "/Rendition");
                fmt.add_token_pair("/OP", "0");
                fmt.add_token("/AN");
                fmt.add_object_ref(obj_num);
                fmt.add_token("/R");
                {
                    fmt.begin_dict();
                    fmt.add_token_pair("/Type", "/Rendition");
                    fmt.add_token_pair("/S", "/MR");
                    fmt.add_token("/C");
                    {
                        fmt.begin_dict();
                        fmt.add_token_pair("/Type", "/MediaClip");
                        fmt.add_token_pair("/S", "/MCS");
                        fmt.add_token("/D");
                        {
                            fmt.begin_dict();
                            fmt.add_token_pair("/Type", "/MediaClip");
                            fmt.add_token("/CT");
                            fmt.add_pdfstring(sa->mimetype);
                            fmt.add_token_pair("/S", "/MCD");
                            fmt.add_token("/D");
                            fmt.add_object_ref(media_filespec);
                            fmt.add_token("/P");
                            {
                                fmt.begin_dict();
                                fmt.add_token_pair("/TF", "(TEMPALWAYS");
                                fmt.end_dict();
                            }
                            fmt.end_dict();
                        }
                        fmt.add_token("/MH");
                        {
                            fmt.begin_dict();
                            serialize_time(fmt, "/B", sa->times->starttime);
                            serialize_time(fmt, "/E", sa->times->endtime);
                            fmt.end_dict();
                        }
                    }
                    fmt.end_dict();
                }
                fmt.end_dict();
            }
            fmt.end_dict();
        }
    }

    else if(auto pma = std::get_if<PrintersMarkAnnotation>(&annotation.a.sub)) {
        fmt.add_token_pair("/Subtype", "/PrinterMark");
        fmt.add_token("/AP");
        fmt.begin_dict();
        fmt.add_token("/N");
        fmt.add_object_ref(doc.form_xobjects.at(pma->appearance.id).xobj_num);
        fmt.end_dict();
    } else {
        fprintf(stderr, "Unknown annotation type.\n");
        std::abort();
    }
    fmt.end_dict();
    ERCV(write_finished_object(obj_num, fmt.steal(), {}));
    RETOK;
}

rvoe<NoReturnValue> PdfWriter::write_delayed_structure_item(int obj_num,
                                                            const DelayedStructItem &dsi) {
    std::vector<CapyPDF_StructureItemId> children;
    const auto &si = doc.structure_items.at(dsi.sid.id);
    assert(doc.structure_root_object);
    int32_t parent_object = *doc.structure_root_object;
    if(si.parent) {
        parent_object = doc.structure_items.at(si.parent->id).obj_id;
    }

    // Yes, this is O(n^2). I got lazy.
    for(int32_t i = 0; i < (int32_t)doc.structure_items.size(); ++i) {
        if(doc.structure_items[i].parent) {
            auto current_parent = doc.structure_items.at(doc.structure_items[i].parent->id).obj_id;
            if(current_parent == si.obj_id) {
                children.emplace_back(CapyPDF_StructureItemId{i});
            }
        }
    }
    ObjectFormatter fmt;
    fmt.begin_dict();
    fmt.add_token_pair("/Type", "/StructElem");
    if(auto bi = std::get_if<CapyPDF_Structure_Type>(&si.stype)) {
        fmt.add_token("/S");
        fmt.add_token_with_slash(structure_type_names.at(*bi));
    } else if(auto ri = std::get_if<CapyPDF_RoleId>(&si.stype)) {
        const auto &role = *ri;
        fmt.add_token_pair("/S", bytes2pdfstringliteral(doc.rolemap.at(role.id).name));
    } else {
        fprintf(stderr, "UNREACHABLE.\n");
        std::abort();
    }
    fmt.add_token("/P");
    fmt.add_object_ref(parent_object);

    if(!children.empty()) {
        fmt.add_token("/K");
        fmt.begin_array(1);

        for(const auto &c : children) {
            fmt.add_object_ref(doc.structure_items.at(c.id).obj_id);
        }
        fmt.end_array();
    } else {
        // FIXME. Maybe not correct? Assumes that a struct item
        // either has children or is used on a page. Not both.
        const auto it = doc.structure_use.lookup(dsi.sid);
        if(it) {
            const auto &[page_num, mcid_num] = *it;
            fmt.add_token("/Pg");
            fmt.add_object_ref(doc.pages.at(page_num).page_obj_num);
            fmt.add_token_pair("/K", mcid_num);
        }
    }

    // Extra elements.
    if(!si.extra.T.empty()) {
        fmt.add_token_pair("/T", utf8_to_pdfutf16be(si.extra.T));
    }
    if(!si.extra.Lang.empty()) {
        fmt.add_token_pair("/Lang", pdfstring_quote(si.extra.Lang.sv()));
    }
    if(!si.extra.Alt.empty()) {
        fmt.add_token_pair("/Alt", utf8_to_pdfutf16be(si.extra.Alt));
    }
    if(!si.extra.ActualText.empty()) {
        fmt.add_token_pair("/ActualText", utf8_to_pdfutf16be(si.extra.ActualText));
    }
    fmt.end_dict();
    ERCV(write_finished_object(obj_num, fmt.steal(), {}));
    RETOK;
}

bool PdfWriter::add_info_key_to_trailer() const {
    if(auto *pdfa = std::get_if<CapyPDF_PDFA_Type>(&doc.docprops.subtype)) {
        if(*pdfa >= CAPY_PDFA_4f) {
            // FIXME, should be true if there is a PieceInfo.
            return false;
        }
    }

    return true;
}

} // namespace capypdf::internal
