// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <ft_subsetter.hpp>
#include <string_view>
#include <stdexcept>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t bufsize) {
    if(!buf) {
        return 0;
    }
    try {
        auto font = capypdf::parse_truetype_font(std::string_view((const char *)buf, bufsize));
    } catch(const std::runtime_error &) {
    }

    return 0;
}
