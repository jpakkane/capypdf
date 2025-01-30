// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <errorhandling.hpp>
#include <mmapper.hpp>
#include <filesystem>
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
    Charset = 15,
    Encoding = 16,
    CharStrings = 17,
    Private = 18,
    UnderlinePosition = 0x0c03,
    FontMatrix = 0x0c07,
    ROS = 0x0c1e,
    CIDFontVersion = 0x0c1f,
    CIDFontRevision = 0x0c20,
    CIDFontType = 0x0c21,
    CIDCount = 0x0c22,
    FDArray = 0x0c24,
    FDSelect = 0x0c25,
};

struct CFFHeader {
    uint8_t major;
    uint8_t minor;
    uint8_t hdrsize;
    uint8_t offsize;
};

struct CFFDict {
    std::vector<int32_t> operand;
    DictOperator opr; // "operator" is a reserved word
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

struct CFFont {
    DataSource original_data;
    CFFHeader header;
    std::vector<std::span<std::byte>> name;
    std::vector<std::span<std::byte>> top_dict_data;
    std::vector<CFFDict> top_dict;
    std::vector<std::span<std::byte>> string;
    std::vector<std::span<std::byte>> global_subr;
    std::vector<std::span<std::byte>> char_strings;
    std::vector<CFFCharsetRange2> charsets;
    std::vector<CFFDict> pdict;
    std::vector<std::vector<CFFDict>> fontdict;
    std::vector<CFFSelectRange3> fdselect;
};

rvoe<CFFont> parse_cff_file(const std::filesystem::path &fname);
rvoe<CFFont> parse_cff_data(DataSource original_data);

struct SubsetGlyphs {
    uint32_t codepoint; // unicode
    uint16_t gid;
};

class CFFDictWriter {
public:
    void append_command(const std::vector<int32_t> operands, DictOperator op);

    std::vector<std::byte> steal() { return std::move(output); }

private:
    std::vector<std::byte> output;
};

class CFFWriter {
public:
    CFFWriter(const CFFont &source, const std::vector<SubsetGlyphs> &sub);

    void create();

    std::vector<std::byte> steal() { return std::move(output); }

private:
    void append_index(const std::vector<std::span<std::byte>> &entries);

    void create_topdict();
    void copy_dict_item(CFFDictWriter &w, DictOperator op);

    const CFFont &source;
    const std::vector<SubsetGlyphs> &sub;

    std::vector<std::byte> output;
};

} // namespace capypdf::internal
