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
        ERC(v, extract<uint16_t>(dataspan, offset))
        return std::byteswap(v);
    } else if(offSize == 3) {
        const uint8_t b2 = (uint8_t)dataspan[offset];
        const uint8_t b1 = (uint8_t)dataspan[offset + 1];
        const uint8_t b0 = (uint8_t)dataspan[offset + 2];
        return b2 << 16 | b1 << 8 | b0;
    } else if(offSize == 4) {
        ERC(v, extract<uint32_t>(dataspan, offset))
        return std::byteswap(v);
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

rvoe<std::vector<CFFDict>> unpack_dictionary(std::span<std::byte> dataspan) {
    std::vector<CFFDict> dict;
    size_t offset = 0;
    std::vector<int32_t> operands;
    while(offset < dataspan.size_bytes()) {
        // Read operand
        const int32_t b0 = (uint8_t)dataspan[offset++];
        if(b0 <= 21) {
            // Is an operator.
            int32_t unpacked_operator = b0;
            if(b0 == 0xc) {
                const int32_t b1 = (uint8_t)dataspan[offset++];
                unpacked_operator = b0 << 8 | b1;
            }
            dict.emplace_back(std::move(operands), unpacked_operator);
            operands.clear();
        } else if((b0 >= 28 && b0 <= 30) || (b0 >= 32 && b0 <= 254)) {
            // Is an operand.
            int32_t unpacked_operand;
            if(b0 >= 32 && b0 <= 246) {
                unpacked_operand = b0 - 139;
            } else if(b0 >= 247 && b0 <= 250) {
                const int32_t b1 = (uint8_t)dataspan[offset++];
                unpacked_operand = (b0 - 247) * 256 + b1 + 108;
            } else if(b0 >= 251 && b0 <= 254) {
                const int32_t b1 = (uint8_t)dataspan[offset++];
                unpacked_operand = -(b0 - 251) * 256 - b1 - 108;
            } else if(b0 == 28) {
                const int32_t b1 = (uint8_t)dataspan[offset++];
                const int32_t b2 = (uint8_t)dataspan[offset++];
                unpacked_operand = b1 << 8 | b2;
            } else if(b0 == 29) {
                const int32_t b1 = (uint8_t)dataspan[offset++];
                const int32_t b2 = (uint8_t)dataspan[offset++];
                const int32_t b3 = (uint8_t)dataspan[offset++];
                const int32_t b4 = (uint8_t)dataspan[offset++];
                unpacked_operand = b1 << 24 | b2 << 16 | b3 << 8 | b4;
            } else if(b0 == 30) {
                // Floating point.
                uint8_t b1;
                do {
                    b1 = (uint8_t)dataspan[offset++];
                } while((b1 & 0xf) != 0xf);
                unpacked_operand = -1; // FIXME
            } else {
                std::abort();
            }
            operands.push_back(unpacked_operand);
        }
    }
    assert(operands.empty());
    return dict;
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
    ERC(topdict_index, load_index(dataspan, offset));
    f.top_dict = std::move(topdict_index);
    ERC(string_index, load_index(dataspan, offset));
    f.string = std::move(string_index);
    ERC(glsub_index, load_index(dataspan, offset));
    f.global_subr = std::move(glsub_index);

    auto res = unpack_dictionary(f.top_dict.front()).value();

    return f;
}

} // namespace capypdf::internal
