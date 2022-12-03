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

#include <pdfcommon.hpp>
#include <pdfcolorconverter.hpp>
#include <string>
#include <unordered_set>

class PdfGen;

class PdfPage {

public:
    PdfPage(PdfGen *g, PdfColorConverter *cm);
    ~PdfPage();
    void finalize();

    void save();
    void restore();
    void rectangle(double x, double y, double w, double h);
    void fill();
    void stroke();
    void set_line_width(double w);
    void set_stroke_color(const DeviceRGBColor &c);
    void set_nonstroke_color(const DeviceRGBColor &c);
    void draw_image(ImageId obj_num);
    void concatenate_matrix(double m1, double m2, double m3, double m4, double m5, double m6);
    void scale(double xscale, double yscale);
    void translate(double xtran, double ytran);
    void simple_text(const char *u8text, FontId font_id, double pointsize, double x, double y);

private:
    void build_resource_dict();

    PdfGen *g;
    PdfColorConverter *cm;
    std::string resources;
    std::string commands;
    std::unordered_set<int32_t> used_images;
    std::unordered_set<int32_t> used_fonts;
    bool is_finalized = false;
};
