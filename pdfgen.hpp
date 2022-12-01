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

#pragma once

#include <pdfpage.hpp>

#include <cstdio>
#include <cstdint>

#include <vector>
#include <string_view>
#include <string>

struct PdfBox {
    double x;
    double y;
    double w;
    double h;
};

struct Area {
    double w;
    double h;

    static Area a4() { return Area{595.28, 841.89}; }
};

struct PdfGenerationData {
    Area page_size;
    PdfBox mediabox;
    std::string title;
    std::string author;
};

class PdfGen {
public:
    explicit PdfGen(const char *ofname, const PdfGenerationData &d);
    ~PdfGen();

    PdfPage new_page();
    void page_done();

    int32_t add_object(const std::string_view object_data);

private:
    void write_catalog();
    void write_pages();
    void write_header();
    void write_info();
    void write_cross_reference_table();
    void write_trailer(int64_t xref_offset);

    void start_object(int32_t obj_num);
    void finish_object();

    void close_file();
    void write_bytes(const char *buf, size_t buf_size); // With error checking.
    void write_bytes(std::string_view view) { write_bytes(view.data(), view.size()); }

    FILE *ofile;
    PdfGenerationData opts;
    std::vector<int64_t> object_offsets;
    bool page_ongoing = false;
};
