// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <cffsubsetter.hpp>
#include <bitfiddling.hpp>
#include <utils.hpp>

#include <bit>

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
    std::vector<std::span<std::byte>> entries;
    ERC(cnt, extract<uint16_t>(dataspan, offset));
    const uint16_t count = std::byteswap(cnt);
    if(count == 0) {
        return entries;
    }
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

const CFFDict *find_command(const CFFont &f, uint16_t op) {
    for(size_t i = 0; i < f.top_dict.size(); ++i) {
        if(f.top_dict[i].opr == op) {
            return &f.top_dict[i];
        }
    }
    return nullptr;
}

rvoe<std::vector<uint16_t>> unpack_charsets(std::span<std::byte> dataspan) {
    std::vector<uint16_t> charset;
    size_t offset = 0;
    const auto format = (uint8_t)dataspan[offset++];
    if(format == 0) {
        RETERR(UnsupportedFormat);
    } else if(format == 1) {
        RETERR(UnsupportedFormat);
    } else {
        ERC(first, extract<uint16_t>(dataspan, offset));
        first = std::byteswap(first);
        offset += 2;
        ERC(nleft, extract<uint16_t>(dataspan, offset));
        nleft = std::byteswap(nleft);
        offset += 2;
        charset.push_back(first);
        for(size_t i = 0; i < nleft; ++i) {
            charset.push_back(first + i + 1);
        }
        // Loop until finished?
        return charset;
    }
}

} // namespace

rvoe<CFFont> parse_cff_span(std::span<std::byte> dataspan) {
    CFFont f;
    ERC(h, extract<CFFHeader>(dataspan, 0));
    if(h.major != 1) {
        RETERR(UnsupportedFormat);
    }
    if(h.minor != 0) {
        RETERR(UnsupportedFormat);
    }
    f.header = h;
    assert(f.header.hdrsize == 4);
    assert(f.header.offsize > 0 || f.header.offsize < 5);
    size_t offset = f.header.hdrsize;
    ERC(name_index, load_index(dataspan, offset));
    f.name = std::move(name_index);
    ERC(topdict_index, load_index(dataspan, offset));
    f.top_dict_data = std::move(topdict_index);
    ERC(tc, unpack_dictionary(f.top_dict_data.front()));
    f.top_dict = std::move(tc);
    ERC(string_index, load_index(dataspan, offset));
    f.string = std::move(string_index);
    ERC(glsub_index, load_index(dataspan, offset));
    f.global_subr = std::move(glsub_index);

    const uint8_t CharStringsOperator = 17;
    auto *cse = find_command(f, CharStringsOperator);
    if(!cse) {
        RETERR(UnsupportedFormat);
    }
    if(cse->operand.size() != 1) {
        RETERR(UnsupportedFormat);
    }
    offset = cse->operand.front();
    ERC(cstring, load_index(dataspan, offset));
    f.char_strings = std::move(cstring);

    const uint8_t EncodingOperator = 16;
    auto *ence = find_command(f, EncodingOperator);
    if(ence) {
        // Not a CID font.
        // Ignore for not, hopefully forever.
        std::abort();
    }
    const uint8_t CharsetOperator = 15;
    auto *cste = find_command(f, CharsetOperator);
    if(!cste) {
        RETERR(UnsupportedFormat);
    }
    assert(cste->operand.size() == 1);
    ERC(charsets, unpack_charsets(dataspan.subspan(cste->operand[0])));
    f.charsets = std::move(charsets);

    const uint8_t PrivateOperator = 18;
    auto *priv = find_command(f, PrivateOperator);
    if(priv) {
        offset = priv->operand[0];
        ERC(pdata, load_index(dataspan, offset));
        ERC(pdict, unpack_dictionary(pdata.front()));
        f.pdict = std::move(pdict);
    }

    const uint16_t FDArrayOperator = 0xc24;
    const uint16_t FDSelectOperator = 0xc25;
    auto *fda = find_command(f, FDArrayOperator);
    auto *fds = find_command(f, FDSelectOperator);
    if(!fda) {
        RETERR(UnsupportedFormat);
    }
    if(!fds) {
        RETERR(UnsupportedFormat);
    }
    offset = fda->operand[0];
    ERC(fdastr, load_index(dataspan, offset))
    for(const auto dstr : fdastr) {
        ERC(dict, unpack_dictionary(dstr));
        f.fontdict.emplace_back(std::move(dict));
    }
    offset = fds->operand[0];
    ERC(fdsstr, load_index(dataspan, offset))
    f.fdselect = std::move(fdsstr);

    return f;
}

rvoe<CFFont> parse_cff_file(const std::filesystem::path &fname) {
    ERC(data, load_file_as_bytes(fname));
    auto dataspan = std::span<std::byte>(data);
    // FIXME, the data is lost here so spans to it become invalid.
    return parse_cff_span(dataspan);
}

} // namespace capypdf::internal
