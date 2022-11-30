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

#include <cstdio>
#include <cstdint>

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
};

class PdfGen {
public:
    explicit PdfGen(const char *ofname, const PdfGenerationData &d);
    ~PdfGen();

private:
    void write_catalog();
    void write_pages();
    void write_header();
    void write_cross_reference_table();
    void write_trailer(int64_t xref_offset);

    void close_file();
    void write_bytes(const char *buf, size_t buf_size); // With error checking.
    FILE *ofile;
    PdfGenerationData opts;
};
