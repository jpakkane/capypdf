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

const char PDF_header[] = {"%PDF-1.7\n\xe5\xf6\xc4\xd6\n"};

}

PdfGen::PdfGen(const char *ofname, const PdfGenerationData &d) : opts{d} {
    ofile = fopen(ofname, "wb");
    if(!ofile) {
        throw std::runtime_error(strerror(errno));
    }
    write_header();
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

void PdfGen::close_file() {
    write_pages();
    const int64_t xref_offset = ftell(ofile);
    write_catalog();
    write_cross_reference_table();
    write_trailer(xref_offset);
}

void PdfGen::write_pages() {
    int32_t pages_obj_num = 3;
    int32_t page_obj_num = 2;
    int32_t content_obj_num = 1;
    int32_t resources_obj_num = 0;
    int32_t num_pages = 1;

    std::string content("q\nQ\n");

    fprintf(ofile, "%d 0 obj\n<< >>\nendobj\n", resources_obj_num);

    fprintf(
        ofile, "%d 0 obj\n<</Length %d\n>>\nstream\n", content_obj_num, (int32_t)content.size());
    fprintf(ofile, "%s", content.c_str());
    fprintf(ofile, "endstream\nendobj\n");

    fprintf(ofile, "%d 0 obj\n", page_obj_num);
    fprintf(ofile, "<< /Type /Page\n");
    fprintf(ofile, "   /Parent %d\n", pages_obj_num);
    fprintf(ofile,
            "   /MediaBox [%.2f, %.2f, %.2f %.2f]\n",
            opts.mediabox.x,
            opts.mediabox.y,
            opts.mediabox.w,
            opts.mediabox.h);
    fprintf(ofile, "   /Contents %d 0 R\n", content_obj_num);
    fprintf(ofile, "   /Resources %d 0 R\n", resources_obj_num);

    fprintf(ofile,
            "%d 0 obj\n<</Type /Pages\n   /Kids [ %d 0 R ]\n   /Count %d\n>>endobj\n",
            pages_obj_num,
            page_obj_num,
            num_pages);
}

void PdfGen::write_catalog() {
    int32_t catalog_obj_num = 4;
    int32_t pages_obj_num = 3;
    fprintf(ofile, "%d 0 obj\n", catalog_obj_num);
    fprintf(ofile, "<< /Type /Catalog\n   /Pages %d 0 R\n>>\n", pages_obj_num);
    fprintf(ofile, "endobj\n");
}

void PdfGen::write_cross_reference_table() {
    int32_t num_items = 10;
    fprintf(ofile, "%s\n", "xref");
    fprintf(ofile, "0 %d\n", num_items);
    for(int32_t i = 0; i < num_items; ++i) {
        fprintf(ofile, "%010d 00000 f \n", i);
    }
}

void PdfGen::write_trailer(int64_t xref_offset) {
    int32_t num_items = 42;
    int32_t root = 1;
    int32_t info = 2;
    fprintf(ofile, "trailer\n");
    fprintf(ofile, "<< /Size %d\n", num_items);
    fprintf(ofile, "   /Root %d 0 R\n", root);
    fprintf(ofile, "   /Info %d 0 R\n", info);
    fprintf(ofile, ">>\n");
    fprintf(ofile, "startxref\n%ld\n%%EOF\n", xref_offset);
}
