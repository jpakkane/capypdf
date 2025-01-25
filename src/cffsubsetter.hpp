// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include "errorhandling.hpp"
#include <filesystem>
#include <vector>

#include <cstdint>

namespace capypdf::internal {

struct CFFHeader {
    uint8_t major;
    uint8_t minor;
    uint8_t hdrsize;
    uint8_t offsize;
};

struct CFFont {
    CFFHeader header;
    std::vector<std::span<std::byte>> name;
    std::vector<std::span<std::byte>> top_dict;
    std::vector<std::span<std::byte>> string;
    std::vector<std::span<std::byte>> global_subr;
};

struct CFFDict {
    std::vector<int32_t> operand;
    uint16_t opr; // "operator" is a reserved word
};

rvoe<CFFont> parse_cff_file(const std::filesystem::path &fname);

} // namespace capypdf::internal
