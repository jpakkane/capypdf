// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <cffsubsetter.hpp>
#include <bitfiddling.hpp>
#include <utils.hpp>

namespace capypdf::internal {

namespace {

rvoe<size_t> extract_index_offset(std::span<std::byte> dataspan, size_t offset, uint8_t offSize) {
    if(dataspan.size_bytes() <= offset + offSize) {
        RETERR(IndexOutOfBounds);
    }
    if(offSize == 1) {
        const uint8_t b0 = (uint8_t)dataspan[offset];
        return b0;
    } else if(offSize == 2) {
        const uint8_t b1 = (uint8_t)dataspan[offset];
        const uint8_t b0 = (uint8_t)dataspan[offset + 1];
        return b1 << 8 | b0;
    } else if(offSize == 3) {
        const uint8_t b2 = (uint8_t)dataspan[offset];
        const uint8_t b1 = (uint8_t)dataspan[offset + 1];
        const uint8_t b0 = (uint8_t)dataspan[offset + 2];
        return b2 << 16 | b1 << 8 | b0;
    } else {
        std::abort();
    }
}

rvoe<std::vector<std::span<std::byte>>> load_index(std::span<std::byte> dataspan, size_t &offset) {
    ERC(cnt, extract<uint16_t>(dataspan, offset));
    const uint16_t count = std::byteswap(cnt);
    offset += sizeof(uint16_t);
    ERC(offSize, extract<uint8_t>(dataspan, offset));
    ++offset;
    std::vector<size_t> offsets;
    offsets.reserve(count + 1);
    assert(offSize <= 5);
    for(uint16_t i = 0; i <= count; ++i) {
        ERC(c, extract_index_offset(dataspan, offset, offSize));
        assert(c > 0);
        if(!offsets.empty()) {
            assert(offsets.back() < c);
        }
        offsets.push_back(c);
        offset += offSize;
    }
    --offset;
    std::vector<std::span<std::byte>> entries;
    entries.reserve(count);

    for(uint16_t i = 0; i < count; ++i) {
        const auto entrysize = offsets[i + 1] - offsets[i];
        auto entry = dataspan.subspan(offset + offsets[i], entrysize);
        entries.emplace_back(entry);
    }
    offset += offsets.back();
    return entries;
}

} // namespace

rvoe<CFFont> parse_cff_file(const std::filesystem::path &fname) {
    CFFont f;
    ERC(data, load_file_as_bytes(fname));
    auto dataspan = std::span<std::byte>(data);
    ERC(h, extract<CFFHeader>(dataspan, 0));
    if(h.major != 1) {
        RETERR(UnsupportedFormat);
    }
    if(h.minor != 0) {
        RETERR(UnsupportedFormat);
    }
    f.header = h;
    assert(f.header.hdrsize == 4);
    assert(f.header.offsize == 3);
    size_t offset = f.header.hdrsize;
    ERC(name_index, load_index(dataspan, offset));
    f.name = std::move(name_index);
    return f;
}

} // namespace capypdf::internal
