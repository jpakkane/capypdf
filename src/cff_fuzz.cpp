// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <ft_subsetter.hpp>
#include <stdexcept>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t bufsize) {
    if(!buf) {
        return 0;
    }
    try {
        pystd2025::BytesView data((std::byte *)buf, bufsize);
        auto font = capypdf::internal::parse_cff_data(data);
    } catch(const std::exception &) {
    }

    return 0;
}
