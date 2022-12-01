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

class PdfGen;

class PdfPage {

public:
    explicit PdfPage(PdfGen *g);
    ~PdfPage();

    void rectangle(double x, double y, double w, double h);
    void fill();
    void stroke();

private:
    PdfGen *g;
    std::string resources;
    std::string commands;
};
