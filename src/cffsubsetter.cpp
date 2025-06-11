// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <cffsubsetter.hpp>
#include <bitfiddling.hpp>
#include <utils.hpp>

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

rvoe<std::span<std::byte>>
safe_subspan(const std::span<std::byte> &span, size_t offset, size_t size) {
    if(offset + size < offset) {
        // Overflow
        RETERR(IndexOutOfBounds);
    }
    if(offset > span.size_bytes()) {
        RETERR(IndexOutOfBounds);
    }
    if(offset + size > span.size_bytes()) {
        RETERR(IndexOutOfBounds);
    }
    return span.subspan(offset, size);
}

rvoe<size_t> extract_index_offset(std::span<std::byte> dataspan, size_t offset, uint8_t offSize) {
    if(dataspan.size_bytes() <= offset + offSize) {
        RETERR(IndexOutOfBounds);
    }
    if(offSize == 1) {
        const uint8_t b0 = (uint8_t)dataspan[offset];
        return b0;
    } else if(offSize == 2) {
        ERC(v, extract<uint16_t>(dataspan, offset))
        return byteswap(v);
    } else if(offSize == 3) {
        const uint8_t b2 = (uint8_t)dataspan[offset];
        const uint8_t b1 = (uint8_t)dataspan[offset + 1];
        const uint8_t b0 = (uint8_t)dataspan[offset + 2];
        return b2 << 16 | b1 << 8 | b0;
    } else if(offSize == 4) {
        ERC(v, extract<uint32_t>(dataspan, offset))
        return byteswap(v);
    } else {
        RETERR(MalformedFontFile);
    }
}

rvoe<CFFIndex> load_index(std::span<std::byte> dataspan, size_t &offset) {
    CFFIndex index;
    ERC(cnt, extract<uint16_t>(dataspan, offset));
    const uint32_t count = byteswap(cnt);
    if(count == 0) {
        return index;
    }
    offset += sizeof(uint16_t);
    ERC(offSize, extract<uint8_t>(dataspan, offset));
    ++offset;
    std::vector<size_t> offsets;
    offsets.reserve(count + 1);
    if(offSize > 5) {
        RETERR(MalformedFontFile);
    }
    for(uint32_t i = 0; i <= count; ++i) {
        ERC(c, extract_index_offset(dataspan, offset, offSize));
        if(c <= 0) {
            RETERR(MalformedFontFile);
        }
        if(!offsets.empty()) {
            if(offsets.back() > c) {
                /*
                fprintf(stderr,
                        "CFF font has index with negative size in entry %d (previous offset %d, "
                        "current offset %d).\n ",
                        i,
                        (int)offsets.back(),
                        (int)c);
*/
                RETERR(MalformedFontFile);
            }
        }
        offsets.push_back(c);
        offset += offSize;
    }
    --offset;
    index.entries.reserve(count);

    for(uint16_t i = 0; i < count; ++i) {
        const auto entrysize = offsets[i + 1] - offsets[i];
        if(offset + offsets[i] > dataspan.size() ||
           offset + offsets[i] + entrysize > dataspan.size()) {
            RETERR(MalformedFontFile);
        }
        ERC(entry, safe_subspan(dataspan, offset + offsets[i], entrysize));
        index.entries.emplace_back(entry);
    }
    offset += offsets.back();
    return index;
}

rvoe<CFFPrivateDict>
build_pdict_from_dict(std::span<std::byte> dataspan, size_t priv_dict_offset, CFFDict raw_pdict) {
    CFFPrivateDict priv(std::move(raw_pdict), {});
    if(priv.entries.entries.size() > 0) {
        // This seems to always be last.
        const auto &subrs = priv.entries.entries.back();
        if(subrs.opr == DictOperator::Subrs) {
            size_t local_subrs_offset = priv_dict_offset + subrs.operand.front();
            ERC(rsdata, load_index(dataspan, local_subrs_offset));
            priv.subr = std::move(rsdata);
            priv.entries.entries.pop_back();
        }
    }
    return priv;
}

rvoe<CFFDict> unpack_dictionary(std::span<std::byte> dataspan_orig) {
    CFFDict dict;
    size_t offset = 0;
    pystd2025::Vector<int32_t> operands;
    // span.at() is not available yet. Change back to span once it is.
    auto dataspan = span2sv(dataspan_orig);
    while(offset < dataspan.size()) {
        // Read operand
        const int32_t b0 = (uint8_t)dataspan.at(offset++);
        if(b0 <= 21) {
            // Is an operator.
            int32_t unpacked_operator = b0;
            if(b0 == 0xc) {
                const int32_t b1 = (uint8_t)dataspan.at(offset++);
                unpacked_operator = b0 << 8 | b1;
            }
            dict.entries.emplace_back(
                CFFDictItem(std::move(operands), (DictOperator)unpacked_operator));
            operands.clear();
        } else if((b0 >= 28 && b0 <= 30) || (b0 >= 32 && b0 <= 254)) {
            // Is an operand.
            int32_t unpacked_operand;
            if(b0 >= 32 && b0 <= 246) {
                unpacked_operand = b0 - 139;
            } else if(b0 >= 247 && b0 <= 250) {
                const int32_t b1 = (uint8_t)dataspan.at(offset++);
                unpacked_operand = (b0 - 247) * 256 + b1 + 108;
            } else if(b0 >= 251 && b0 <= 254) {
                const int32_t b1 = (uint8_t)dataspan.at(offset++);
                unpacked_operand = -(b0 - 251) * 256 - b1 - 108;
            } else if(b0 == 28) {
                const int32_t b1 = (uint8_t)dataspan.at(offset++);
                const int32_t b2 = (uint8_t)dataspan.at(offset++);
                unpacked_operand = b1 << 8 | b2;
            } else if(b0 == 29) {
                const int32_t b1 = (uint8_t)dataspan.at(offset++);
                const int32_t b2 = (uint8_t)dataspan.at(offset++);
                const int32_t b3 = (uint8_t)dataspan.at(offset++);
                const int32_t b4 = (uint8_t)dataspan.at(offset++);
                unpacked_operand = b1 << 24 | b2 << 16 | b3 << 8 | b4;
            } else if(b0 == 30) {
                // Floating point.
                uint8_t b1;
                do {
                    b1 = (uint8_t)dataspan.at(offset++);
                } while((b1 & 0xf) != 0xf);
                unpacked_operand = -1; // FIXME
            } else {
                std::abort();
            }
            operands.push_back(unpacked_operand);
        }
    }
    if(!operands.is_empty()) {
        RETERR(MalformedFontFile);
    }
    return dict;
}

const CFFDictItem *find_command(const CFFDict &dict, DictOperator op) {
    for(const auto &entry : dict.entries) {
        if(entry.opr == op) {
            return &entry;
        }
    }
    return nullptr;
}

const CFFDictItem *find_command(const CFFont &f, DictOperator op) {
    return find_command(f.top_dict, op);
}

rvoe<pystd2025::Vector<CFFCharsetRange2>> unpack_charsets(const CFFont &f,
                                                          std::span<std::byte> dataspan) {
    if(dataspan.empty()) {
        RETERR(MalformedFontFile);
    }
    pystd2025::Vector<CFFCharsetRange2> charset;
    size_t offset = 0;
    const auto format = (uint8_t)dataspan[offset++];
    if(format == 0) {
        const auto num_glyphs = (int32_t)f.char_strings.size() - 1;
        assert(num_glyphs >= 0);
        std::vector<uint16_t> glyphlist;
        for(int32_t i = 0; i < num_glyphs; ++i) {
            ERC(value, extract<uint16_t>(dataspan, offset + i * sizeof(uint16_t)));
            value = byteswap(value);
            glyphlist.push_back(value);
        }
        // FIXME do something with this.
        charset.push_back(CFFCharsetRange2{1, (uint16_t)(glyphlist.size() - 1)});
    } else if(format == 1) {
        ERC(rng, extract<CFFCharsetRange1>(dataspan, offset));
        rng.swap_endian();
        charset.emplace_back(CFFCharsetRange2{rng.first, rng.nLeft});
    } else {
        ERC(rng, extract<CFFCharsetRange2>(dataspan, offset));
        rng.swap_endian();
        charset.push_back(rng);
    }
    return charset;
}

rvoe<pystd2025::Vector<CFFSelectRange3>> unpack_fdselect(std::span<std::byte> dataspan,
                                                         uint32_t num_glyphs) {
    pystd2025::Vector<CFFSelectRange3> ranges;
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

pystd2025::Vector<uint32_t> append_index_to(pystd2025::Vector<std::byte> &output,
                                            const CFFIndex &index) {
    swap_and_append_bytes<uint16_t>(output, index.size());
    output.push_back(std::byte{4});
    pystd2025::Vector<uint32_t> offsets;
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

rvoe<CFFPrivateDict>
load_private_dict(std::span<std::byte> dataspan, size_t dict_offset, size_t dict_size) {
    CFFPrivateDict pdict;
    if(dict_offset > dataspan.size_bytes() || dict_offset + dict_size > dataspan.size_bytes()) {
        RETERR(MalformedFontFile);
    }
    ERC(dictspan, safe_subspan(dataspan, dict_offset, dict_size));
    ERC(raw_pdict, unpack_dictionary(dictspan));
    for(const auto &e : raw_pdict.entries) {
        if(e.opr == DictOperator::Subrs) {
            if(e.operand.is_empty()) {
                RETERR(MalformedFontFile);
            }
            const auto &subr_offset = e.operand.front();
            size_t from_the_top = dict_offset + subr_offset;
            ERC(subr_index, load_index(dataspan, from_the_top));
            pdict.subr = std::move(subr_index);
        } else {
            pdict.entries.entries.push_back(e);
        }
    }
    return pdict;
}

rvoe<CFFFontDict> load_fdarray_entry(std::span<std::byte> dataspan, std::span<std::byte> dstr) {
    ERC(raw_entries, unpack_dictionary(dstr));
    CFFFontDict fdict;
    for(const auto &e : raw_entries.entries) {
        if(e.opr == DictOperator::Private) {
            if(e.operand.size() != 2) {
                RETERR(MalformedFontFile);
            }
            const auto &local_dsize = e.operand.front();
            const auto &local_offset = e.operand.back();
            ERC(priv, load_private_dict(dataspan, local_offset, local_dsize));
            fdict.priv = std::move(priv);
        } else {
            fdict.entries.entries.push_back(e);
        }
    }
    return fdict;
}

size_t write_private_dict(pystd2025::Vector<std::byte> &output, const CFFPrivateDict &pd) {
    CFFDictWriter w;
    for(const auto &e : pd.entries.entries) {
        const auto opr = e.opr;
        if(opr == DictOperator::Subrs || opr == DictOperator::Encoding) {
            // We output only CID fonts, which are not allowed to have these operators in them.
            continue;
        }
        w.append_command(e);
    }
    if(pd.subr) {
        // This is always last, so we know the layout.
        CFFDictItem e;
        e.operand.push_back(w.current_size() + 1 + 4 + 1);
        e.opr = DictOperator::Subrs;
        w.append_command(e);
    }
    auto bytes = w.steal();
    output.append(bytes.output.cbegin(), bytes.output.cend());
    if(pd.subr) {
        append_index_to(output, pd.subr.value());
    }
    return bytes.output.size();
}

pystd2025::Vector<CFFSelectRange3> build_fdselect3(const CFFont &source,
                                                   const std::vector<SubsetGlyphs> &sub) {
    pystd2025::Vector<CFFSelectRange3> result;
    result.reserve(10);
    if(source.is_cid) {
        result.emplace_back(CFFSelectRange3(0, source.get_fontdict_id(0)));
        for(size_t i = 1; i < sub.size(); ++i) {
            const auto &sg = sub[i];
            const auto sg_fd = source.get_fontdict_id(sg.gid);
            if(sg_fd != result.back().fd) {
                result.emplace_back(CFFSelectRange3((uint16_t)i, sg_fd));
            }
        }
    } else {
        // If the source is not in CID format, then in the output
        // CID format all glyphs use the same private dictionary.
        for(size_t i = 0; i < sub.size(); ++i) {
            result.emplace_back(CFFSelectRange3((uint16_t)i, 0));
        }
    }
    return result;
}

} // namespace

void CFFSelectRange3::swap_endian() { first = byteswap(first); }

void CFFCharsetRange1::swap_endian() { first = byteswap(first); }

void CFFCharsetRange2::swap_endian() {
    first = byteswap(first);
    nLeft = byteswap(nLeft);
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
    if(f.header.hdrsize != 4) {
        RETERR(MalformedFontFile);
    }
    if(f.header.offsize == 0 || f.header.offsize >= 5) {
        RETERR(MalformedFontFile);
    }
    size_t offset = f.header.hdrsize;
    ERC(name_index, load_index(dataspan, offset));
    f.name = std::move(name_index);
    ERC(topdict_index, load_index(dataspan, offset));
    f.top_dict_data = std::move(topdict_index);
    if(f.top_dict_data.entries.empty()) {
        RETERR(MalformedFontFile);
    }
    ERC(tc, unpack_dictionary(f.top_dict_data.entries.front()));
    if(tc.entries.is_empty()) {
        RETERR(MalformedFontFile);
    }
    f.top_dict = std::move(tc);
    ERC(string_index, load_index(dataspan, offset));
    f.string = std::move(string_index);
    ERC(global_subr_index, load_index(dataspan, offset));
    f.global_subr = std::move(global_subr_index);

    if(false) {
        print_info(f);
    }
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
        f.predefined_encoding = ence->operand.front();
    }
    auto *cste = find_command(f, DictOperator::Charset);
    if(!cste) {
        RETERR(UnsupportedFormat);
    }
    if(cste->operand.size() != 1) {
        RETERR(MalformedFontFile);
    }
    if((size_t)cste->operand[0] > dataspan.size_bytes()) {
        RETERR(MalformedFontFile);
    }
    ERC(charsets, unpack_charsets(f, dataspan.subspan(cste->operand[0])));
    f.charsets = std::move(charsets);

    auto *priv = find_command(f, DictOperator::Private);
    if(priv) {
        if(priv->operand.is_empty()) {
            RETERR(MalformedFontFile);
        }
        auto dict_offset = priv->operand[1];
        auto dict_size = priv->operand[0];
        ERC(raw_pdict, unpack_dictionary(dataspan.subspan(dict_offset, dict_size)));
        ERC(processed_pdict, build_pdict_from_dict(dataspan, dict_offset, raw_pdict));
        f.pdict = std::move(processed_pdict);
    }

    auto *fda = find_command(f, DictOperator::FDArray);
    auto *fds = find_command(f, DictOperator::FDSelect);
    if(fda) {
        f.is_cid = true;
    } else {
        f.is_cid = false;
    }
    if(!f.is_cid) {
        append_ros_strings(f);
    }
    if(f.is_cid) {
        if(!fda || fda->operand.is_empty()) {
            RETERR(UnsupportedFormat);
        }
        offset = fda->operand[0];
        ERC(fdastr, load_index(dataspan, offset))
        for(const auto dstr : fdastr.entries) {
            ERC(fdentry, load_fdarray_entry(dataspan, dstr));
            f.fdarray.emplace_back(std::move(fdentry));
        }
        if(!fds || fds->operand.is_empty()) {
            RETERR(UnsupportedFormat);
        }
        offset = fds->operand[0];
        if(offset > dataspan.size_bytes()) {
            RETERR(MalformedFontFile);
        }
        ERC(fdsstr, unpack_fdselect(dataspan.subspan(offset), f.char_strings.size()));
        f.fdselect = std::move(fdsstr);
    }
    return f;
}

static const char registry_str[] = "Adobe";
static const char ordering_str[] = "Identity";

void append_ros_strings(CFFont &f) {
    assert(!f.is_cid);
    // Add two string needed to specify ROS values.
    std::span<std::byte> registry((std::byte *)registry_str, strlen(registry_str));
    f.string.entries.push_back(registry);
    std::span<std::byte> ordering((std::byte *)ordering_str, strlen(ordering_str));
    f.string.entries.push_back(ordering);
}

uint8_t CFFont::get_fontdict_id(uint16_t glyph_id) const {
    if(!is_cid) {
        return glyph_id;
    }
    assert(!fdselect.is_empty());
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

rvoe<CFFont> parse_cff_file(const pystd2025::Path &fname) {
    ERC(source, mmap_file(fname.c_str()));
    return parse_cff_data(std::move(source));
}

void CFFDictWriter::append_command(const pystd2025::Vector<int32_t> &operands, DictOperator op) {
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
    std::vector<std::vector<std::byte>> fontdicts;
    pystd2025::Vector<std::byte>
        privatedict_buffer;                  // Stores concatenated private dict/localsubr pairs.
    std::vector<size_t> privatedict_offsets; // within the above buffer
    std::vector<size_t> privatereference_offsets; // where the correct value shall be written
    if(source.is_cid) {
        for(const auto &source_dict : source.fdarray) {
            // Why is this so complicated you ask?
            // Because the data model is completely wacko.
            //
            // Each glyph has a "font" dictionary.
            // There can be only 256 font dictionaries total. Even with 65k glyphs.
            // Each of those point to a "private" dictionary.
            // Each of those point to "local subrs" index
            // That one is needed for rendering.
            //
            // The latter two are not stored in any global
            // index, they jost float around in the file.
            // If you don't read every single letter of
            // the (not particularly great) CFF spec with a
            // magnifying glass, you can't decipher that
            // and your subset fonts won't work.
            //
            // To make things simple:
            //
            // Any metadata that is somewhat shared
            // is copied as is so all indexes and offsets
            // work directly. Only character data is subset.

            privatedict_offsets.push_back(privatedict_buffer.size());
            size_t last_dict_size = -1;
            if(source_dict.priv) {
                last_dict_size = write_private_dict(privatedict_buffer, source_dict.priv.value());
            } else {
                last_dict_size = write_private_dict(privatedict_buffer, source.pdict);
            }
            CFFDictWriter fdarray_dict_writer;
            for(const auto &entry : source_dict.entries.entries) {
                fdarray_dict_writer.append_command(entry);
            }
            if(source_dict.priv) {
                privatereference_offsets.push_back(fdarray_dict_writer.current_size() + 6);
                CFFDictItem e;
                e.operand.push_back(last_dict_size);
                e.operand.push_back(-1); // Offset from the beginning of the file, fixed below.
                e.opr = DictOperator::Private;
                fdarray_dict_writer.append_command(e);
            } else {
                privatereference_offsets.push_back(-1);
            }
            auto serialization = fdarray_dict_writer.steal();
            fontdicts.emplace_back(std::move(serialization.output));
        }
    } else {
        // Create a FDArray that has only one element that all glyphs use.
        CFFDictWriter fdarray_dict_writer;

        privatedict_offsets.push_back(privatedict_buffer.size());
        auto dict_size = write_private_dict(privatedict_buffer, source.pdict);
        CFFDictItem e;
        privatereference_offsets.push_back(fdarray_dict_writer.current_size() + 6);
        // FIXME, add font name here.
        e.operand.push_back(dict_size);
        e.operand.push_back(-1);
        e.opr = DictOperator::Private;
        fdarray_dict_writer.append_command(e);
        auto serialization = fdarray_dict_writer.steal();
        fontdicts.emplace_back(std::move(serialization.output));
    }
    fixups.fdarray.value = output.size();
    auto fdarray_index_offsets = append_index(fontdicts);
    const auto privatedict_area_start = output.size();
    output.append(privatedict_buffer.begin(), privatedict_buffer.end());

    assert(fdarray_index_offsets.size() == privatereference_offsets.size());
    assert(fdarray_index_offsets.size() == privatedict_offsets.size());
    for(size_t i = 0; i < fdarray_index_offsets.size(); ++i) {
        const auto fdarray_index_offset = fdarray_index_offsets[i];
        const auto privatereference_offset = privatereference_offsets[i];
        const auto privatedict_offset = privatedict_offsets[i];
        const auto write_location = fdarray_index_offset + privatereference_offset;
        const uint32_t offset_value = privatedict_area_start + privatedict_offset;

        if(privatereference_offset == (size_t)-1) {
            continue;
        }
        uint32_t sanity_check;
        memcpy(&sanity_check, output.data() + write_location, sizeof(int32_t));
        if(sanity_check != (uint32_t)-1) {
            std::abort();
        }
        const auto offset_be = byteswap(offset_value);
        memcpy(output.data() + write_location, &offset_be, sizeof(int32_t));
    }

    // Now fdselect using the 16 bit format 3
    fixups.fdselect.value = output.size();
    auto fdrange = build_fdselect3(source, sub);
    assert(fdrange.size() <= 65535);
    output.push_back(std::byte(3));
    swap_and_append_bytes(output, (uint16_t)fdrange.size());
    for(auto &fd : fdrange) {
        fd.swap_endian();
        append_bytes(output, fd);
    }
    swap_and_append_bytes(output, (uint16_t)sub.size());
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
    const uint32_t value = byteswap(p.value);
    assert(p.offset + sizeof(uint32_t) < output.size());
    memcpy(output.data() + p.offset, &value, sizeof(uint32_t));
}

void CFFWriter::create_topdict() {
    CFFDictWriter topdict;

    if(source.is_cid) {
        copy_dict_item(topdict, DictOperator::ROS);
    } else {
        CFFDictItem ros;
        ros.opr = DictOperator::ROS;
        ros.operand.push_back(391 + source.string.size() - 2);
        ros.operand.push_back(391 + source.string.size() - 1);
        ros.operand.push_back(0);
        topdict.append_command(ros);
    }
    copy_dict_item(topdict, DictOperator::Notice);
    copy_dict_item(topdict, DictOperator::FullName);
    copy_dict_item(topdict, DictOperator::FamilyName);
    copy_dict_item(topdict, DictOperator::Weight);
    copy_dict_item(topdict, DictOperator::FontBBox);
    if(source.is_cid) {
        copy_dict_item(topdict, DictOperator::CIDFontVersion);
        copy_dict_item(topdict, DictOperator::CIDCount);
        copy_dict_item(topdict, DictOperator::FDArray);  // offset needs to be fixed in post.
        copy_dict_item(topdict, DictOperator::FDSelect); // offset needs to be fixed in post.
    } else {
        pystd2025::Vector<int32_t> operand;
        operand.push_back(-1);
        topdict.append_command(operand, DictOperator::CIDFontVersion);
        operand.clear();
        operand.push_back(65535);
        topdict.append_command(operand, DictOperator::CIDCount);
        operand.clear();
        operand.push_back(-1);
        topdict.append_command(operand, DictOperator::FDArray);
        topdict.append_command(operand, DictOperator::FDSelect);
    }
    copy_dict_item(topdict, DictOperator::Charset); // offset needs to be fixed in post.
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
    const auto written_value = byteswap(sanity_check_be);
    const auto original_value = find_command(source, DictOperator::FDArray)->operand.front();
    const auto original_swapped = byteswap(original_value);
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

pystd2025::Vector<uint32_t> CFFWriter::append_index(const CFFIndex &index) {
    return append_index_to(output, index);
}

pystd2025::Vector<uint32_t>
CFFWriter::append_index(const std::vector<std::vector<std::byte>> &entries) {
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
