// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <errorhandling.hpp>
#include <mmapper.hpp>
#include <filesystem>
#include <optional>
#include <vector>

#include <span>
#include <cstdint>

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
    std::vector<std::span<std::byte>> entries;

    size_t size() const { return entries.size(); }
};

struct CFFDictItem {
    std::vector<int32_t> operand;
    DictOperator opr; // "operator" is a reserved word
};

struct CFFDict {
    std::vector<CFFDictItem> entries;
};

#pragma pack(push, r1, 1)

struct CFFSelectRange3 {
    uint16_t first;
    uint8_t fd;

    void swap_endian();
};

struct CFFCharsetRange2 {
    uint16_t first;
    uint16_t nLeft;

    void swap_endian();
};

#pragma pack(pop, r1)

struct DictOutput {
    std::vector<std::byte> output;
    std::vector<uint16_t> offsets;
};

struct LocalSubrs {
    std::vector<std::byte> data;
    std::vector<uint32_t> data_offsets;
};

struct CFFPrivateDict {
    CFFDict entries; // Without substr entry.
    std::optional<CFFIndex> subr;
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
    std::vector<CFFCharsetRange2> charsets;
    CFFDict pdict;
    std::vector<CFFFontDict> fdarray;
    //    std::vector<CFFDict> fontdict;
    //    std::vector<CFFIndex> local_subrs;
    std::vector<CFFSelectRange3> fdselect;

    uint8_t get_fontdict_id(uint16_t glyph_id) const;
};

rvoe<CFFont> parse_cff_file(const std::filesystem::path &fname);
rvoe<CFFont> parse_cff_data(DataSource original_data);

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
    void append_command(const std::vector<int32_t> operands, DictOperator op);
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

    std::vector<std::byte> steal() { return std::move(output); }

private:
    std::vector<uint32_t> append_index(const CFFIndex &entries);
    std::vector<uint32_t> append_index(const std::vector<std::vector<std::byte>> &entries);
    void append_charset();
    void append_charstrings();
    void append_fdthings();

    void create_topdict();
    void copy_dict_item(CFFDictWriter &w, DictOperator op);

    void patch_offsets();
    void write_fix(const OffsetPatch &p);

    const CFFont &source;
    const std::vector<SubsetGlyphs> &sub;
    std::vector<std::byte> output;
    Fixups fixups;
};

} // namespace capypdf::internal
