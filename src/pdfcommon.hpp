/*
 * Copyright 2022-2023 Jussi Pakkanen
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

#include <a4pdf.h>

#include <optional>

#include <cstdint>

namespace A4PDF {

struct PdfBox {
    double x;
    double y;
    double w;
    double h;

    static PdfBox a4() { return PdfBox{0, 0, 595.28, 841.89}; }
};

class LimitDouble {
public:
    LimitDouble() : value(minval) {}

    // No "explicit" because we want the following to work for convenience:
    // DeviceRGBColor{0.0, 0.3, 1.0}
    LimitDouble(double new_val) : value(new_val) { clamp(); }

    double v() const { return value; }

    LimitDouble &operator=(double d) {
        value = d;
        clamp();
        return *this;
    }

private:
    constexpr static double maxval = 1.0;
    constexpr static double minval = 0.0;

    void clamp() {
        if(value < minval) {
            value = minval;
        }
        if(value > maxval) {
            value = maxval;
        }
    }

    double value;
};

// Every resource type has its own id type to avoid
// accidentally mixing them up.

struct FontId {
    int32_t id;
};

struct ImageId {
    int32_t id;
};

struct SeparationId {
    int32_t id;
};

struct GstateId {
    int32_t id;
};

struct GraphicsState {
    std::optional<A4PDF_Rendering_Intent> intent;
    std::optional<A4PDF_Blend_Mode> blend_mode;
};

struct DeviceRGBColor {
    LimitDouble r;
    LimitDouble g;
    LimitDouble b;
};

struct DeviceGrayColor {
    LimitDouble v;
};

struct DeviceCMYKColor {
    LimitDouble c;
    LimitDouble m;
    LimitDouble y;
    LimitDouble k;
};

struct FontSubset {
    FontId fid;
    int32_t subset_id;

    bool operator==(const FontSubset &other) const {
        return fid.id == other.fid.id && subset_id == other.subset_id;
    }

    bool operator!=(const FontSubset &other) const { return !(*this == other); }
};

} // namespace A4PDF
