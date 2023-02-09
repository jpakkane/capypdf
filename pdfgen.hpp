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

#pragma once

#include <pdfpage.hpp>
#include <pdfdocument.hpp>

#include <cstdio>
#include <cstdint>

#include <vector>
#include <string_view>
#include <optional>
#include <filesystem>

class PdfGen {
public:
    explicit PdfGen(const char *ofname, const PdfGenerationData &d);
    ~PdfGen();

    void new_page();

    ImageId load_image(const char *fname) { return pdoc.load_image(fname); }
    FontId load_font(const char *fname) { return pdoc.load_font(ft, fname); };
    ImageSize get_image_info(ImageId img_id) { return pdoc.image_info.at(img_id.id).s; }
    SeparationId create_separation(std::string_view name, const DeviceCMYKColor &fallback) {
        return pdoc.create_separation(name, fallback);
    }

    PdfPage &page_context() { return page; }

    friend class PdfPage;

private:
    void close_file();

    std::filesystem::path ofilename;
    FT_Library ft;
    PdfDocument pdoc;
    PdfPage page;
};
