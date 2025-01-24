// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <cffsubsetter.hpp>
#include <bitfiddling.hpp>
#include <utils.hpp>

namespace capypdf::internal {

rvoe<CFFont> parse_cff_file(const std::filesystem::path &fname) {
    CFFont f;
    ERC(data, load_file_as_bytes(fname));
    ERC(h, extract<CFFHeader>(data, 0));
    if(h.major != 1) {
        RETERR(UnsupportedFormat);
    }
    if(h.minor != 0) {
        RETERR(UnsupportedFormat);
    }
    f.header = h;
    return f;
}

} // namespace capypdf::internal
