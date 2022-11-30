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

const char PDF_header[] = {"PDF-1.7\n\xe5\xf6\xc4\xd6\n"};

}

PdfGen::PdfGen(const char *ofname) {
    ofile = fopen(ofname, "wb");
    if(!ofile) {
        throw std::runtime_error(strerror(errno));
    }
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
    const int64_t xref_offset = ftell(ofile);
    write_cross_reference_table();
    write_trailer(xref_offset);
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
