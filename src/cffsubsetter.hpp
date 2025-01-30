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

struct CFFHeader {
    uint8_t major;
    uint8_t minor;
    uint8_t hdrsize;
    uint8_t offsize;
};

struct CFFDict {
    std::vector<int32_t> operand;
    uint16_t opr; // "operator" is a reserved word
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

} // namespace capypdf::internal
