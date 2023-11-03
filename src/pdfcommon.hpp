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

#include <capypdf.h>
#include <errorhandling.hpp>

#include <optional>
#include <vector>
#include <array>
#include <string>
#include <string_view>
#include <functional>
#include <variant>

#include <cstdint>
#include <cmath>

// This macro must not be used from within a namespace.
#define DEF_BASIC_OPERATORS(TNAME)                                                                 \
    inline bool operator==(const TNAME &object_number_1, const TNAME &object_number_2) {           \
        return object_number_1.id == object_number_2.id;                                           \
    }                                                                                              \
                                                                                                   \
    inline std::strong_ordering operator<=>(const TNAME &object_number_1,                          \
                                            const TNAME &object_number_2) {                        \
        return object_number_1.id <=> object_number_2.id;                                          \
    }                                                                                              \
                                                                                                   \
    template<> struct std::hash<TNAME> {                                                           \
        std::size_t operator()(const TNAME &tobj) const noexcept {                                 \
            return std::hash<int32_t>{}(tobj.id);                                                  \
        }                                                                                          \
    }

DEF_BASIC_OPERATORS(CapyPDF_ImageId);

DEF_BASIC_OPERATORS(CapyPDF_FontId);

DEF_BASIC_OPERATORS(CapyPDF_IccColorSpaceId);

DEF_BASIC_OPERATORS(CapyPDF_FormXObjectId);

DEF_BASIC_OPERATORS(CapyPDF_FormWidgetId);

DEF_BASIC_OPERATORS(CapyPDF_AnnotationId);

DEF_BASIC_OPERATORS(CapyPDF_StructureItemId);

DEF_BASIC_OPERATORS(CapyPDF_OptionalContentGroupId);

DEF_BASIC_OPERATORS(CapyPDF_TransparencyGroupId);

namespace capypdf {

class u8string {
public:
    u8string() = default;
    u8string(u8string &&o) = default;
    u8string(const u8string &o) = default;

    std::string_view sv() const { return buf; }

    static rvoe<u8string> from_cstr(const char *cstr);
    static rvoe<u8string> from_cstr(const std::string &str) {
        return u8string::from_cstr(str.c_str());
    }

    bool empty() const { return buf.empty(); }

    u8string &operator=(u8string &&o) = default;
    u8string &operator=(const u8string &o) = default;

private:
    explicit u8string(const char *prevalidated_utf8) : buf(prevalidated_utf8) {}
    std::string buf;
};

struct PdfBox {
    double x{};
    double y{};
    double w{};
    double h{};

    static PdfBox a4() { return PdfBox{0, 0, 595.28, 841.89}; }
};

struct PdfRectangle {
    double x1{};
    double y1{};
    double x2{};
    double y2{};

    static PdfRectangle a4() { return PdfRectangle{0, 0, 595.28, 841.89}; }
};

struct Point {
    double x, y;
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
        if(std::isnan(value)) {
            value = minval;
        } else if(value < minval) {
            value = minval;
        } else if(value > maxval) {
            value = maxval;
        }
    }

    double value;
};

// Every resource type has its own id type to avoid
// accidentally mixing them up.

struct SeparationId {
    int32_t id;
};

struct PatternId {
    int32_t id;
};

struct LabId {
    int32_t id;
};

struct PageId {
    int32_t id;
};

struct OutlineId {
    int32_t id;
};

// Named and ordered according to PDF spec 2.0 section 8.4.5, table 57
struct GraphicsState {
    std::optional<double> LW;
    std::optional<CAPYPDF_Line_Cap> LC;
    std::optional<CAPYPDF_Line_Join> LJ;
    std::optional<double> ML;
    std::optional<CapyPDF_Rendering_Intent> RI;
    std::optional<bool> OP;
    std::optional<bool> op;
    std::optional<int32_t> OPM;
    // std::optional<DashArray> D;
    // std::optional<FontSomething> Font;
    // std::optional<std::string> BG;
    // std::optional<std::string> BG2;
    // std::optional<std::string> UCR;
    // std::optional<std::string> UCR2;
    // std::optional<std::string> TR;
    // std::optional<std::string> TR2;
    // std::optional<str::string> HT;
    std::optional<double> FT;
    std::optional<double> SM;
    std::optional<bool> SA;
    std::optional<CAPYPDF_Blend_Mode> BM;
    // std::optional<std::string> SMask;
    std::optional<LimitDouble> CA;
    std::optional<LimitDouble> ca;
    std::optional<bool> AIS;
    std::optional<bool> TK;
    // std::string UseBlackPtComp;
    //  std::optional<Point> HTO;
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

struct LabColor {
    LabId id;
    double l;
    double a;
    double b;
};

struct ICCColor {
    CapyPDF_IccColorSpaceId id;
    std::vector<double> values;
};

struct SeparationColor {
    SeparationId id;
    LimitDouble v;
};

typedef std::variant<DeviceRGBColor,
                     DeviceGrayColor,
                     DeviceCMYKColor,
                     ICCColor,
                     LabColor,
                     SeparationColor,
                     PatternId>
    Color;

struct LabColorSpace {
    double xw, yw, zw;
    double amin, amax, bmin, bmax;

    static LabColorSpace cielab_1976_D65() {
        LabColorSpace cl;
        cl.xw = 0.9505;
        cl.yw = 1.0;
        cl.zw = 1.089;

        cl.amin = -128;
        cl.amax = 127;
        cl.bmin = -128;
        cl.bmax = 127;
        return cl;
    }
};

struct FunctionType2 {
    std::vector<double> domain;
    Color C0;
    Color C1;
    double n;
};

// Linear
struct ShadingType2 {
    CapyPDF_Colorspace colorspace;
    double x0, y0, x1, y1;
    CapyPDF_FunctionId function;
    bool extend0, extend1;
};

// Radial
struct ShadingType3 {
    CapyPDF_Colorspace colorspace;
    double x0, y0, r0, x1, y1, r1;
    CapyPDF_FunctionId function;
    bool extend0, extend1;
};

// Gouraud

struct ShadingPoint {
    Point p;
    Color c;
};

struct ShadingElement {
    ShadingPoint sp;
    int32_t flag;
};

struct ShadingType4 {
    std::vector<ShadingElement> elements;
    double minx;
    double miny;
    double maxx;
    double maxy;
    CapyPDF_Colorspace colorspace = CAPYPDF_CS_DEVICE_RGB;

    void start_strip(const ShadingPoint &v0, const ShadingPoint &v1, const ShadingPoint &v2) {
        elements.emplace_back(ShadingElement{v0, 0});
        elements.emplace_back(ShadingElement{v1, 0});
        elements.emplace_back(ShadingElement{v2, 0});
    }

    void extend_strip(const ShadingPoint &v, int flag) {
        // assert(flag == 1 || flag == 2);
        elements.emplace_back(ShadingElement{v, flag});
    }
};

struct FullCoonsPatch {
    std::array<Point, 12> p;
    std::array<DeviceRGBColor, 4> c;
};

struct ContinuationCoonsPatch {
    int flag;
    std::array<Point, 8> p;
    std::array<DeviceRGBColor, 2> c;
};

typedef std::variant<FullCoonsPatch, ContinuationCoonsPatch> CoonsPatches;

struct ShadingType6 {
    std::vector<CoonsPatches> elements;
    double minx = 0;
    double miny = 0;
    double maxx = 200;
    double maxy = 200;
    CapyPDF_Colorspace colorspace = CAPYPDF_CS_DEVICE_RGB;
};

struct TextStateParameters {
    std::optional<double> char_spacing;
    std::optional<double> word_spacing;
    std::optional<double> horizontal_scaling;
    std::optional<double> leading;
    std::optional<CapyPDF_Text_Mode> render_mode;
    std::optional<double> rise;
    // Knockout can only be set with gs.
};

struct FontSubset {
    CapyPDF_FontId fid;
    int32_t subset_id;

    bool operator==(const FontSubset &other) const {
        return fid.id == other.fid.id && subset_id == other.subset_id;
    }

    bool operator!=(const FontSubset &other) const { return !(*this == other); }
};

extern const std::array<const char *, 4> rendering_intent_names;

struct Transition {
    std::optional<CAPYPDF_Transition_Type> type;
    std::optional<double> duration;
    std::optional<bool> Dm;    // true is horizontal
    std::optional<bool> M;     // true is inward
    std::optional<int32_t> Di; // FIXME, turn into an enum and add none
    std::optional<double> SS;
    std::optional<bool> B;
};

struct OptionalContentGroup {
    std::string name;
    // std::string intent;
    //  Usage usage;
};

struct TransparencyGroupExtra {
    // Additional values in transparency group dictionary.
    // std::string cs?
    std::optional<bool> I;
    std::optional<bool> K;
};

struct SubPageNavigation {
    CapyPDF_OptionalContentGroupId id;
    std::optional<Transition> tr;
    // backwards transition
};

struct RasterImageMetadata {
    int32_t w = 0;
    int32_t h = 0;
    int32_t pixel_depth = 8;
    int32_t alpha_depth;
    CAPYPDF_Image_Interpolation interp;
    CapyPDF_Colorspace cs;
    // RI to use for color conversion if needed.
    // CapyPDF_Rendering_Intent ri = CAPY_RI_PERCEPTUAL;
};

struct RasterImage {
    RasterImageMetadata md;
    std::string pixels;
    std::string alpha;
    std::string icc_profile; // std::variant<std::monostate, std::string, CapyPDF_IccColorSpaceId>;
    // CapyPDF_Compression pixel_compression;
    // CapyPDF_Compression alpha_compression;
};

struct jpg_image {
    int32_t w;
    int32_t h;
    std::string file_contents;
};

} // namespace capypdf
