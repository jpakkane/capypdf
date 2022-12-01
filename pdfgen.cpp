/*
 * Copyright 2022 Jussi Pakkanen
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

#include "pdfgen.hpp"
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <fmt/core.h>

namespace {

const char PDF_header[] = "%PDF-1.7\n\xe5\xf6\xc4\xd6\n";

}

PdfGen::PdfGen(const char *ofname, const PdfGenerationData &d) : opts{d} {
    ofile = fopen(ofname, "wb");
    if(!ofile) {
        throw std::runtime_error(strerror(errno));
    }
    write_header();
    write_info();
}

PdfGen::~PdfGen() {
    try {
        close_file();
    } catch(const std::exception &e) {
        fprintf(stderr, "%s", e.what());
        fclose(ofile);
        return;
    }

    if(fflush(ofile) != 0) {
        perror("Flushing file contents failed");
    }
    if(fclose(ofile) != 0) {
        perror("Closing output file failed");
    }
}

void PdfGen::write_bytes(const char *buf, size_t buf_size) {
    if(fwrite(buf, 1, buf_size, ofile) != buf_size) {
        throw std::runtime_error(strerror(errno));
    }
}

void PdfGen::write_header() { write_bytes(PDF_header, strlen(PDF_header)); }

void PdfGen::write_info() {
    std::string obj_data{"<<\n"};
    if(!opts.title.empty()) {
        obj_data += "  /Title (";
        obj_data += opts.title; // FIXME, do escaping.
        obj_data += ")\n";
    }
    if(!opts.author.empty()) {
        obj_data += "  /Author (";
        obj_data += opts.author; // FIXME, here too.
        obj_data += ")\n";
    }
    obj_data += "  /Producer (PDF Testbed generator)\n>>\n";
    add_object(obj_data);
}

void PdfGen::close_file() {
    write_pages();
    write_catalog();
    const int64_t xref_offset = ftell(ofile);
    write_cross_reference_table();
    write_trailer(xref_offset);
}

void PdfGen::write_pages() {
    int32_t pages_obj_num = 5;
    int32_t num_pages = 1;

    std::string content("1.0 g\n");
    std::string buf;

    const auto resources_obj_num = add_object("<< /ExtGState << /a0 << /CA 1 /ca 1 >> >> >>");

    fmt::format_to(std::back_inserter(buf),
                   R"(<<
  /Length {}
>>
stream
{}
endstream
endobj
)",
                   content.size(),
                   content);

    const auto content_obj_num = add_object(buf);
    buf.clear();
    fmt::format_to(std::back_inserter(buf),
                   R"(<<
  /Type /Page
  /Parent {} 0 R
  /MediaBox [ {} {} {} {} ]
  /Contents {} 0 R
  /Resources {} %d 0 R
>>
)",
                   pages_obj_num,
                   opts.mediabox.x,
                   opts.mediabox.y,
                   opts.mediabox.w,
                   opts.mediabox.h,
                   content_obj_num,
                   resources_obj_num);
    const auto page_obj_num = add_object(buf);
    buf.clear();

    fmt::format_to(std::back_inserter(buf),
                   R"(<<
  /Type /Pages
  /Kids [ {} 0 R ]
  /Count {}
>>
)",
                   page_obj_num,
                   num_pages);
    add_object(buf);
}

void PdfGen::write_catalog() {
    const int32_t pages_obj_num = 5;
    std::string buf;
    fmt::format_to(std::back_inserter(buf),
                   R"(<<
  /Type /Catalog
  /Pages {} 0 R
>>
)",
                   pages_obj_num);
    add_object(buf);
}

void PdfGen::write_cross_reference_table() {
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

void PdfGen::write_trailer(int64_t xref_offset) {
    const int32_t info = 1;                     // Info object is the first printed.
    const int32_t root = object_offsets.size(); // Root object is the last one printed.
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
                   object_offsets.size() + 1,
                   root,
                   info,
                   xref_offset);
    write_bytes(buf);
}

PdfPage PdfGen::new_page() {
    if(page_ongoing) {
        throw std::runtime_error("Tried to open a new page without closing the previous one.");
    }
    page_ongoing = true;
    return PdfPage(this);
}

void PdfGen::page_done() {
    if(!page_ongoing) {
        throw std::runtime_error("Tried to close a page even though one was not open.");
    }
    page_ongoing = false;
}

int32_t PdfGen::add_object(const std::string_view object_data) {
    auto object_num = (int32_t)object_offsets.size() + 1;
    object_offsets.push_back(ftell(ofile));
    const int bufsize = 128;
    char buf[bufsize];
    snprintf(buf, bufsize, "%d 0 obj\n", object_num);
    write_bytes(buf);
    write_bytes(object_data);
    write_bytes("endobj\n");
    return object_num;
}
