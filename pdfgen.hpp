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
#include <unordered_map>

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

struct PageOffsets {
    int32_t resource_obj_num;
    int32_t commands_obj_num;
};

struct ImageSize {
    int32_t w;
    int32_t h;
};

class PdfGen {
public:
    explicit PdfGen(const char *ofname, const PdfGenerationData &d);
    ~PdfGen();

    PdfPage new_page();

    int32_t add_object(std::string_view object_data);
    void add_page(std::string_view resource_data, std::string_view page_data);

    int32_t load_image(const char *fname);
    int32_t get_builtin_font_id(BuiltinFonts font);
    ImageSize get_image_info(int32_t obj_id) { return image_info.at(obj_id); }

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
    std::vector<PageOffsets> pages; // Refers to object num.
    std::unordered_map<int32_t, ImageSize> image_info;
    std::unordered_map<BuiltinFonts, int32_t> builtin_fonts;
};
