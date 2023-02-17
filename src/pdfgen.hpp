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

#include <pdfdrawcontext.hpp>

#include <cstdio>
#include <cstdint>

#include <vector>
#include <string_view>
#include <optional>
#include <filesystem>

namespace A4PDF {

class PdfGen;

struct DrawContextPopper {
    PdfGen *g;
    PdfDrawContext ctx;

    explicit DrawContextPopper(PdfGen *g,
                               PdfDocument *doc,
                               PdfColorConverter *cm,
                               A4PDF_Draw_Context_Type dtype)
        : g{g}, ctx{doc, cm, dtype} {}

    DrawContextPopper() = delete;
    DrawContextPopper(const DrawContextPopper &) = delete;

    ~DrawContextPopper();
};

class PdfGen {
public:
    explicit PdfGen(const char *ofname, const PdfGenerationData &d);
    ~PdfGen();

    void new_page();

    ImageId load_image(const char *fname) { return pdoc.load_image(fname); }
    ImageId embed_jpg(const char *fname) { return pdoc.embed_jpg(fname); }
    FontId load_font(const char *fname) { return pdoc.load_font(ft, fname); };
    ImageSize get_image_info(ImageId img_id) { return pdoc.image_info.at(img_id.id).s; }
    SeparationId create_separation(std::string_view name, const DeviceCMYKColor &fallback) {
        return pdoc.create_separation(name, fallback);
    }
    GstateId add_graphics_state(const GraphicsState &state) {
        return pdoc.add_graphics_state(state);
    }

    FunctionId add_function(const FunctionType2 &func) { return pdoc.add_function(func); }

    ShadingId add_shading(const ShadingType2 &shade) { return pdoc.add_shading(shade); }
    ShadingId add_shading(const ShadingType3 &shade) { return pdoc.add_shading(shade); }

    LabId add_lab_colorspace(const LabColorSpace &lab) { return pdoc.add_lab_colorspace(lab); }

    DrawContextPopper guarded_page_context();
    PdfDrawContext *new_page_draw_context();

    void add_page(PdfDrawContext &ctx);

private:
    void close_file();

    std::filesystem::path ofilename;
    FT_Library ft;
    PdfDocument pdoc;
};

} // namespace A4PDF