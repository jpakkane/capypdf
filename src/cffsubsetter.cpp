// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <cffsubsetter.hpp>
#include <bitfiddling.hpp>
#include <utils.hpp>

#include <bit>

// clang-format off

/*
 * A subset CFF font created from Noto Serif CJK with one glyph contains the following:
 *
 * Subset top dict entries

 * 3102  1e ROS
 * 1     notice
 * 2     fullname
 * 3     familyname
 * 4     weight
 * 5     fontbbox
 * 3103  1f CIDFontVersion
 * 3106  20 CIDFontRevision
 * 3108  22 CIDCount
 * 3109  25 FDSelect
 * 15    charset
 * 17    charstrings
 * 3075  03 UnderlinePosition
 *
 * Strings:
 *
 * Adobe
 * Identity
 * Copyright 2014-2021 Adobe (http://www.adobe.com/). Noto is a trademark of Google Inc.
 * Noto Sans CJK JP Regular
 * Noto Sans CJK JP
 * NotoSansCJKjp-Regular-Generic
 * NotoSansCJKjp-Regular-Ideographs
 *
 */

// clang-format on

namespace capypdf::internal {

namespace {

const int32_t NUM_STANDARD_STRINGS = 390;

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
    const uint32_t count = std::byteswap(cnt);
    if(count == 0) {
        return entries;
    }
    offset += sizeof(uint16_t);
    ERC(offSize, extract<uint8_t>(dataspan, offset));
    ++offset;
    std::vector<size_t> offsets;
    offsets.reserve(count + 1);
    assert(offSize <= 5);
    for(uint32_t i = 0; i <= count; ++i) {
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
            dict.emplace_back(std::move(operands), (DictOperator)unpacked_operator);
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

const CFFDict *find_command(const CFFont &f, DictOperator op) {
    for(size_t i = 0; i < f.top_dict.size(); ++i) {
        if(f.top_dict[i].opr == op) {
            return &f.top_dict[i];
        }
    }
    return nullptr;
}

rvoe<std::vector<CFFCharsetRange2>> unpack_charsets(std::span<std::byte> dataspan) {
    std::vector<CFFCharsetRange2> charset;
    size_t offset = 0;
    const auto format = (uint8_t)dataspan[offset++];
    if(format == 0) {
        RETERR(UnsupportedFormat);
    } else if(format == 1) {
        RETERR(UnsupportedFormat);
    } else {
        ERC(rng, extract<CFFCharsetRange2>(dataspan, offset));
        rng.swap_endian();
        charset.push_back(rng);
        return charset;
    }
}

rvoe<std::vector<CFFSelectRange3>> unpack_fdselect(std::span<std::byte> dataspan,
                                                   uint32_t num_glyphs) {
    std::vector<CFFSelectRange3> ranges;
    size_t offset = 0;
    const uint8_t format = (uint8_t)dataspan[offset++];
    if(format == 0) {
        std::vector<std::byte> selector_array{dataspan.begin() + offset,
                                              dataspan.begin() + offset + num_glyphs};
        // FIXME. OR maybe not. This seems to be used in generated
        // subset fonts only.
        return ranges;
    } else if(format == 3) {
        ERC(nRg, extract_and_swap<uint16_t>(dataspan, offset));
        const uint32_t nRanges = nRg;
        offset += sizeof(uint16_t);
        ranges.reserve(nRanges);
        for(uint32_t i = 0; i < nRanges; ++i) {
            ERC(range, extract<CFFSelectRange3>(dataspan, offset));
            offset += sizeof(range);
            range.swap_endian();
            ranges.push_back(range);
        }
        ERC(sentinel, extract_and_swap<uint16_t>(dataspan, offset));
        if(sentinel != num_glyphs) {
            RETERR(MalformedFontFile);
        }
    }
    return ranges;
}

void print_string(const CFFont &cff, int32_t string_number) {
    if(string_number < NUM_STANDARD_STRINGS) {
        printf("<standard string %d>\n", string_number);
    } else {
        const uint32_t strindex = string_number - NUM_STANDARD_STRINGS;
        auto sv = span2sv(cff.string.at(strindex));
        std::string tmp{sv};
        printf("%s\n", tmp.c_str());
    }
}

void print_string_for_operator(const CFFont &cff, DictOperator op) {
    auto *tmp = find_command(cff, op);
    assert(tmp->operand.size() == 1);
    print_string(cff, tmp->operand[0]);
}

void print_info(const CFFont &cff) {
    printf("Font name: ");
    print_string_for_operator(cff, DictOperator::FullName);
    printf("Family name: ");
    print_string_for_operator(cff, DictOperator::FamilyName);
    printf("Weight: ");
    print_string_for_operator(cff, DictOperator::Weight);
}

} // namespace

void CFFSelectRange3::swap_endian() { first = std::byteswap(first); }

void CFFCharsetRange2::swap_endian() {
    first = std::byteswap(first);
    nLeft = std::byteswap(nLeft);
}

rvoe<CFFont> parse_cff_data(DataSource source) {
    CFFont f;
    f.original_data = std::move(source);
    ERC(dataspan, span_of_source(f.original_data));
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

    print_info(f);

    auto *cse = find_command(f, DictOperator::CharStrings);
    if(!cse) {
        RETERR(UnsupportedFormat);
    }
    if(cse->operand.size() != 1) {
        RETERR(UnsupportedFormat);
    }
    offset = cse->operand.front();
    ERC(cstring, load_index(dataspan, offset));
    f.char_strings = std::move(cstring);

    auto *ence = find_command(f, DictOperator::Encoding);
    if(ence) {
        // Not a CID font.
        // Ignore for not, hopefully forever.
        std::abort();
    }
    auto *cste = find_command(f, DictOperator::Charset);
    if(!cste) {
        RETERR(UnsupportedFormat);
    }
    assert(cste->operand.size() == 1);
    ERC(charsets, unpack_charsets(dataspan.subspan(cste->operand[0])));
    f.charsets = std::move(charsets);

    auto *priv = find_command(f, DictOperator::Private);
    if(priv) {
        offset = priv->operand[0];
        ERC(pdata, load_index(dataspan, offset));
        ERC(pdict, unpack_dictionary(pdata.front()));
        f.pdict = std::move(pdict);
    }

    auto *fda = find_command(f, DictOperator::FDArray);
    auto *fds = find_command(f, DictOperator::FDSelect);
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
    ERC(fdsstr, unpack_fdselect(dataspan.subspan(fds->operand[0]), f.char_strings.size()));
    f.fdselect = std::move(fdsstr);

    return f;
}

rvoe<CFFont> parse_cff_file(const std::filesystem::path &fname) {
    ERC(source, mmap_file(fname.string().c_str()));
    return parse_cff_data(std::move(source));
}

void CFFDictWriter::append_command(const std::vector<int32_t> operands, DictOperator op) {
    for(const auto o : operands) {
        output.push_back(std::byte{29});
        swap_and_append_bytes(output, o);
    }
    if((uint16_t)op > 0xFF) {
        output.push_back(std::byte{0xc});
    }
    output.push_back(std::byte((uint16_t)op & 0xFF));
}

CFFWriter::CFFWriter(const CFFont &source, const std::vector<SubsetGlyphs> &sub)
    : source{source}, sub{sub} {
    output.reserve(100 * 1024);
}

void CFFWriter::create() {
    uint8_t header[4] = {1, 0, 4, 4};
    output.assign((std::byte *)header, (std::byte *)header + 4);
    append_index(source.name);

    create_topdict();
    append_index(source.string);
    append_index(source.global_subr);

    // encodings
    // charsets
    // fdselect
    // charstrings
    // fontdict
    // private
    // local subr
}

void CFFWriter::create_topdict() {
    CFFDictWriter topdict;

    copy_dict_item(topdict, DictOperator::ROS);
    copy_dict_item(topdict, DictOperator::Notice);
    copy_dict_item(topdict, DictOperator::FullName);
    copy_dict_item(topdict, DictOperator::FamilyName);
    copy_dict_item(topdict, DictOperator::Weight);
    copy_dict_item(topdict, DictOperator::FontBBox);
    copy_dict_item(topdict, DictOperator::CIDFontVersion);
    copy_dict_item(topdict, DictOperator::CIDFontRevision);
    copy_dict_item(topdict, DictOperator::CIDCount);
    copy_dict_item(topdict, DictOperator::FDSelect);    // FIXME, offset needs to be fixed in post.
    copy_dict_item(topdict, DictOperator::Charset);     // FIXME, offset needs to be fixed in post.
    copy_dict_item(topdict, DictOperator::CharStrings); // FIXME, offset needs to be fixed in post.
    copy_dict_item(topdict, DictOperator::UnderlinePosition);
    auto td = topdict.steal();
    output.insert(output.end(), td.cbegin(), td.cend());
}

void CFFWriter::copy_dict_item(CFFDictWriter &w, DictOperator op) {
    auto *e = find_command(source, DictOperator::ROS);
    w.append_command(e->operand, e->opr);
}

void CFFWriter::append_index(const std::vector<std::span<std::byte>> &entries) {
    swap_and_append_bytes<uint16_t>(output, entries.size());
    output.push_back(std::byte{4});
    uint32_t offset = 1;
    for(const auto &e : entries) {
        swap_and_append_bytes(output, offset);
        offset += e.size_bytes();
    }
    swap_and_append_bytes(output, offset);
    for(const auto &e : entries) {
        append_bytes(output, e);
    }
}

} // namespace capypdf::internal
