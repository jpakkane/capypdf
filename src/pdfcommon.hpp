// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#pragma once

#include <capypdf.h>
#include <errorhandling.hpp>

#include <variant>
#include <iterator>
#include <pystd2025.hpp>
#include <math.h>

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
    };                                                                                             \
    template<typename Hasher> struct pystd2025::HashFeeder<Hasher, TNAME> {                        \
        void operator()(Hasher &h, const TNAME &fid) noexcept { h.feed_hash(fid.id); }             \
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

template<typename Hasher> struct pystd2025::HashFeeder<Hasher, CapyPDF_Builtin_Fonts> {
    void operator()(Hasher &h, const CapyPDF_Builtin_Fonts &builtin) noexcept {
        h.feed_hash((int32_t)builtin);
    }
};

namespace capypdf::internal {

// If at all possible, never expose this in the public API
// Instead have the user specify some higher level version,
// like PDF/A or PDF/X and set this based on that.
enum class PdfVersion : uint8_t { v17, v20 };

class ObjectFormatter;

extern const pystd2025::Span<const char *> structure_type_names;
extern const pystd2025::Span<const char *> colorspace_names;
extern const pystd2025::Span<const char *> rendering_intent_names;

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
    pystd2025::Optional<CharInfo> char_info;
};

class asciistring {
public:
    asciistring() = default;
    asciistring(asciistring &&o) = default;
    asciistring(const asciistring &o) = default;

    pystd2025::CStringView sv() const { return buf.view(); }
    const char *c_str() const { return buf.c_str(); }

    static rvoe<asciistring> from_cstr(const char *cstr);
    static rvoe<asciistring> from_cstr(const pystd2025::CString &str) {
        return asciistring::from_view(str.view());
    }
    static rvoe<asciistring> from_view(pystd2025::CStringView sv);
    static rvoe<asciistring> from_view(const char *buf, uint32_t bufsize) {
        return asciistring::from_view(pystd2025::CStringView(buf, bufsize));
    }
    bool empty() const { return buf.is_empty(); }

    asciistring &operator=(asciistring &&o) = default;
    asciistring &operator=(const asciistring &o) {
        asciistring tmp(o);
        (*this) = pystd2025::move(tmp);
        return *this;
    }

    bool operator==(const asciistring &o) const = default;

    size_t size() const { return buf.size(); }

private:
    explicit asciistring(pystd2025::CStringView prevalidated_ascii) : buf(prevalidated_ascii) {}
    pystd2025::CString buf;
};

class u8string {
public:
    u8string() = default;
    u8string(u8string &&o) = default;
    u8string(const u8string &o) = default;

    pystd2025::CStringView sv() const { return buf.view(); }

    static rvoe<u8string> from_cstr(const char *cstr);
    static rvoe<u8string> from_cstr(const pystd2025::CString &str) {
        return u8string::from_cstr(str.c_str());
    }

    static rvoe<u8string> from_view(pystd2025::CStringView sv);
    static rvoe<u8string> from_view(const char *buf, uint32_t bufsize) {
        return u8string::from_view(pystd2025::CStringView(buf, bufsize));
    }

    bool empty() const { return buf.is_empty(); }
    size_t length() const { return buf.size(); }
    size_t size() const { return buf.size(); }
    const char *c_str() const { return buf.c_str(); }

    CodepointIterator begin() const {
        return CodepointIterator((const unsigned char *)buf.c_str());
    }

    CodepointIterator end() const {
        return CodepointIterator((const unsigned char *)buf.c_str() + buf.size());
    }

    u8string &operator=(u8string &&o) = default;
    u8string &operator=(const u8string &o) {
        u8string tmp(o);
        (*this) = pystd2025::move(tmp);
        return *this;
    }

    bool operator==(const u8string &other) const = default;

private:
    explicit u8string(pystd2025::CStringView prevalidated_utf8) : buf(prevalidated_utf8) {}
    pystd2025::CString buf;
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
    LimitDouble() noexcept : value(minval) {}
    LimitDouble(LimitDouble &&o) noexcept : value(o.value) {}
    LimitDouble(const LimitDouble &o) noexcept : value(o.value) {}

    // No "explicit" because we want the following to work for convenience:
    // DeviceRGBColor{0.0, 0.3, 1.0}
    LimitDouble(double new_val) noexcept : value(new_val) { clamp(); }

    double v() const { return value; }

    LimitDouble &operator=(double d) {
        value = d;
        clamp();
        return *this;
    }

    LimitDouble &operator=(LimitDouble &&o) noexcept {
        value = o.value;
        return *this;
    }

    LimitDouble &operator=(const LimitDouble &o) noexcept {
        value = o.value;
        return *this;
    }

private:
    constexpr static double maxval = 1.0;
    constexpr static double minval = 0.0;

    void clamp() {
        if(isnan(value)) {
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
    pystd2025::Variant<pystd2025::CString, pystd2025::Bytes> storage;

public:
    RawData() noexcept : storage{} {}
    explicit RawData(pystd2025::CString input);
    explicit RawData(pystd2025::Bytes input);
    explicit RawData(pystd2025::CStringView input);
    explicit RawData(pystd2025::BytesView input);

    const char *data() const;
    size_t size() const;

    pystd2025::CStringView sv() const;
    pystd2025::BytesView span() const;

    bool empty() const;
    void clear();

    void assign(const char *buf, size_t bufsize);

    RawData &operator=(pystd2025::CString input);
    RawData &operator=(pystd2025::Bytes input);

    bool operator==(pystd2025::CStringView other) const;
    bool operator==(pystd2025::BytesView other) const;
};

// Every resource type has its own id type to avoid
// accidentally mixing them up.

struct PageId {
    int32_t id;
};

// Named and ordered according to PDF spec 2.0 section 8.4.5, table 57
struct GraphicsState {
    pystd2025::Optional<double> LW;
    pystd2025::Optional<CapyPDF_Line_Cap> LC;
    pystd2025::Optional<CapyPDF_Line_Join> LJ;
    pystd2025::Optional<double> ML;
    // pystd2025::Optional<DashArray> D;
    pystd2025::Optional<CapyPDF_Rendering_Intent> RI;
    pystd2025::Optional<bool> OP;
    pystd2025::Optional<bool> op;
    pystd2025::Optional<int32_t> OPM;
    // pystd2025::Optional<FontSomething> Font;
    // pystd2025::Optional<pystd2025::CString> BG;
    // pystd2025::Optional<pystd2025::CString> BG2;
    // pystd2025::Optional<pystd2025::CString> UCR;
    // pystd2025::Optional<pystd2025::CString> UCR2;
    // pystd2025::Optional<pystd2025::CString> TR;
    // pystd2025::Optional<pystd2025::CString> TR2;
    // pystd2025::Optional<str::string> HT;
    pystd2025::Optional<double> FL;
    pystd2025::Optional<double> SM;
    pystd2025::Optional<bool> SA;
    pystd2025::Optional<CapyPDF_Blend_Mode> BM;
    pystd2025::Optional<CapyPDF_SoftMaskId> SMask;
    pystd2025::Optional<LimitDouble> CA;
    pystd2025::Optional<LimitDouble> ca;
    pystd2025::Optional<bool> AIS;
    pystd2025::Optional<bool> TK;
    // pystd2025::Optional<CapyPDF_BlackPointComp> UseBlackPtComp;
    //  pystd2025::Optional<Point> HTO;
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
    pystd2025::Vector<double> values;
};

struct SeparationColor {
    CapyPDF_SeparationId id;
    LimitDouble v;
};

typedef pystd2025::Variant<DeviceRGBColor,
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
    pystd2025::Vector<double> domain;
    Color C0;
    Color C1;
    double n;
};

// Multiple FunctionType2's
struct FunctionType3 {
    pystd2025::Vector<double> domain;
    pystd2025::Vector<CapyPDF_FunctionId> functions;
    pystd2025::Vector<double> bounds;
    pystd2025::Vector<double> encode;
};

struct FunctionType4 {
    pystd2025::Vector<double> domain;
    pystd2025::Vector<double> range;
    pystd2025::CString code;
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
    pystd2025::Optional<ShadingExtend> extend{};
    pystd2025::Optional<ShadingDomain> domain{};
};

// Radial
struct ShadingType3 {
    CapyPDF_Device_Colorspace colorspace;
    double x0, y0, r0, x1, y1, r1;
    CapyPDF_FunctionId function;
    pystd2025::Optional<ShadingExtend> extend{};
    pystd2025::Optional<ShadingDomain> domain{};
};

struct ShadingPattern {
    CapyPDF_ShadingId sid;
    pystd2025::Optional<PdfMatrix> m;
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
    pystd2025::Vector<ShadingElement> elements;
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
    Point p[12];
    Color c[4];
};

struct ContinuationCoonsPatch {
    int flag;
    Point p[8];
    Color c[2];
};

typedef std::variant<FullCoonsPatch, ContinuationCoonsPatch> CoonsPatches;

struct ShadingType6 {
    pystd2025::Vector<CoonsPatches> elements;
    double minx = 0;
    double miny = 0;
    double maxx = 200;
    double maxy = 200;
    CapyPDF_Device_Colorspace colorspace = CAPY_DEVICE_CS_RGB;
};

typedef std::variant<ShadingType2, ShadingType3, ShadingType4, ShadingType6> PdfShading;

struct TextStateParameters {
    pystd2025::Optional<double> char_spacing;
    pystd2025::Optional<double> word_spacing;
    pystd2025::Optional<double> horizontal_scaling;
    pystd2025::Optional<double> leading;
    pystd2025::Optional<CapyPDF_Text_Mode> render_mode;
    pystd2025::Optional<double> rise;
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
    pystd2025::Optional<CapyPDF_Transition_Type> type;
    pystd2025::Optional<double> duration;
    pystd2025::Optional<CapyPDF_Transition_Dimension> Dm; // true is horizontal
    pystd2025::Optional<CapyPDF_Transition_Motion> M;     // true is inward
    pystd2025::Optional<int32_t> Di;                      // FIXME, turn into an enum and add none
    pystd2025::Optional<double> SS;
    pystd2025::Optional<bool> B;
};

struct OptionalContentGroup {
    pystd2025::CString name;
    // pystd2025::CString intent;
    //  Usage usage;
};

struct TransparencyGroupProperties {
    // This should eventually be a variant of some sort,
    // because the mixing colorspace can be an ICC one.
    pystd2025::Optional<CapyPDF_Device_Colorspace> CS;
    pystd2025::Optional<bool> I;
    pystd2025::Optional<bool> K;

    void serialize(ObjectFormatter &fmt) const;
};

struct SoftMask {
    CapyPDF_Soft_Mask_Subtype S;
    CapyPDF_TransparencyGroupId G;
    // pystd2025::Vector<double> BC;
    // TransferFunctionId TR;
};

struct SubPageNavigation {
    CapyPDF_OptionalContentGroupId id;
    pystd2025::Optional<Transition> tr;
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
    pystd2025::Bytes pixels;
    pystd2025::Bytes alpha;
    pystd2025::Bytes icc_profile;
};

struct jpg_image {
    uint32_t w;
    uint32_t h;
    uint32_t depth;
    CapyPDF_Device_Colorspace cs;
    pystd2025::Vector<double> domain;
    pystd2025::Bytes file_contents;
    pystd2025::Bytes icc_profile;
};

typedef std::variant<RawPixelImage, jpg_image> RasterImage;

struct ImagePDFProperties {
    CapyPDF_Image_Interpolation interp = CAPY_INTERPOLATION_AUTO;
    bool as_mask = false;
};

struct DestinationXYZ {
    pystd2025::Optional<double> x;
    pystd2025::Optional<double> y;
    pystd2025::Optional<double> z;
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

typedef pystd2025::HashMap<asciistring, asciistring> BDCTags;

struct FontProperties {
    uint16_t subfont = 0;
    // Add other font properties here like:
    //
    // - OpenType alternatives to use
    // - Vertical or Horizontal writing
    // - Other CID metadata at al
};

} // namespace capypdf::internal
/*
template<> struct std::hash<capypdf::internal::asciistring> {
    std::size_t operator()(const capypdf::internal::asciistring &astr) const noexcept {
        return std::hash<pystd2025::CStringView>{}(astr.sv());
    }
};

template<> struct std::hash<capypdf::internal::u8string> {
    std::size_t operator()(const capypdf::internal::u8string &u8str) const noexcept {
        return std::hash<pystd2025::CStringView>{}(u8str.sv());
    }
};
*/
template<typename Hasher> struct pystd2025::HashFeeder<Hasher, capypdf::internal::asciistring> {
    void operator()(Hasher &h, const capypdf::internal::asciistring &astr) noexcept {
        h.feed_bytes(astr.c_str(), astr.size());
    }
};
