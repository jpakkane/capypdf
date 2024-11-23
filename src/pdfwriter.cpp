// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#include <pdfwriter.hpp>
#include <utils.hpp>

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

const char PDF_header[] = "%PDF-1.7\n%\xe5\xf6\xc4\xd6\n";

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

void write_rectangle(auto &appender, const char *boxname, const PdfRectangle &box) {
    std::format_to(
        appender, "  /{} [ {:f} {:f} {:f} {:f} ]\n", boxname, box.x1, box.y1, box.x2, box.y2);
}

std::string create_subset_cmap(const std::vector<TTGlyphs> &glyphs) {
    std::string buf = std::format(R"(/CIDInit/ProcSet findresource begin
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
        if(std::holds_alternative<LigatureGlyph>(g)) {
            const auto &lg = std::get<LigatureGlyph>(g);
            const auto u16repr = utf8_to_pdfutf16be(lg.text, false);
            std::format_to(appender, "<{:02X}> <{}>\n", i, u16repr);
        } else {
            uint32_t unicode_codepoint = 0;
            if(std::holds_alternative<RegularGlyph>(g)) {
                unicode_codepoint = std::get<RegularGlyph>(g).unicode_codepoint;
            }
            std::format_to(appender, "<{:02X}> <{:04X}>\n", i, unicode_codepoint);
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

rvoe<std::string> build_subset_width_array(FT_Face face, const std::vector<TTGlyphs> &glyphs) {
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
        // I don't know if this is correct or not, but it worked with all fonts I had.
        //
        // Determined via debugging empirism.
        std::format_to(bi, "{} ", (int32_t)(double(horiadvance) * 1000 / face->units_per_EM));
    }
    arr += "]";
    return arr;
}

} // namespace

PdfWriter::PdfWriter(PdfDocument &doc) : doc(doc) {}

rvoe<NoReturnValue> PdfWriter::write_to_file(const std::filesystem::path &ofilename) {
    if(doc.pages.size() == 0) {
        RETERR(NoPages);
    }
    if(doc.write_attempted) {
        RETERR(WritingTwice);
    }
    doc.write_attempted = true;
    auto tempfname = ofilename;
    tempfname.replace_extension(".pdf~");
    FILE *out_file = fopen(tempfname.string().c_str(), "wb");
    if(!out_file) {
        perror(nullptr);
        RETERR(CouldNotOpenFile);
    }
    std::unique_ptr<FILE, int (*)(FILE *)> fcloser(out_file, fclose);

    try {
        ERCV(write_to_file(out_file));
    } catch(const std::exception &e) {
        fprintf(stderr, "%s\n", e.what());
        RETERR(DynamicError);
    } catch(...) {
        fprintf(stderr, "Unexpected error.\n");
        RETERR(DynamicError);
    }

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
    std::error_code ec;
    std::filesystem::rename(tempfname, ofilename, ec);
    if(ec) {
        fprintf(stderr, "%s\n", ec.category().message(ec.value()).c_str());
        RETERR(FileWriteError);
    }
    RETOK;
}

rvoe<NoReturnValue> PdfWriter::write_to_file(FILE *output_file) {
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

rvoe<NoReturnValue> PdfWriter::write_to_file_impl() {
    ERCV(write_header());
    ERCV(doc.create_catalog());
    doc.pad_subset_fonts();
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
    return write_bytes(PDF_header, strlen(PDF_header));
}

rvoe<std::vector<uint64_t>> PdfWriter::write_objects() {
    size_t i = 0;
    auto visitor = overloaded{
        [](DummyIndexZero &) -> rvoe<NoReturnValue> { RETOK; },

        [&](const FullPDFObject &pobj) -> rvoe<NoReturnValue> {
            ERCV(write_finished_object(i, pobj.dictionary, pobj.stream));
            RETOK;
        },

        [&](const DeflatePDFObject &pobj) -> rvoe<NoReturnValue> {
            ERC(compressed, flate_compress(pobj.stream));
            std::string dict = std::format("{}  /Filter /FlateDecode\n  /Length {}\n>>\n",
                                           pobj.unclosed_dictionary,
                                           compressed.size());
            ERCV(write_finished_object(i, dict, compressed));
            RETOK;
        },

        [&](const DelayedSubsetFontData &ssfont) -> rvoe<NoReturnValue> {
            ERCV(write_subset_font_data(i, ssfont));
            RETOK;
        },

        [&](const DelayedSubsetFontDescriptor &ssfontd) -> rvoe<NoReturnValue> {
            write_subset_font_descriptor(i,
                                         doc.fonts.at(ssfontd.fid.id).fontdata,
                                         ssfontd.subfont_data_obj,
                                         ssfontd.subset_num);
            RETOK;
        },

        [&](const DelayedSubsetCMap &sscmap) -> rvoe<NoReturnValue> {
            ERCV(write_subset_cmap(i, doc.fonts.at(sscmap.fid.id), sscmap.subset_id));
            RETOK;
        },

        [&](const DelayedSubsetFont &ssfont) -> rvoe<NoReturnValue> {
            ERCV(write_subset_font(i,
                                   doc.fonts.at(ssfont.fid.id),
                                   0,
                                   ssfont.subfont_descriptor_obj,
                                   ssfont.subfont_cmap_obj));
            RETOK;
        },

        [&](const DelayedPages &) -> rvoe<NoReturnValue> {
            ERCV(write_pages_root());
            RETOK;
        },

        [&](const DelayedPage &dp) -> rvoe<NoReturnValue> {
            ERCV(write_delayed_page(dp));
            RETOK;
        },

        [&](const DelayedCheckboxWidgetAnnotation &checkbox) -> rvoe<NoReturnValue> {
            ERCV(write_checkbox_widget(i, checkbox));
            RETOK;
        },

        [&](const DelayedAnnotation &anno) -> rvoe<NoReturnValue> {
            ERCV(write_annotation(i, anno));
            RETOK;
        },

        [&](const DelayedStructItem &si) -> rvoe<NoReturnValue> {
            ERCV(write_delayed_structure_item(i, si));
            RETOK;
        },
    };

    std::vector<uint64_t> object_offsets;
    for(; i < doc.document_objects.size(); ++i) {
        object_offsets.push_back(ftell(ofile));
        ERCV(std::visit(visitor, doc.document_objects.at(i)));
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

rvoe<NoReturnValue> PdfWriter ::write_trailer(int64_t xref_offset) {
    const int32_t info = 1;                               // Info object is the first printed.
    const int32_t root = doc.document_objects.size() - 1; // Root object is the last one printed.
    std::string buf;
    auto documentid = create_trailer_id();
    std::format_to(std::back_inserter(buf),
                   R"(trailer
<<
  /Size {}
  /Root {} 0 R
  /Info {} 0 R
  /ID [{}{}]
>>
startxref
{}
%%EOF
)",
                   doc.document_objects.size(),
                   root,
                   info,
                   documentid,
                   documentid,
                   xref_offset);
    return write_bytes(buf);
}

rvoe<NoReturnValue> PdfWriter::write_finished_object(int32_t object_number,
                                                     std::string_view dict_data,
                                                     std::string_view stream_data) {
    std::string buf;
    auto appender = std::back_inserter(buf);
    std::format_to(appender, "{} 0 obj\n", object_number);
    buf += dict_data;
    if(!stream_data.empty()) {
        if(buf.back() != '\n') {
            buf += '\n';
        }
        buf += "stream\n";
        buf += stream_data;
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

rvoe<NoReturnValue> PdfWriter::write_subset_font(int32_t object_num,
                                                 const FontThingy &font,
                                                 int32_t subset,
                                                 int32_t font_descriptor_obj,
                                                 int32_t tounicode_obj) {
    auto face = font.fontdata.face.get();
    const std::vector<TTGlyphs> &subset_glyphs = font.subsets.get_subset(subset);
    int32_t start_char = 0;
    int32_t end_char = subset_glyphs.size() - 1;
    ERC(width_arr, build_subset_width_array(face, subset_glyphs));
    auto objbuf = std::format(R"(<<
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
    RETOK;
}

rvoe<NoReturnValue> PdfWriter::write_subset_font_data(int32_t object_num,
                                                      const DelayedSubsetFontData &ssfont) {
    const auto &font = doc.fonts.at(ssfont.fid.id);
    ERC(subset_font, font.subsets.generate_subset(font.fontdata.fontdata, ssfont.subset_id));

    ERC(compressed_bytes, flate_compress(subset_font));
    std::string dictbuf = std::format(R"(<<
  /Length {}
  /Length1 {}
  /Filter /FlateDecode
>>
)",
                                      compressed_bytes.size(),
                                      subset_font.size());
    ERCV(write_finished_object(object_num, dictbuf, compressed_bytes));
    RETOK;
}

void PdfWriter::write_subset_font_descriptor(int32_t object_num,
                                             const TtfFont &font,
                                             int32_t font_data_obj,
                                             int32_t subset_number) {
    auto face = font.face.get();
    const uint32_t fflags = 4;
    auto objbuf = std::format(R"(<<
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

rvoe<NoReturnValue>
PdfWriter::write_subset_cmap(int32_t object_num, const FontThingy &font, int32_t subset_number) {
    auto cmap = create_subset_cmap(font.subsets.get_subset(subset_number));
    auto dict = std::format(R"(<<
  /Length {}
>>
)",
                            cmap.length());
    return write_finished_object(object_num, dict, cmap);
}

rvoe<NoReturnValue> PdfWriter::write_pages_root() {
    std::string buf;
    auto buf_append = std::back_inserter(buf);
    buf = std::format(R"(<<
  /Type /Pages
  /Kids [
)");
    for(const auto &i : doc.pages) {
        std::format_to(buf_append, "    {} 0 R\n", i.page_obj_num);
    }
    std::format_to(buf_append, "  ]\n  /Count {}\n>>\n", doc.pages.size());
    return write_finished_object(doc.pages_object, buf, "");
}

rvoe<NoReturnValue> PdfWriter::write_delayed_page(const DelayedPage &dp) {
    std::string buf;

    auto buf_append = std::back_inserter(buf);
    auto &p = doc.pages.at(dp.page_num);
    std::format_to(buf_append,
                   R"(<<
  /Type /Page
  /Parent {} 0 R
  /LastModified {}
)",
                   doc.pages_object,
                   current_date_string());
    if(dp.custom_props.transparency_props) {
        buf += "  /Group ";
        dp.custom_props.transparency_props->serialize(buf_append, "  ");
    } else if(doc.opts.default_page_properties.transparency_props) {
        buf += "  /Group ";
        doc.opts.default_page_properties.transparency_props->serialize(buf_append, "  ");
    }
    PageProperties current_props = doc.opts.default_page_properties.merge_with(dp.custom_props);
    write_rectangle(buf_append, "MediaBox", *current_props.mediabox);

    if(current_props.cropbox) {
        write_rectangle(buf_append, "CropBox", *current_props.cropbox);
    }
    if(current_props.bleedbox) {
        write_rectangle(buf_append, "BleedBox", *current_props.bleedbox);
    }
    if(current_props.trimbox) {
        write_rectangle(buf_append, "TrimBox", *current_props.trimbox);
    }
    if(current_props.artbox) {
        write_rectangle(buf_append, "ArtBox", *current_props.artbox);
    }
    if(dp.structparents) {
        std::format_to(buf_append, "  /StructParents {}\n", dp.structparents.value());
    }

    if(current_props.user_unit) {
        std::format_to(buf_append, "  /UserUnit {:f}\n", *current_props.user_unit);
    }

    std::format_to(buf_append,
                   R"(  /Contents {} 0 R
  /Resources {} 0 R
)",
                   p.commands_obj_num,
                   p.resource_obj_num);

    if(!dp.used_form_widgets.empty() || !dp.used_annotations.empty()) {
        buf += "  /Annots [\n";
        for(const auto &a : dp.used_form_widgets) {
            std::format_to(buf_append, "    {} 0 R\n", doc.form_widgets.at(a.id));
        }
        for(const auto &a : dp.used_annotations) {
            std::format_to(buf_append, "    {} 0 R\n", doc.annotations.at(a.id));
        }
        buf += "  ]\n";
    }
    if(dp.transition) {
        const auto &t = *dp.transition;
        serialize_trans(buf_append, t, "  ");
    }

    if(dp.subnav_root) {
        std::format_to(buf_append, "  /PresSteps {} 0 R\n", dp.subnav_root.value());
    }
    buf += ">>\n";

    return write_finished_object(p.page_obj_num, buf, "");
}

rvoe<NoReturnValue>
PdfWriter::write_checkbox_widget(int obj_num, const DelayedCheckboxWidgetAnnotation &checkbox) {
    auto loc = doc.form_use.find(checkbox.widget);
    if(loc == doc.form_use.end()) {
        std::abort();
    }

    std::string dict = std::format(R"(<<
  /Type /Annot
  /Subtype /Widget
  /Rect [ {:f} {:f} {:f} {:f} ]
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
                                   doc.form_xobjects.at(checkbox.on.id).xobj_num,
                                   doc.form_xobjects.at(checkbox.off.id).xobj_num);
    ERCV(write_finished_object(obj_num, dict, ""));
    RETOK;
}

rvoe<NoReturnValue> PdfWriter::write_annotation(int obj_num, const DelayedAnnotation &annotation) {
    auto loc = doc.annotation_use.find(annotation.id);
    // It is ok for an annotation not to be used.

    assert(annotation.a.rect);
    std::string dict = std::format(R"(<<
  /Type /Annot
  /Rect [ {:f} {:f} {:f} {:f} ]
  /F {}
)",
                                   annotation.a.rect->x1,
                                   annotation.a.rect->y1,
                                   annotation.a.rect->x2,
                                   annotation.a.rect->y2,
                                   (int)annotation.a.flags);
    auto app = std::back_inserter(dict);
    if(loc != doc.annotation_use.end()) {
        std::format_to(app, "  /P {} 0 R\n", loc->second);
    }
    if(const auto ta = std::get_if<TextAnnotation>(&annotation.a.sub)) {
        std::format_to(app,
                       R"(  /Subtype /Text
  /Contents {}
)",
                       utf8_to_pdfutf16be(ta->content));
    } else if(auto faa = std::get_if<FileAttachmentAnnotation>(&annotation.a.sub)) {
        std::format_to(app,
                       R"(  /Subtype /FileAttachment
  /FS {} 0 R
)",
                       doc.get(faa->fileid).filespec_obj);
    } else if(auto ua = std::get_if<UriAnnotation>(&annotation.a.sub)) {
        auto uri_as_str = pdfstring_quote(ua->uri.sv());
        std::format_to(app,
                       R"(  /Subtype /Link
  /Contents {}
  /A <<
    /S /URI
    /URI {}
  >>
)",
                       uri_as_str,
                       uri_as_str);
    } else if(auto sa = std::get_if<ScreenAnnotation>(&annotation.a.sub)) {
        int32_t media_filespec = doc.get(sa->mediafile).filespec_obj;
        if(!sa->times) {
            std::format_to(app,
                           R"(  /Subtype /Screen
  /A <<
    /Type /Action
    /S /Rendition
    /OP 0
    /AN {} 0 R
    /R <<
      /Type /Rendition
      /S /MR
      /C <<
        /Type /MediaClip
        /CT ({})
        /S /MCD
        /D {} 0 R
        /P << /TF (TEMPALWAYS) >>
      >>
    >>
  >>
)",
                           obj_num,
                           sa->mimetype,
                           media_filespec);
        } else {
            // NOTE! This should work but does not. Acrobat reader will error
            // out if there are any entries in the MH dictionary, regardless whether they
            // are time or frame dictionaries.
            std::format_to(app,
                           R"(  /Subtype /Screen
  /A <<
    /Type /Action
    /S /Rendition
    /OP 0
    /AN {} 0 R
    /R <<
      /Type /Rendition
      /S /MR
      /C <<
        /Type /MediaClip
        /S /MCS
        /D <<
          /Type /MediaClip
          /CT ({})
          /S /MCD
          /D {} 0 R
          /P << /TF (TEMPALWAYS) >>
        >>
        /MH <<
          /B << /S /T /T << /S /S /V {:f} >> >>
          /E << /S /T /T << /S /S /V {:f} >> >>
        >>
      >>
    >>
  >>
)",
                           obj_num,
                           sa->mimetype,
                           media_filespec,
                           sa->times->starttime,
                           sa->times->endtime);
        }
    } else if(auto pma = std::get_if<PrintersMarkAnnotation>(&annotation.a.sub)) {
        std::format_to(app,
                       R"(  /Subtype /PrinterMark
  /AP << /N {} 0 R >>
)",
                       doc.form_xobjects.at(pma->appearance.id).xobj_num);
    } else {
        fprintf(stderr, "Unknown annotation type.\n");
        std::abort();
    }
    dict += ">>\n";
    ERCV(write_finished_object(obj_num, dict, ""));
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
    std::string dict = R"(<<
  /Type /StructElem
)";
    auto app = std::back_inserter(dict);
    if(auto bi = std::get_if<CapyPDF_StructureType>(&si.stype)) {
        std::format_to(app,
                       R"(  /S /{}
)",
                       structure_type_names.at(*bi));
    } else if(auto ri = std::get_if<CapyPDF_RoleId>(&si.stype)) {
        const auto &role = *ri;
        std::format_to(app, "  /S {}\n", bytes2pdfstringliteral(doc.rolemap.at(role.id).name));
    } else {
        fprintf(stderr, "UNREACHABLE.\n");
        std::abort();
    }
    std::format_to(app, "  /P {} 0 R\n", parent_object);

    if(!children.empty()) {
        dict += "  /K [\n";
        for(const auto &c : children) {
            std::format_to(app, "    {} 0 R\n", doc.structure_items.at(c.id).obj_id);
        }
        dict += "  ]\n";
    } else {
        // FIXME. Maybe not correct? Assumes that a struct item
        // either has children or is used on a page. Not both.
        const auto it = doc.structure_use.find(dsi.sid);
        if(it != doc.structure_use.end()) {
            const auto &[page_num, mcid_num] = it->second;
            std::format_to(app, "  /Pg {} 0 R\n", doc.pages.at(page_num).page_obj_num);
            std::format_to(app, "  /K {}\n", mcid_num);
        }
    }

    // Extra elements.
    if(!si.extra.T.empty()) {
        std::format_to(app, "  /T {}\n", utf8_to_pdfutf16be(si.extra.T));
    }
    if(!si.extra.Lang.empty()) {
        std::format_to(app, "  /Lang {}\n", pdfstring_quote(si.extra.Lang.sv()));
    }
    if(!si.extra.Alt.empty()) {
        std::format_to(app, "  /Alt {}\n", utf8_to_pdfutf16be(si.extra.Alt));
    }
    if(!si.extra.ActualText.empty()) {
        std::format_to(app, "  /ActualText {}\n", utf8_to_pdfutf16be(si.extra.ActualText));
    }
    dict += ">>\n";
    ERCV(write_finished_object(obj_num, dict, ""));
    RETOK;
}

} // namespace capypdf::internal
