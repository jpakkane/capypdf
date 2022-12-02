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

#include <string>
#include <unordered_set>

class PdfGen;

class PdfPage {

public:
    explicit PdfPage(PdfGen *g);
    ~PdfPage();
    void finalize();

    void save();
    void restore();
    void rectangle(double x, double y, double w, double h);
    void fill();
    void stroke();
    void set_line_width(double w);
    void set_stroke_color_rgb(double r, double g, double b);
    void set_nonstroke_color_rgb(double r, double g, double b);
    void draw_image(int32_t obj_num);
    void set_matrix(double m1, double m2, double m3, double m4, double m5, double m6);

private:
    void build_resource_dict();

    PdfGen *g;
    std::string resources;
    std::string commands;
    std::unordered_set<int32_t> used_images;
    bool is_finalized = false;
};
