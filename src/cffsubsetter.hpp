// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include "errorhandling.hpp"
#include <filesystem>

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
};

rvoe<CFFont> parse_cff_file(const std::filesystem::path &fname);

} // namespace capypdf::internal
