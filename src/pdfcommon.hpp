// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

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
#include <iterator>
#include <span>

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

namespace capypdf::internal {

// If at all possible, never expose this in the public API
// Instead have the user specify some higher level version,
// like PDF/A or PDF/X and set this based on that.
enum class PdfVersion : uint8_t { v13, v14, v15, v16, v17, v20 };

class ObjectFormatter;

extern const std::array<const char *, (int)CAPY_STRUCTURE_TYPE_NUM_ITEMS> structure_type_names;
extern const std::array<const char *, 3> colorspace_names;
extern const std::array<const char *, 4> rendering_intent_names;

// Does not check if the given buffer is valid UTF-8.
// If it is not, UB ensues.
class CodepointIterator {
public:
    struct CharInfo {
        uint32_t codepoint;
        uint32_t byte_count;
    };

    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = uint32_t;
    using pointer = const uint32_t *;
    using reference = const uint32_t &;

    CodepointIterator(const unsigned char *utf8_string) : buf{utf8_string} {}
    CodepointIterator(const CodepointIterator &) = default;
    CodepointIterator(CodepointIterator &&) = default;

    reference operator*() {
        compute_char_info();
        return char_info.value().codepoint;
    }

    pointer operator->() {
        compute_char_info();
        return &char_info.value().codepoint;
    }
    CodepointIterator &operator++() {
        compute_char_info();
        buf += char_info.value().byte_count;
        char_info.reset();
        return *this;
    }
    CodepointIterator operator++(int) {
        compute_char_info();
        CodepointIterator rval{*this};
        ++(*this);
        return rval;
    }

    bool operator==(const CodepointIterator &o) const { return buf == o.buf; }
    bool operator!=(const CodepointIterator &o) const { return !(*this == o); }

private:
    void compute_char_info() {
        if(char_info) {
            return;
        }
        char_info = extract_one_codepoint(buf);
    };

    CharInfo extract_one_codepoint(const unsigned char *buf);
    const unsigned char *buf;
    std::optional<CharInfo> char_info;
};

class asciistring {
public:
    asciistring() = default;
    asciistring(asciistring &&o) = default;
    asciistring(const asciistring &o) = default;

    std::string_view sv() const { return buf; }
    const char *c_str() const { return buf.c_str(); }

    static rvoe<asciistring> from_cstr(const char *cstr);
    static rvoe<asciistring> from_cstr(const std::string &str) {
        return asciistring::from_view(std::string_view(str));
    }
    static rvoe<asciistring> from_view(std::string_view sv);
    static rvoe<asciistring> from_view(const char *buf, uint32_t bufsize) {
        return asciistring::from_view(std::string_view(buf, bufsize));
    }
    bool empty() const { return buf.empty(); }

    asciistring &operator=(asciistring &&o) = default;
    asciistring &operator=(const asciistring &o) = default;

    bool operator==(const asciistring &o) const = default;

private:
    explicit asciistring(std::string_view prevalidated_ascii) : buf(prevalidated_ascii) {}
    std::string buf;
};

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

    static rvoe<u8string> from_view(std::string_view sv);
    static rvoe<u8string> from_view(const char *buf, uint32_t bufsize) {
        return u8string::from_view(std::string_view(buf, bufsize));
    }

    bool empty() const { return buf.empty(); }
    size_t length() const { return buf.length(); }
    size_t size() const { return buf.size(); }
    const char *c_str() const { return buf.c_str(); }

    CodepointIterator begin() const {
        return CodepointIterator((const unsigned char *)buf.c_str());
    }

    CodepointIterator end() const {
        return CodepointIterator((const unsigned char *)buf.c_str() + buf.size());
    }

    u8string &operator=(u8string &&o) = default;
    u8string &operator=(const u8string &o) = default;

    bool operator==(const u8string &other) const = default;

private:
    explicit u8string(std::string_view prevalidated_utf8) : buf(prevalidated_utf8) {}
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

    static rvoe<PdfRectangle> construct(double l, double b, double r, double t);

    double w() const { return y2 - y1; }
    double h() const { return x2 - x1; }
};

// In PDF only 6 of the 9 values are stored.
struct PdfMatrix {
    double a, b, c, d, e, f;
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

class RawData {
private:
    std::variant<std::string, std::vector<std::byte>> storage;

public:
    RawData() : storage{} {}
    explicit RawData(std::string input);
    explicit RawData(std::vector<std::byte> input);
    explicit RawData(std::string_view input);
    explicit RawData(std::span<std::byte> input);

    const char *data() const;
    size_t size() const;

    std::string_view sv() const;
    std::span<std::byte> span() const;

    bool empty() const;
    void clear();

    void assign(const char *buf, size_t bufsize);
    void assign(const std::byte *buf, size_t bufsize);

    RawData &operator=(std::string input);
    RawData &operator=(std::vector<std::byte> input);

    bool operator==(std::string_view other) const;
    bool operator==(std::span<std::byte> other) const;
};

// Every resource type has its own id type to avoid
// accidentally mixing them up.

struct PageId {
    int32_t id;
};

struct HalftoneDefault {};

struct HalftoneType1 {
    double frequency;
    double angle;
    CapyPDF_Halftone_Spot_Function spot;
};

typedef std::variant<HalftoneDefault, HalftoneType1> Halftone;

// Named and ordered according to PDF spec 2.0 section 8.4.5, table 57
struct GraphicsState {
    std::optional<double> LW;
    std::optional<CapyPDF_Line_Cap> LC;
    std::optional<CapyPDF_Line_Join> LJ;
    std::optional<double> ML;
    // std::optional<DashArray> D;
    std::optional<CapyPDF_Rendering_Intent> RI;
    std::optional<bool> OP;
    std::optional<bool> op;
    std::optional<int32_t> OPM;
    // std::optional<FontSomething> Font;
    // std::optional<std::string> BG;
    // std::optional<std::string> BG2;
    // std::optional<std::string> UCR;
    // std::optional<std::string> UCR2;
    // std::optional<std::string> TR;
    // std::optional<std::string> TR2;
    std::optional<Halftone> HT;
    std::optional<double> FL;
    std::optional<double> SM;
    std::optional<bool> SA;
    std::optional<CapyPDF_Blend_Mode> BM;
    std::optional<CapyPDF_SoftMaskId> SMask;
    std::optional<LimitDouble> CA;
    std::optional<LimitDouble> ca;
    std::optional<bool> AIS;
    std::optional<bool> TK;
    // std::optional<CapyPDF_BlackPointComp> UseBlackPtComp;
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
    CapyPDF_LabColorSpaceId id;
    double l;
    double a;
    double b;
};

struct ICCColor {
    CapyPDF_IccColorSpaceId id;
    std::vector<double> values;
};

struct SeparationColor {
    CapyPDF_SeparationId id;
    LimitDouble v;
};

typedef std::variant<DeviceRGBColor,
                     DeviceGrayColor,
                     DeviceCMYKColor,
                     ICCColor,
                     LabColor,
                     SeparationColor,
                     CapyPDF_PatternId>
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

// Multiple FunctionType2's
struct FunctionType3 {
    std::vector<double> domain;
    std::vector<CapyPDF_FunctionId> functions;
    std::vector<double> bounds;
    std::vector<double> encode;
};

struct FunctionType4 {
    std::vector<double> domain;
    std::vector<double> range;
    std::string code;
};

typedef std::variant<FunctionType2, FunctionType3, FunctionType4> PdfFunction;

struct ShadingExtend {
    bool starting;
    bool ending;
};

struct ShadingDomain {
    double starting;
    double ending;
};

// Linear
struct ShadingType2 {
    CapyPDF_Device_Colorspace colorspace;
    double x0, y0, x1, y1;
    CapyPDF_FunctionId function;
    std::optional<ShadingExtend> extend{};
    std::optional<ShadingDomain> domain{};
};

// Radial
struct ShadingType3 {
    CapyPDF_Device_Colorspace colorspace;
    double x0, y0, r0, x1, y1, r1;
    CapyPDF_FunctionId function;
    std::optional<ShadingExtend> extend{};
    std::optional<ShadingDomain> domain{};
};

struct ShadingPattern {
    CapyPDF_ShadingId sid;
    std::optional<PdfMatrix> m;
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
    CapyPDF_Device_Colorspace colorspace = CAPY_DEVICE_CS_RGB;

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
    std::array<Color, 4> c;
};

struct ContinuationCoonsPatch {
    int flag;
    std::array<Point, 8> p;
    std::array<Color, 2> c;
};

typedef std::variant<FullCoonsPatch, ContinuationCoonsPatch> CoonsPatches;

struct ShadingType6 {
    std::vector<CoonsPatches> elements;
    double minx = 0;
    double miny = 0;
    double maxx = 200;
    double maxy = 200;
    CapyPDF_Device_Colorspace colorspace = CAPY_DEVICE_CS_RGB;
};

typedef std::variant<ShadingType2, ShadingType3, ShadingType4, ShadingType6> PdfShading;

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

struct Transition {
    std::optional<CapyPDF_Transition_Type> type;
    std::optional<double> duration;
    std::optional<CapyPDF_Transition_Dimension> Dm; // true is horizontal
    std::optional<CapyPDF_Transition_Motion> M;     // true is inward
    std::optional<int32_t> Di;                      // FIXME, turn into an enum and add none
    std::optional<double> SS;
    std::optional<bool> B;
};

struct OptionalContentGroup {
    std::string name;
    // std::string intent;
    //  Usage usage;
};

struct TransparencyGroupProperties {
    // This should eventually be a variant of some sort,
    // because the mixing colorspace can be an ICC one.
    std::optional<CapyPDF_Device_Colorspace> CS;
    std::optional<bool> I;
    std::optional<bool> K;

    void serialize(ObjectFormatter &fmt) const;
};

struct SoftMask {
    CapyPDF_Soft_Mask_Subtype S;
    CapyPDF_TransparencyGroupId G;
    // std::vector<double> BC;
    // TransferFunctionId TR;
};

struct SubPageNavigation {
    CapyPDF_OptionalContentGroupId id;
    std::optional<Transition> tr;
    // backwards transition
};

struct RasterImageMetadata {
    uint32_t w = 0;
    uint32_t h = 0;
    uint32_t pixel_depth = 8;
    uint32_t alpha_depth = 0;
    CapyPDF_Image_Colorspace cs = CAPY_IMAGE_CS_RGB;
    CapyPDF_Compression compression = CAPY_COMPRESSION_NONE;
};

struct RawPixelImage {
    RasterImageMetadata md;
    std::vector<std::byte> pixels;
    std::vector<std::byte> alpha;
    std::vector<std::byte> icc_profile;
    std::vector<double> decode;
};

struct jpg_image {
    uint32_t w;
    uint32_t h;
    uint32_t depth;
    CapyPDF_Device_Colorspace cs;
    std::vector<double> decode;
    std::vector<std::byte> file_contents;
    std::vector<std::byte> icc_profile;
};

typedef std::variant<RawPixelImage, jpg_image> RasterImage;

struct ImagePDFProperties {
    CapyPDF_Image_Interpolation interp = CAPY_INTERPOLATION_AUTO;
    bool as_mask = false;
};

struct DestinationXYZ {
    std::optional<double> x;
    std::optional<double> y;
    std::optional<double> z;
};

struct DestinationFit {};

struct DestinationFitR {
    double left, bottom, top, right;
};

typedef std::variant<DestinationXYZ, DestinationFit, DestinationFitR> DestinationType;

struct Destination {
    int32_t page;
    DestinationType loc;
};

typedef std::unordered_map<asciistring, asciistring> BDCTags;

struct FontProperties {
    uint16_t subfont = 0;
    // Add other font properties here like:
    //
    // - OpenType alternatives to use
    // - Vertical or Horizontal writing
    // - Other CID metadata at al
};

} // namespace capypdf::internal

template<> struct std::hash<capypdf::internal::asciistring> {
    std::size_t operator()(const capypdf::internal::asciistring &astr) const noexcept {
        return std::hash<std::string_view>{}(astr.sv());
    }
};

template<> struct std::hash<capypdf::internal::u8string> {
    std::size_t operator()(const capypdf::internal::u8string &u8str) const noexcept {
        return std::hash<std::string_view>{}(u8str.sv());
    }
};
