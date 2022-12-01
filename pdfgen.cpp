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
    int32_t page_obj_num = 4;
    int32_t content_obj_num = 3;
    int32_t resources_obj_num = 2;
    int32_t num_pages = 1;

    std::string content("1.0 g\n");

    start_object(resources_obj_num);
    fprintf(ofile,
            "%d 0 obj\n<< /ExtGState << /a0 << /CA 1 /ca 1 >> >> >>\nendobj\n",
            resources_obj_num);
    finish_object();

    start_object(content_obj_num);
    fprintf(
        ofile, "%d 0 obj\n<</Length %d\n>>\nstream\n", content_obj_num, (int32_t)content.size());
    fprintf(ofile, "%s", content.c_str());
    fprintf(ofile, "endstream\nendobj\n");
    finish_object();

    start_object(page_obj_num);
    fprintf(ofile, "%d 0 obj\n", page_obj_num);
    fprintf(ofile, "<< /Type /Page\n");
    fprintf(ofile, "   /Parent %d 0 R\n", pages_obj_num);
    fprintf(ofile,
            "   /MediaBox [ %.2f %.2f %.2f %.2f ]\n",
            opts.mediabox.x,
            opts.mediabox.y,
            opts.mediabox.w,
            opts.mediabox.h);
    fprintf(ofile, "   /Contents %d 0 R\n", content_obj_num);
    fprintf(ofile, "   /Resources %d 0 R\n", resources_obj_num);
    fprintf(ofile, ">>\nendobj\n");
    finish_object();

    start_object(pages_obj_num);
    fprintf(ofile,
            "%d 0 obj\n<</Type /Pages\n   /Kids [ %d 0 R ]\n   /Count %d\n>>\nendobj\n",
            pages_obj_num,
            page_obj_num,
            num_pages);
    finish_object();
}

void PdfGen::write_catalog() {
    int32_t catalog_obj_num = 6;
    int32_t pages_obj_num = 5;
    start_object(catalog_obj_num);
    fprintf(ofile, "%d 0 obj\n", catalog_obj_num);
    fprintf(ofile, "<< /Type /Catalog\n   /Pages %d 0 R\n>>\n", pages_obj_num);
    fprintf(ofile, "endobj\n");
    finish_object();
}

void PdfGen::write_cross_reference_table() {
    fprintf(ofile, "%s\n", "xref");
    fprintf(ofile, "0 %d\n", (int32_t)object_offsets.size() + 1);
    fprintf(ofile, "%010ld 65535 f \n", (int64_t)0);
    for(const auto &i : object_offsets) {
        fprintf(ofile, "%010ld 00000 n \n", i);
    }
}

void PdfGen::write_trailer(int64_t xref_offset) {
    int32_t root = 6;
    int32_t info = 1;
    fprintf(ofile, "trailer\n");
    fprintf(ofile, "<< /Size %d\n", (int32_t)object_offsets.size() + 1);
    fprintf(ofile, "   /Root %d 0 R\n", root);
    fprintf(ofile, "   /Info %d 0 R\n", info);
    fprintf(ofile, ">>\n");
    fprintf(ofile, "startxref\n%ld\n%%%%EOF\n", xref_offset);
}

void PdfGen::start_object(int32_t obj_num) {
    if(obj_num != (int32_t)object_offsets.size() + 1) {
        throw std::runtime_error("Internal error, object count confusion.");
    }
    if(defining_object) {
        throw std::runtime_error(
            "Internal error, tried to define object when one was already ongoing.");
    }
    defining_object = true;
    object_offsets.push_back(ftell(ofile));
}

void PdfGen::finish_object() {
    if(!defining_object) {
        throw std::runtime_error(
            "Internal error, tried to finish object when one was not ongoing.");
    }
    defining_object = false;
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
