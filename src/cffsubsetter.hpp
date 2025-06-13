// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <errorhandling.hpp>
#include <mmapper.hpp>
#include <optional>
#include <vector>

#include <span>
#include <stdint.h>

#include <pystd2025.hpp>

namespace capypdf::internal {

enum class DictOperator : uint16_t {
    Version,
    Notice,
    FullName,
    FamilyName,
    Weight,
    FontBBox,
    BlueValues,
    OtherBlues,
    FamilyBlues,
    FamilyOtherBlues,
    StdHW,
    StdVW,
    Escape,
    UniqueID,
    XUID,
    Charset,
    Encoding,
    CharStrings,
    Private,
    Subrs,
    DefaultWidthX,
    NominalWidthX,

    Copyright = 0x0c00,
    IsFixedPitch,
    ItalicAngle,
    UnderlinePosition,
    UnderlineThickness,
    PaintType,
    CharstringType,
    FontMatrix,
    StrokeWidth,
    BlueScale,
    BlueShift,
    BlueFuzz,
    StemSnapH,
    StemSnapV,
    ForceBold,

    LanguageGroup = 0xc11,
    ExpansionFactor,
    InitialRandomSeed,
    SyntheticBase,
    PostScript,
    BaseFontName,
    BaseFontBlend,

    ROS = 0x0c1e,
    CIDFontVersion,
    CIDFontRevision,
    CIDFontType,
    CIDCount,
    UIDBase,
    FDArray,
    FDSelect,
    FontName,
};

static_assert((uint16_t)DictOperator::NominalWidthX == 0x15);
static_assert((uint16_t)DictOperator::ForceBold == 0xc0e);
static_assert((uint16_t)DictOperator::FontName == 0xc26);

struct CFFHeader {
    uint8_t major;
    uint8_t minor;
    uint8_t hdrsize;
    uint8_t offsize;
};

struct CFFIndex {
    std::vector<pystd2025::BytesView> entries;

    size_t size() const { return entries.size(); }
};

struct CFFDictItem {
    pystd2025::Vector<int32_t> operand;
    DictOperator opr; // "operator" is a reserved word
};

struct CFFDict {
    pystd2025::Vector<CFFDictItem> entries;
};

#pragma pack(push, r1, 1)

struct CFFSelectRange3 {
    uint16_t first;
    uint8_t fd;

    void swap_endian();
};

struct CFFCharsetRange1 {
    uint16_t first;
    uint8_t nLeft;

    void swap_endian();
};

struct CFFCharsetRange2 {
    uint16_t first;
    uint16_t nLeft;

    void swap_endian();
};

#pragma pack(pop, r1)

struct DictOutput {
    pystd2025::Bytes output;
    std::vector<uint16_t> offsets;
};

struct LocalSubrs {
    pystd2025::Bytes data;
    pystd2025::Vector<uint32_t> data_offsets;
};

struct CFFPrivateDict {
    CFFDict entries; // Without substr entry.
    pystd2025::Optional<CFFIndex> subr;
};

struct CFFFontDict {
    CFFDict entries; // without private entry
    std::optional<CFFPrivateDict> priv;
};

struct CFFont {
    DataSource original_data;
    CFFHeader header;
    CFFIndex name;
    CFFIndex top_dict_data;
    CFFDict top_dict;
    CFFIndex string;
    CFFIndex global_subr;
    CFFIndex char_strings;
    pystd2025::Vector<CFFCharsetRange2> charsets;
    CFFPrivateDict pdict;
    pystd2025::Vector<CFFFontDict> fdarray;
    pystd2025::Vector<CFFSelectRange3> fdselect;
    bool is_cid;
    pystd2025::Optional<int32_t> predefined_encoding;
    pystd2025::Optional<uint32_t> predefined_charset;

    uint8_t get_fontdict_id(uint16_t glyph_id) const;
};

rvoe<CFFont> parse_cff_file(const pystd2025::Path &fname);
rvoe<CFFont> parse_cff_data(DataSource original_data);
void append_ros_strings(CFFont &f);

struct SubsetGlyphs {
    uint32_t codepoint; // unicode
    uint16_t gid;
};

struct OffsetPatch {
    uint32_t offset = -1;
    uint32_t value = -1;
};

struct Fixups {
    OffsetPatch charsets;
    OffsetPatch fdselect;
    OffsetPatch charstrings;
    OffsetPatch fdarray;
};

class CFFDictWriter {
public:
    void append_command(const pystd2025::Vector<int32_t> &operands, DictOperator op);
    void append_command(const CFFDictItem &e) { append_command(e.operand, e.opr); };

    DictOutput steal() { return std::move(o); }

    size_t current_size() const { return o.output.size(); }

private:
    DictOutput o;
};

class CFFWriter {
public:
    CFFWriter(const CFFont &source, const std::vector<SubsetGlyphs> &sub);

    void create();

    pystd2025::Bytes steal() { return output; }

private:
    pystd2025::Vector<uint32_t> append_index(const CFFIndex &entries);
    pystd2025::Vector<uint32_t> append_index(const std::vector<pystd2025::Bytes> &entries);
    void append_charset();
    void append_charstrings();
    void append_fdthings();

    void create_topdict();
    void copy_dict_item(CFFDictWriter &w, DictOperator op);

    void patch_offsets();
    void write_fix(const OffsetPatch &p);

    const CFFont &source;
    const std::vector<SubsetGlyphs> &sub;
    pystd2025::Bytes output;
    Fixups fixups;
};

} // namespace capypdf::internal
