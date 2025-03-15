// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <ft_subsetter.hpp>
#include <stdexcept>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t bufsize) {
    if(!buf) {
        return 0;
    }
    try {
        std::span<std::byte> data((std::byte *)buf, bufsize);
        auto font = capypdf::internal::parse_truetype_file(data, 0);
    } catch(const std::runtime_error &) {
    }

    return 0;
}
