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
 * 3109  24 FDArray
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

rvoe<CFFIndex> load_index(std::span<std::byte> dataspan, size_t &offset) {
    CFFIndex index;
    ERC(cnt, extract<uint16_t>(dataspan, offset));
    const uint32_t count = std::byteswap(cnt);
    if(count == 0) {
        return index;
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
    index.entries.reserve(count);

    for(uint16_t i = 0; i < count; ++i) {
        const auto entrysize = offsets[i + 1] - offsets[i];
        auto entry = dataspan.subspan(offset + offsets[i], entrysize);
        index.entries.emplace_back(entry);
    }
    offset += offsets.back();
    return index;
}

rvoe<CFFDict> unpack_dictionary(std::span<std::byte> dataspan) {
    CFFDict dict;
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
            dict.entries.emplace_back(std::move(operands), (DictOperator)unpacked_operator);
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

const CFFDictItem *find_command(const CFFDict &dict, DictOperator op) {
    for(size_t i = 0; i < dict.entries.size(); ++i) {
        if(dict.entries[i].opr == op) {
            return &dict.entries[i];
        }
    }
    return nullptr;
}

const CFFDictItem *find_command(const CFFont &f, DictOperator op) {
    return find_command(f.top_dict, op);
}

rvoe<std::vector<CFFCharsetRange2>> unpack_charsets(std::span<std::byte> dataspan) {
    std::vector<CFFCharsetRange2> charset;
    size_t offset = 0;
    const auto format = (uint8_t)dataspan[offset++];
    if(format == 0) {
        // RETERR(UnsupportedFormat);
        //  FIXME
        return charset;
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
        auto sv = span2sv(cff.string.entries.at(strindex));
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

std::vector<uint32_t> append_index_to(std::vector<std::byte> &output, const CFFIndex &index) {
    swap_and_append_bytes<uint16_t>(output, index.size());
    output.push_back(std::byte{4});
    std::vector<uint32_t> offsets;
    uint32_t offset = 1;
    for(const auto &e : index.entries) {
        swap_and_append_bytes(output, offset);
        offset += e.size_bytes();
    }
    swap_and_append_bytes(output, offset);
    for(const auto &e : index.entries) {
        offsets.push_back(output.size());
        append_bytes(output, e);
    }
    return offsets;
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
    ERC(tc, unpack_dictionary(f.top_dict_data.entries.front()));
    f.top_dict = std::move(tc);
    ERC(string_index, load_index(dataspan, offset));
    f.string = std::move(string_index);
    ERC(global_subr_index, load_index(dataspan, offset));
    f.global_subr = std::move(global_subr_index);

    // print_info(f);

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
        ERC(pdict, unpack_dictionary(pdata.entries.front()));
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
    for(const auto dstr : fdastr.entries) {
        ERC(dict, unpack_dictionary(dstr));
        f.fontdict.emplace_back(std::move(dict));
        const auto &private_op = f.fontdict.back();
        assert(private_op.entries.back().opr == DictOperator::Private);
        const auto &local_dsize = private_op.entries.back().operand.front();
        const auto &local_offset = private_op.entries.back().operand.back();
        auto privdict_range = dataspan.subspan(local_offset, local_dsize);
        ERC(priv, unpack_dictionary(privdict_range));
        auto subrcmd = find_command(priv, DictOperator::Subrs);
        if(subrcmd) {
            std::byte *dptr = privdict_range.data() + subrcmd->operand.front();
            size_t difftmp = dptr - dataspan.data();
            ERC(local_subr, load_index(dataspan, difftmp));
            f.local_subrs.emplace_back(std::move(local_subr));
        } else {
            f.local_subrs.emplace_back(std::vector<std::span<std::byte>>());
        }
    }
    offset = fds->operand[0];
    ERC(fdsstr, unpack_fdselect(dataspan.subspan(fds->operand[0]), f.char_strings.size()));
    f.fdselect = std::move(fdsstr);

    return f;
}

uint16_t CFFont::get_fontdict_id(uint16_t glyph_id) const {
    assert(!fdselect.empty());
    for(size_t i = 0; i < fdselect.size(); ++i) {
        if(fdselect[i].first == glyph_id) {
            return fdselect[i].fd;
        }
        if(fdselect[i].first > glyph_id) {
            assert(i > 0);
            return fdselect[i - 1].fd;
        }
    }
    return fdselect.back().fd;
}

rvoe<CFFont> parse_cff_file(const std::filesystem::path &fname) {
    ERC(source, mmap_file(fname.string().c_str()));
    return parse_cff_data(std::move(source));
}

void CFFDictWriter::append_command(const std::vector<int32_t> operands, DictOperator op) {
    o.offsets.push_back(o.output.size());
    for(const auto opr : operands) {
        o.output.push_back(std::byte{29});
        swap_and_append_bytes(o.output, opr);
    }
    if((uint16_t)op > 0xFF) {
        o.output.push_back(std::byte{0xc});
    }
    o.output.push_back(std::byte((uint16_t)op & 0xFF));
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
    fixups.charsets.value = output.size();
    append_charset();
    fixups.charstrings.value = output.size();
    append_charstrings();
    append_fdthings();

    patch_offsets();
    //  encodings
    //  private
    //  local subr
}

void CFFWriter::append_fdthings() {
    LocalSubrs localsubs;
    auto source_data = span_of_source(source.original_data).value();
    std::vector<std::vector<std::byte>> fontdicts;
    for(const auto &s : sub) {
        auto source_id = source.get_fontdict_id(s.gid);
        const auto &source_dict = source.fontdict.at(source_id);
        const size_t private_id = 1; // FIXME. Got lazy. :(
        assert(source_dict.entries[private_id].opr == DictOperator::Private);
        const auto &source_localsubr = source.local_subrs.at(source_id);
        const auto local_private_dict_size = source_dict.entries[private_id].operand.front();
        const auto local_private_dict_offset = source_dict.entries[private_id].operand.back();

        CFFDictWriter w;
        for(const auto &entry : source_dict.entries) {
            w.append_command(entry.operand, entry.opr);
        }
        auto serialization = w.steal();
        localsubs.data_offsets.push_back(localsubs.data.size());
        auto localprivate_span =
            source_data.subspan(local_private_dict_offset, local_private_dict_size);
        auto local_private_dict = unpack_dictionary(localprivate_span).value();
        /*
        if(local_subr_cmd_offset != (size_t)-1) {
            const auto localsubr_fixup_location = local_subr_cmd_offset + 1;
            const uint32_t localsubr_fixup_value =
                serialization.output.size() - local_subr_cmd_offset + localprivate.size();
            const uint32_t fixup_value_be = std::byteswap(localsubr_fixup_value);
            memcpy(serialization.output.data() + localsubr_fixup_location,
                   &fixup_value_be,
                   sizeof(uint32_t));
        }

        localsubs.data.insert(localsubs.data.end(), localprivate.begin(), localprivate.end());
*/
        append_index_to(localsubs.data, source_localsubr);
        fontdicts.emplace_back(std::move(serialization.output));
    }
    fixups.fdarray.value = output.size();
    auto index_offsets = append_index(fontdicts);

    // Now fdselect
    fixups.fdselect.value = output.size();
    if(sub.size() >= 256) {
        fprintf(stderr, "More than 255 glyphs not yet supported.\n");
        std::abort();
    }
    // To get started use format 0, which is super easy, barely an inconvenience.
    // Need to be changed to v3 or something.
    output.push_back(std::byte(0));
    for(uint8_t i = 0; i < sub.size(); ++i) {
        output.push_back(std::byte(i));
    }

    // Local subrs are stored wherever in unstructured slop.
    const uint32_t localsub_start = output.size();
    output.insert(output.end(), localsubs.data.begin(), localsubs.data.end());
    // Fix offsets.
    assert(localsubs.data_offsets.size() == index_offsets.size());
    for(size_t i = 0; i < localsubs.data_offsets.size(); ++i) {
        const uint32_t fixed_offset = localsub_start + localsubs.data_offsets[i];
        const uint32_t off_be = std::byteswap(fixed_offset);
        const auto fix_location = index_offsets[i] + 13; // localsubs.index_offsets[i];
        /*
        const auto &original = source.fontdict.at(source.get_fontdict_id(sub[i].gid));
        const auto original_value = original[1].operand.back();
        const auto swapped_original = std::byteswap(original_value);
        auto found = std::search(output.begin(),
                                 output.end(),
                                 (std::byte *)&swapped_original,
                                 (std::byte *)&swapped_original + 4);
        assert(found != output.end());
        const auto correct_distance = std::distance(output.begin(), found);
        assert(correct_distance == fix_location);
*/
        memcpy(output.data() + fix_location, &off_be, sizeof(off_be));
    }
}

void CFFWriter::patch_offsets() {
    write_fix(fixups.charsets);
    write_fix(fixups.charstrings);
    write_fix(fixups.fdselect);
    write_fix(fixups.fdarray);
}

void CFFWriter::write_fix(const OffsetPatch &p) {
    assert(p.offset != (uint32_t)-1);
    assert(p.value != (uint32_t)-1);
    const uint32_t value = std::byteswap(p.value);
    assert(p.offset + sizeof(uint32_t) < output.size());
    memcpy(output.data() + p.offset, &value, sizeof(uint32_t));
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
    // copy_dict_item(topdict, DictOperator::CIDFontRevision);
    copy_dict_item(topdict, DictOperator::CIDCount);
    copy_dict_item(topdict, DictOperator::FDArray);  // offset needs to be fixed in post.
    copy_dict_item(topdict, DictOperator::FDSelect); // offset needs to be fixed in post.
    copy_dict_item(topdict, DictOperator::Charset);  // offset needs to be fixed in post.
    copy_dict_item(topdict,
                   DictOperator::CharStrings); // offset needs to be fixed in post.
    // copy_dict_item(topdict, DictOperator::UnderlinePosition);
    auto serialization = topdict.steal();
    std::vector<std::vector<std::byte>> wrapper{std::move(serialization.output)};
    auto offsets = append_index(wrapper);
    assert(offsets.size() == 1);
    const auto dict_start = offsets.front();
    fixups.fdarray.offset = serialization.offsets.at(8) + 1 + dict_start;
    fixups.fdselect.offset = serialization.offsets.at(9) + 1 + dict_start;
    fixups.charsets.offset = serialization.offsets.at(10) + 1 + dict_start;
    fixups.charstrings.offset = serialization.offsets.at(11) + 1 + dict_start;

    /*
    uint32_t sanity_check_be;
    memcpy(&sanity_check_be, output.data() + fixups.fdarray.offset, sizeof(sanity_check_be));
    const auto written_value = std::byteswap(sanity_check_be);
    const auto original_value = find_command(source, DictOperator::FDArray)->operand.front();
    const auto original_swapped = std::byteswap(original_value);
    auto loc = std::search(output.begin(),
                           output.end(),
                           (std::byte *)&original_swapped,
                           (std::byte *)&original_swapped + 4);
    assert(loc != output.end());
    const auto real_offset = std::distance(output.begin(), loc);
    assert(written_value == original_value);
    */
}

void CFFWriter::copy_dict_item(CFFDictWriter &w, DictOperator op) {
    auto *e = find_command(source, op);
    assert(e);
    w.append_command(e->operand, e->opr);
}

std::vector<uint32_t> CFFWriter::append_index(const CFFIndex &index) {
    return append_index_to(output, index);
}

std::vector<uint32_t> CFFWriter::append_index(const std::vector<std::vector<std::byte>> &entries) {
    CFFIndex converter;
    converter.entries.reserve(entries.size());
    for(const auto &e : entries) {
        converter.entries.emplace_back((std::byte *)e.data(), e.size());
    }
    return append_index(converter);
}

void CFFWriter::append_charset() {
    output.emplace_back(std::byte(0));
    for(uint16_t i = 1; i < sub.size(); ++i) {
        swap_and_append_bytes(output, i);
    }
}

void CFFWriter::append_charstrings() {
    CFFIndex subdata;
    subdata.entries.reserve(sub.size());
    for(const auto &subglyph : sub) {
        subdata.entries.push_back(source.char_strings.entries.at(subglyph.gid));
    }
    append_index(subdata);
}

} // namespace capypdf::internal
