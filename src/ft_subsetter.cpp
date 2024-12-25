// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <ft_subsetter.hpp>
#include <fontsubsetter.hpp>
#include <utils.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <cassert>
#include <bit>
#include <cmath>

#include <stdexcept>
#include <variant>
#include <expected>

namespace capypdf::internal {

namespace {

const uint32_t SPACE = ' ';

template<typename T> void byte_swap_inplace(T &val) { val = std::byteswap(val); }

uint32_t ttf_checksum(std::string_view data) {
    uint32_t checksum = 0;
    uint32_t current;
    uint32_t offset = 0;
    for(; (offset + sizeof(uint32_t)) <= data.size(); offset += sizeof(uint32_t)) {
        memcpy(&current, data.data() + offset, sizeof(uint32_t));
        byte_swap_inplace(current);
        checksum += current;
    }
    current = 0;
    const auto tail_size = data.size() % sizeof(uint32_t);
    if(tail_size != 0) {
        memcpy(&current, data.data() + offset, tail_size);
        byte_swap_inplace(current);
        checksum += current;
    }
    return checksum;
}

rvoe<std::string_view> get_substring(const char *buf,
                                     const int64_t bufsize,
                                     const int64_t offset,
                                     const int64_t substr_size) {
    if(!buf) {
        RETERR(ArgIsNull);
    }
    if(bufsize < 0 || offset < 0 || substr_size < 0) {
        RETERR(IndexIsNegative);
    }
    if(offset > bufsize) {
        RETERR(IndexOutOfBounds);
    }
    if(offset + substr_size > bufsize) {
        RETERR(IndexOutOfBounds);
    }
    if(substr_size == 0) {
        return std::string_view{};
    }
    return std::string_view(buf + offset, substr_size);
}

rvoe<std::string_view>
get_substring(std::string_view sv, const size_t offset, const int64_t substr_size) {
    return get_substring(sv.data(), sv.size(), offset, substr_size);
}

template<typename T>
rvoe<capypdf::internal::NoReturnValue>
safe_memcpy(T *obj, std::string_view source, const uint64_t offset) {
    ERC(validated_area, get_substring(source, offset, sizeof(T)));
    memcpy(obj, validated_area.data(), sizeof(T));
    return capypdf::internal::NoReturnValue{};
}

template<typename T> rvoe<T> extract(std::string_view bf, const size_t offset) {
    T obj;
    ERCV(safe_memcpy(&obj, bf, offset));
    return obj;
}

} // namespace

#pragma pack(push, r1, 1)

struct TTOffsetTable {
    int32_t scaler = 0x10000;
    int16_t num_tables;
    int16_t search_range;
    int16_t entry_selector;
    int16_t range_shift;

    void swap_endian() {
        byte_swap_inplace(scaler);
        byte_swap_inplace(num_tables);
        byte_swap_inplace(search_range);
        byte_swap_inplace(entry_selector);
        byte_swap_inplace(range_shift);
    }

    void set_table_size(int16_t new_size) {
        num_tables = new_size;
        assert(new_size > 0);
        int i = 0;
        while((1 << i) < new_size) {
            ++i;
        }
        const auto pow2 = 1 << (i - 1);
        // https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6.html
        //
        // Note that for table 4 the text description has a different definition for
        // entrySelector, i.e. whether it is multiplied by 16 or not.
        search_range = 16 * pow2;
        entry_selector = (int)log2(pow2);
        range_shift = num_tables * 16 - search_range;
    }
};

void TTHead::swap_endian() {
    byte_swap_inplace(version);
    byte_swap_inplace(revision);
    byte_swap_inplace(checksum_adjustment);
    byte_swap_inplace(magic);
    byte_swap_inplace(flags);
    byte_swap_inplace(units_per_em);
    byte_swap_inplace(created);
    byte_swap_inplace(modified);
    byte_swap_inplace(x_min);
    byte_swap_inplace(y_min);
    byte_swap_inplace(x_max);
    byte_swap_inplace(y_max);
    byte_swap_inplace(mac_style);
    byte_swap_inplace(lowest_rec_pppem);
    byte_swap_inplace(font_direction_hint);
    byte_swap_inplace(index_to_loc_format);
    byte_swap_inplace(glyph_data_format);
}

struct TTDirEntry {
    char tag[4];
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;

    void clear() { memset(this, 0, sizeof(TTDirEntry)); }

    bool tag_is(const char *txt) const {
        if(strlen(txt) != 4) {
            fprintf(stderr, "Bad tag.\n");
            std::abort();
        }
        for(int i = 0; i < 4; ++i) {
            if(txt[i] != tag[i]) {
                return false;
            }
        }
        return true;
    }

    void set_tag(const char *txt) {
        if(strlen(txt) != 4) {
            fprintf(stderr, "Bad tag.\n");
            std::abort();
        }
        for(int i = 0; i < 4; ++i) {
            tag[i] = txt[i];
        }
    }

    void swap_endian() {
        // byte_swap_inplace(tag);
        byte_swap_inplace(checksum);
        byte_swap_inplace(offset);
        byte_swap_inplace(length);
    }
};

struct TTDsig {
    uint32_t version;
    uint16_t num_signatures;
    uint16_t flags;
    // Signature array follows

    void swap_endian() {
        byte_swap_inplace(version);
        byte_swap_inplace(num_signatures);
        byte_swap_inplace(flags);
    }
};

struct TTGDEF {
    uint16_t major;
    uint16_t minor;
    uint16_t glyph_class_offset;
    uint16_t attach_list_offset;
    uint16_t lig_caret_offset;
    uint16_t mark_attach_offset;
    uint16_t mark_glyph_sets_offset = -1; // Since version 1.2
    uint32_t item_var_offset = -1;        // Since version 1.3

    void swap_endian() {
        byte_swap_inplace(major);
        byte_swap_inplace(minor);
        byte_swap_inplace(glyph_class_offset);
        byte_swap_inplace(attach_list_offset);
        byte_swap_inplace(lig_caret_offset);
        byte_swap_inplace(mark_attach_offset);
    }
};

struct TTClassRangeRecord {
    uint16_t start_glyph_id;
    uint16_t end_glyph_id;
    uint16_t gclass;

    void swap_endian() {
        byte_swap_inplace(start_glyph_id);
        byte_swap_inplace(end_glyph_id);
        byte_swap_inplace(gclass);
    }
};

void TTMaxp10::swap_endian() {
    byte_swap_inplace(version);
    byte_swap_inplace(num_glyphs);
    byte_swap_inplace(max_points);
    byte_swap_inplace(max_contours);
    byte_swap_inplace(max_composite_points);
    byte_swap_inplace(max_composite_contours);
    byte_swap_inplace(max_zones);
    byte_swap_inplace(max_twilight_points);
    byte_swap_inplace(max_storage);
    byte_swap_inplace(max_function_defs);
    byte_swap_inplace(max_instruction_defs);
    byte_swap_inplace(max_stack_elements);
    byte_swap_inplace(max_sizeof_instructions);
    byte_swap_inplace(max_component_elements);
    byte_swap_inplace(max_component_depth);
}

struct TTGlyphHeader {
    int16_t num_contours;
    int16_t x_min;
    int16_t y_min;
    int16_t x_max;
    int16_t y_max;

    void swap_endian() {
        byte_swap_inplace(num_contours);
        byte_swap_inplace(x_min);
        byte_swap_inplace(y_min);
        byte_swap_inplace(x_max);
        byte_swap_inplace(y_max);
    }
};

void TTHhea::swap_endian() {
    byte_swap_inplace(version);
    byte_swap_inplace(ascender);
    byte_swap_inplace(descender);
    byte_swap_inplace(linegap);
    byte_swap_inplace(advance_width_max);
    byte_swap_inplace(min_left_side_bearing);
    byte_swap_inplace(min_right_side_bearing);
    byte_swap_inplace(x_max_extent);
    byte_swap_inplace(caret_slope_rise);
    byte_swap_inplace(caret_slope_run);
    byte_swap_inplace(caret_offset);
    byte_swap_inplace(reserved0);
    byte_swap_inplace(reserved1);
    byte_swap_inplace(reserved2);
    byte_swap_inplace(reserved3);
    byte_swap_inplace(metric_data_format);
    byte_swap_inplace(num_hmetrics);
}

void TTLongHorMetric::swap_endian() {
    byte_swap_inplace(advance_width);
    byte_swap_inplace(lsb);
}

struct TTCmapHeader {
    uint16_t version;
    uint16_t num_tables;

    void swap_endian() {
        byte_swap_inplace(version);
        byte_swap_inplace(num_tables);
    }
};

struct TTEncodingRecord {
    uint16_t platform_id;
    uint16_t encoding_id;
    int32_t subtable_offset;

    void swap_endian() {
        byte_swap_inplace(platform_id);
        byte_swap_inplace(encoding_id);
        byte_swap_inplace(subtable_offset);
    }
};

struct TTEncodingSubtable0 {
    uint16_t format;
    uint16_t length;
    uint16_t language;
    uint8_t glyphids[256];

    void swap_endian() {
        byte_swap_inplace(format);
        byte_swap_inplace(length);
        byte_swap_inplace(language);
    }
};

struct TTPost {
    uint16_t version_major;
    uint16_t version_minor;
    int32_t italic_angle;
    int16_t underline_position;
    int16_t underline_thickness;
    uint32_t is_fixed_pitch;
    uint32_t min_mem_type_42;
    uint32_t max_mem_type_42;
    uint32_t min_mem_type_1;
    uint32_t max_mem_type_1;

    void swap_endian() {
        byte_swap_inplace(version_major);
        byte_swap_inplace(version_minor);
        byte_swap_inplace(italic_angle);
        byte_swap_inplace(underline_position);
        byte_swap_inplace(underline_thickness);
        byte_swap_inplace(is_fixed_pitch);
        byte_swap_inplace(min_mem_type_42);
        byte_swap_inplace(max_mem_type_42);
        byte_swap_inplace(min_mem_type_1);
        byte_swap_inplace(max_mem_type_1);
    }
};

#pragma pack(pop, r1)

static_assert(sizeof(TTDirEntry) == 4 * 4);

struct SubsetFont {
    TTOffsetTable offset;
    TTHead head;
    std::vector<TTDirEntry> directory;

    void swap_endian() {
        offset.swap_endian();
        head.swap_endian();
        for(auto &e : directory) {
            e.swap_endian();
        }
    }
};

namespace {

/*
uint32_t str2tag(const unsigned char *txt) {
    return (txt[0] << 24) + (txt[1] << 16) + (txt[2] << 8) + txt[3];
}

uint32_t str2tag(const char *txt) { return str2tag((const unsigned char *)txt); }
*/

const TTDirEntry *find_entry(const std::vector<TTDirEntry> &dir, const char *tag) {
    for(const auto &e : dir) {
        if(e.tag_is(tag)) {
            return &e;
        }
    }
    return nullptr;
}

rvoe<TTMaxp10> load_maxp(const std::vector<TTDirEntry> &dir, std::string_view buf) {
    auto e = find_entry(dir, "maxp");
    if(!e) {
        fprintf(stderr, "Maxp table missing.\n");
        RETERR(MalformedFontFile);
    }
    uint32_t version;
    ERCV(safe_memcpy(&version, buf, e->offset));
    byte_swap_inplace(version);
    if(version != 1 << 16) {
        RETERR(UnsupportedFormat);
    }
    if(e->length < sizeof(TTMaxp10)) {
        RETERR(MalformedFontFile);
    }
    ERC(maxp, extract<TTMaxp10>(buf, e->offset));
    maxp.swap_endian();
    return maxp;
}

rvoe<TTHead> load_head(const std::vector<TTDirEntry> &dir, std::string_view buf) {
    auto e = find_entry(dir, "head");
    if(!e) {
        RETERR(MalformedFontFile);
    }
    ERC(head, extract<TTHead>(buf, e->offset));
    head.swap_endian();
#ifndef CAPYFUZZING
    assert(head.magic == 0x5f0f3cf5);
#endif
    return head;
}

rvoe<std::vector<int32_t>> load_loca(const std::vector<TTDirEntry> &dir,
                                     std::string_view buf,
                                     uint16_t index_to_loc_format,
                                     uint16_t num_glyphs) {
    auto loca = find_entry(dir, "loca");
    if(!loca) {
        RETERR(MalformedFontFile);
    }
    std::vector<int32_t> offsets;
    offsets.reserve(num_glyphs);
    if(index_to_loc_format == 0) {
        for(uint16_t i = 0; i <= num_glyphs; ++i) {
            uint16_t offset;
            ERCV(safe_memcpy(&offset, buf, loca->offset + i * sizeof(uint16_t)));
            byte_swap_inplace(offset);
            offsets.push_back(offset * 2);
        }
    } else if(index_to_loc_format == 1) {
        for(uint16_t i = 0; i <= num_glyphs; ++i) {
            int32_t offset;
            ERCV(safe_memcpy(&offset, buf, loca->offset + i * sizeof(int32_t)));
            byte_swap_inplace(offset);
            if(offset < 0) {
                RETERR(IndexIsNegative);
            }
            offsets.push_back(offset);
        }
    } else {
        RETERR(MalformedFontFile);
    }
    return offsets;
}

rvoe<TTHhea> load_hhea(const std::vector<TTDirEntry> &dir, std::string_view buf) {
    auto e = find_entry(dir, "hhea");
    if(!e) {
        RETERR(MalformedFontFile);
    }

    TTHhea hhea;
    if(sizeof(TTHhea) != e->length) {
        RETERR(MalformedFontFile);
    }
    ERCV(safe_memcpy(&hhea, buf, e->offset));
    hhea.swap_endian();
    if(hhea.version != 1 << 16) {
        RETERR(MalformedFontFile);
    }
    if(hhea.metric_data_format != 0) {
        RETERR(UnsupportedFormat);
    }
    return hhea;
}

rvoe<TTHmtx> load_hmtx(const std::vector<TTDirEntry> &dir,
                       std::string_view buf,
                       uint16_t num_glyphs,
                       uint16_t num_hmetrics) {
    auto e = find_entry(dir, "hmtx");
    if(!e) {
        RETERR(MalformedFontFile);
    }
    if(num_hmetrics > num_glyphs) {
        RETERR(MalformedFontFile);
    }
    TTHmtx hmtx;
    for(uint16_t i = 0; i < num_hmetrics; ++i) {
        TTLongHorMetric hm;
        const auto data_offset = e->offset + i * sizeof(TTLongHorMetric);
        ERCV(safe_memcpy(&hm, buf, data_offset));
        hm.swap_endian();
        hmtx.longhor.push_back(hm);
    }
    for(int i = 0; i < num_glyphs - num_hmetrics; ++i) {
        int16_t lsb;
        const auto data_offset =
            e->offset + num_hmetrics * sizeof(TTLongHorMetric) + i * sizeof(int16_t);
        ERCV(safe_memcpy(&lsb, buf, data_offset));
        byte_swap_inplace(lsb);
        hmtx.left_side_bearings.push_back(lsb);
    }
    return hmtx;
}

rvoe<std::vector<std::string>> load_glyphs(const std::vector<TTDirEntry> &dir,
                                           std::string_view buf,
                                           uint16_t num_glyphs,
                                           const std::vector<int32_t> &loca) {
    std::vector<std::string> glyph_data;
    auto e = find_entry(dir, "glyf");
    if(!e) {
        RETERR(MalformedFontFile);
    }
    if(e->offset > buf.size()) {
        RETERR(MalformedFontFile);
    }
    auto glyf_start = std::string_view(buf).substr(e->offset);
    for(uint16_t i = 0; i < num_glyphs; ++i) {
        const auto data_off = loca.at(i);
        const auto data_size = loca.at(i + 1) - loca.at(i);
        ERC(sstr, get_substring(glyf_start, data_off, data_size));
        glyph_data.emplace_back(std::move(sstr));
    }
    return glyph_data;
}

rvoe<std::string>
load_raw_table(const std::vector<TTDirEntry> &dir, std::string_view buf, const char *tag) {
    auto e = find_entry(dir, tag);
    if(!e) {
        return "";
    }
    if(e->offset > buf.size()) {
        RETERR(IndexOutOfBounds);
    }
    auto end_offset = size_t((int64_t)e->offset + (int64_t)e->length);
    // end_offset is an one-past-the end marker.
    if(end_offset > buf.size()) {
        RETERR(IndexOutOfBounds);
    }
    if(end_offset <= e->offset) {
        RETERR(IndexOutOfBounds);
    }
    return std::string(buf.data() + e->offset, buf.data() + end_offset);
}

rvoe<std::vector<std::string>>
subset_glyphs(const TrueTypeFontFile &source,
              const std::vector<TTGlyphs> glyphs,
              const std::unordered_map<uint32_t, uint32_t> &comp_mapping) {
    std::vector<std::string> subset;
    assert(std::get<RegularGlyph>(glyphs[0]).unicode_codepoint == 0);
    assert(glyphs.size() < 255);
    for(const auto &g : glyphs) {
        uint32_t gid = font_id_for_glyph(g);
        assert(gid < source.glyphs.size());
        subset.push_back(source.glyphs[gid]);
        if(!subset.back().empty()) {
            ERC(num_contours, extract<int16_t>(subset.back(), 0));
            byte_swap_inplace(num_contours);
            if(num_contours < 0) {
                ERCV(reassign_composite_glyph_numbers(subset.back(), comp_mapping));
            }
        }
    }
    // Glyph ID 32 _must_ be the space character. Pad empty things until done.
    if(subset.size() < SPACE + 1) {
        while(subset.size() < SPACE) {
            subset.emplace_back(source.glyphs[0]);
        }
        subset.emplace_back(source.glyphs.at(SPACE));
    }
    return subset;
}

TTHmtx subset_hmtx(const TrueTypeFontFile &source, const std::vector<TTGlyphs> &glyphs) {
    TTHmtx subset;
    assert(source.hmtx.longhor.size() + source.hmtx.left_side_bearings.size() ==
           source.maxp.num_glyphs);
    assert(!source.hmtx.longhor.empty());
    for(const auto &g : glyphs) {
        const auto gid = font_id_for_glyph(g);
        if(gid < source.hmtx.longhor.size()) {
            subset.longhor.push_back(source.hmtx.longhor[gid]);
        } else {
            const auto sidebear_index = gid - source.hmtx.longhor.size();
            if(sidebear_index < source.hmtx.left_side_bearings.size()) {
                subset.longhor.emplace_back(
                    TTLongHorMetric{uint16_t(source.hmtx.longhor.back().advance_width),
                                    int16_t(source.hmtx.left_side_bearings[sidebear_index])});
            } else {
                fprintf(stderr, "Malformed font file?\n");
                std::abort();
            }
        }
    }
    assert(subset.left_side_bearings.empty());
    return subset;
}

template<typename T> void append_bytes(std::string &s, const T &val) {
    if constexpr(std::is_same_v<T, std::string_view>) {
        s += val;
    } else if constexpr(std::is_same_v<T, std::string>) {
        s += val;
    } else {
        s.append((char *)&val, (char *)&val + sizeof(val));
    }
}

TTDirEntry write_raw_table(std::string &odata, const char *tag, std::string_view bytes) {
    TTDirEntry e;
    e.set_tag(tag);
    e.offset = odata.size();
    e.length = bytes.length();
    e.checksum = 0;
    append_bytes(odata, bytes);
    return e;
}

std::string serialize_font(TrueTypeFontFile &tf) {
    std::string odata;
    odata.reserve(1024 * 1024);
    TTDirEntry e;
    TTOffsetTable off;
    std::vector<TTDirEntry> directory;

    off.set_table_size(tf.num_directory_entries());
    const auto num_tables = off.num_tables;
    off.swap_endian();
    append_bytes(odata, off);
    e.clear();
    for(int i = 0; i < num_tables; ++i) {
        append_bytes(odata, e);
    }
    if(!tf.cmap.empty()) {
        directory.push_back(write_raw_table(odata, "cmap", tf.cmap));
    }
    if(!tf.cvt.empty()) {
        directory.push_back(write_raw_table(odata, "cvt ", tf.cvt));
    }
    if(!tf.prep.empty()) {
        directory.push_back(write_raw_table(odata, "prep", tf.prep));
    }
    if(!tf.fpgm.empty()) {
        directory.push_back(write_raw_table(odata, "fpgm", tf.fpgm));
    }

    tf.head.checksum_adjustment = 0;
    // const auto start_of_head = odata.length(); // For checksum adjustment.
    tf.head.swap_endian();
    const auto head_offset = odata.size();
    directory.push_back(
        write_raw_table(odata, "head", std::string_view((char *)&tf.head, sizeof(tf.head))));

    tf.hhea.swap_endian();
    directory.push_back(
        write_raw_table(odata, "hhea", std::string_view((char *)&tf.hhea, sizeof(tf.hhea))));

    tf.maxp.swap_endian();
    directory.push_back(
        write_raw_table(odata, "maxp", std::string_view((char *)&tf.maxp, sizeof(tf.maxp))));
    // glyph time
    std::vector<int32_t> loca;
    size_t glyphs_start = odata.size();
    for(const auto &g : tf.glyphs) {
        const auto offset = (int32_t)(odata.size() - glyphs_start);
        loca.push_back(offset);
        append_bytes(odata, g);
    }
    loca.push_back((int32_t)(odata.size() - glyphs_start));
    e.set_tag("glyf");
    e.offset = glyphs_start;
    e.length = odata.size() - glyphs_start;
    directory.push_back(e);

    e.set_tag("loca");
    e.offset = odata.size();
    for(auto offset : loca) {
        byte_swap_inplace(offset);
        append_bytes(odata, offset);
    }
    e.length = odata.size() - e.offset;
    directory.push_back(e);

    e.set_tag("hmtx");
    e.offset = odata.size();
    for(auto hm : tf.hmtx.longhor) {
        hm.swap_endian();
        append_bytes(odata, hm);
    }
    for(auto lsb : tf.hmtx.left_side_bearings) {
        byte_swap_inplace(lsb);
        append_bytes(odata, lsb);
    }
    e.length = odata.size() - e.offset;
    directory.push_back(e);
    assert(directory.size() == (size_t)tf.num_directory_entries());
    char *directory_start = odata.data() + sizeof(TTOffsetTable);
    std::string_view full_view(odata);
    for(int i = 0; i < (int)directory.size(); ++i) {
        e.checksum = ttf_checksum(full_view.substr(e.offset, e.length));
        directory[i].swap_endian();
        memcpy(directory_start + i * sizeof(TTDirEntry), &directory[i], sizeof(TTDirEntry));
    }
    const uint32_t magic = 0xB1B0AFBA;
    const uint32_t full_checksum = ttf_checksum(full_view);
    uint32_t adjustment = magic - full_checksum;
    byte_swap_inplace(adjustment);
    memcpy(odata.data() + head_offset + offsetof(TTHead, checksum_adjustment),
           &adjustment,
           sizeof(uint32_t));

    return odata;
}

std::string gen_cmap(const std::vector<TTGlyphs> &glyphs) {
    TTEncodingSubtable0 glyphencoding;
    glyphencoding.format = 0;
    glyphencoding.language = 0;
    glyphencoding.length = sizeof(glyphencoding);
    for(size_t i = 0; i < 256; ++i) {
        if(i < glyphs.size()) {
            glyphencoding.glyphids[i] = i;
        } else {
            glyphencoding.glyphids[i] = 0;
        }
    }
    TTEncodingRecord enc;
    enc.platform_id = 1;
    enc.encoding_id = 0;
    enc.subtable_offset = sizeof(TTEncodingRecord) + sizeof(TTCmapHeader);
    TTCmapHeader cmap_head;
    cmap_head.num_tables = 1;
    cmap_head.version = 0;

    enc.swap_endian();
    glyphencoding.swap_endian();
    cmap_head.swap_endian();

    std::string buf;
    append_bytes(buf, cmap_head);
    append_bytes(buf, enc);
    append_bytes(buf, glyphencoding);

    return buf;
}

rvoe<int16_t> num_contours(std::string_view buf) {
    ERC(num_contours, extract<int16_t>(buf, 0));
    byte_swap_inplace(num_contours);
    return num_contours;
}

} // namespace

rvoe<TrueTypeFontFile> parse_truetype_font(std::string_view buf) {
    TrueTypeFontFile tf;
    if(buf.size() < sizeof(TTOffsetTable)) {
        RETERR(MalformedFontFile);
    }
    ERC(off, extract<TTOffsetTable>(buf, 0));
    off.swap_endian();
    std::vector<TTDirEntry> directory;
    for(int i = 0; i < off.num_tables; ++i) {
        ERC(e, extract<TTDirEntry>(buf, sizeof(off) + i * sizeof(TTDirEntry)));
        e.swap_endian();
        if(e.offset + e.length > buf.length()) {
            RETERR(IndexOutOfBounds);
        }
#ifndef CAPYFUZZING
        auto checksum = ttf_checksum(std::string_view(buf.data() + e.offset, e.length));
        (void)checksum;
#endif
        directory.emplace_back(std::move(e));
    }
    ERC(head, load_head(directory, buf));
    tf.head = head;
    ERC(maxp, load_maxp(directory, buf));
    tf.maxp = maxp;
#ifdef CAPYFUZZING
    if(tf.maxp.num_glyphs > 1024) {
        RETERR(MalformedFontFile);
    }
#endif
    ERC(loca, load_loca(directory, buf, tf.head.index_to_loc_format, tf.maxp.num_glyphs));
    ERC(hhea, load_hhea(directory, buf))
    tf.hhea = hhea;
    ERC(hmtx, load_hmtx(directory, buf, tf.maxp.num_glyphs, tf.hhea.num_hmetrics))
    tf.hmtx = hmtx;
    ERC(glyphs, load_glyphs(directory, buf, tf.maxp.num_glyphs, loca))
    tf.glyphs = glyphs;

    ERC(cvt, load_raw_table(directory, buf, "cvt "));
    tf.cvt = cvt;
    ERC(fpgm, load_raw_table(directory, buf, "fpgm"));
    tf.fpgm = fpgm;
    ERC(prep, load_raw_table(directory, buf, "prep"))
    tf.prep = prep;
    ERC(cmap, load_raw_table(directory, buf, "cmap"));
    ERC(cmap_head, extract<TTCmapHeader>(cmap, 0));
    cmap_head.swap_endian();
    for(uint16_t table_num = 0; table_num < cmap_head.num_tables; ++table_num) {
        ERC(enc,
            extract<TTEncodingRecord>(cmap,
                                      2 * sizeof(uint16_t) + table_num * sizeof(TTEncodingRecord)));
        enc.swap_endian();
        ERC(subtable_format, extract<uint16_t>(cmap, enc.subtable_offset));
        byte_swap_inplace(subtable_format);
        if(subtable_format >= 15) {
            RETERR(UnsupportedFormat);
        }
#ifndef CAPYFUZZING
        if(subtable_format == 0) {
            ERC(enctable, extract<TTEncodingSubtable0>(cmap, enc.subtable_offset));
            enctable.swap_endian();
            assert(enctable.format == 0);
        }
#endif
    }
    /*
    for(const auto &e : directory) {
        char tagbuf[5];
        tagbuf[4] = 0;
        memcpy(tagbuf, e.tag, 4);
        printf("%s off: %d size: %d\n", tagbuf, e.offset, e.length);
        if(e.tag_is("DSIG")) {
            // Not actually needed for subsetting.
            TTDsig sig;
            memcpy(&sig, buf.data() + e.offset, sizeof(sig));
            sig.swap_endian();
            assert(sig.version == 1);
            assert(sig.num_signatures == 0);
        } else if(e.tag_is("GDEF")) {
            // This neither.
            TTGDEF gdef;
            assert(e.length > sizeof(gdef));
            memcpy(&gdef, buf.data() + e.offset, sizeof(gdef));
            gdef.swap_endian();
            assert(gdef.major == 1);
            assert(gdef.minor == 2);
            gdef.item_var_offset = -1;
            uint16_t classdef_version;
            memcpy(&classdef_version,
                   buf.data() + e.offset + gdef.glyph_class_offset,
                   sizeof(classdef_version));
            byte_swap_inplace(classdef_version);
            assert(classdef_version == 2);
            uint16_t num_records;
            memcpy(&num_records,
                   buf.data() + e.offset + gdef.glyph_class_offset + sizeof(classdef_version),
                   sizeof(num_records));
            byte_swap_inplace(num_records);
            const char *array_start = buf.data() + e.offset + gdef.glyph_class_offset +
                                      sizeof(classdef_version) + sizeof(num_records);
            for(uint16_t i = 0; i < num_records; ++i) {
                TTClassRangeRecord range;
                memcpy(&range, array_start + i * sizeof(range), sizeof(range));
                range.swap_endian();
            }
        } else if(e.tag_is("cmap")) {
            // Maybe we don't need to parse this table, but
            // instead get it from Freetype as needed
            // when generating output?
        } else if(e.tag_is("GPOS")) {
        } else if(e.tag_is("GSUB")) {
        } else if(e.tag_is("OS/2")) {
        } else if(e.tag_is("gasp")) {
        } else if(e.tag_is("name")) {
        } else {
            printf("Unknown tag %s.\n", tagbuf);
            std::abort();

            // TT fonts contain a ton of additional data tables.
            // We ignore all of them.
        }
    }
    */
    return tf;
}

rvoe<std::string> generate_font(std::string_view buf,
                                const std::vector<TTGlyphs> &glyphs,
                                const std::unordered_map<uint32_t, uint32_t> &comp_mapping) {
    ERC(source, parse_truetype_font(buf));
    return generate_font(source, glyphs, comp_mapping);
}

rvoe<std::string> generate_font(const TrueTypeFontFile &source,
                                const std::vector<TTGlyphs> &glyphs,
                                const std::unordered_map<uint32_t, uint32_t> &comp_mapping) {
    TrueTypeFontFile dest;
    assert(std::get<RegularGlyph>(glyphs[0]).unicode_codepoint == 0);
    ERC(subglyphs, subset_glyphs(source, glyphs, comp_mapping));
    dest.glyphs = subglyphs;

    dest.head = source.head;
    // https://learn.microsoft.com/en-us/typography/opentype/spec/otff#calculating-checksums
    dest.head.checksum_adjustment = 0;
    dest.hhea = source.hhea;
    dest.maxp = source.maxp;
    dest.maxp.num_glyphs = dest.glyphs.size();
    dest.hmtx = subset_hmtx(source, glyphs);
    dest.hhea.num_hmetrics = dest.hmtx.longhor.size();
    dest.head.index_to_loc_format = 1;
    dest.cvt = source.cvt;
    dest.fpgm = source.fpgm;
    dest.prep = source.prep;
    dest.cmap = gen_cmap(glyphs);

    auto bytes = serialize_font(dest);
    return bytes;
}

rvoe<TrueTypeFontFile> load_and_parse_truetype_font(const std::filesystem::path &fname) {
    ERC(buf, load_file_as_string(fname));
    return parse_truetype_font(std::string_view{buf.data(), buf.size()});
}

rvoe<bool> is_composite_glyph(std::string_view buf) {
    ERC(numc, num_contours(buf));
    return numc < 0;
}

rvoe<std::vector<uint32_t>> composite_subglyphs(std::string_view buf) {
    std::vector<uint32_t> subglyphs;
    assert(num_contours(buf).value() < 0);
    std::string_view composite_data = std::string_view(buf).substr(5 * sizeof(int16_t));
    int64_t composite_offset = 0;
    const uint16_t MORE_COMPONENTS = 0x20;
    const uint16_t ARGS_ARE_WORDS = 0x01;
    uint16_t component_flag;
    do {
        ERC(rv, extract<uint16_t>(composite_data, composite_offset));
        component_flag = rv;
        byte_swap_inplace(component_flag);
        composite_offset += sizeof(uint16_t);
        ERC(glyph_index, extract<uint16_t>(composite_data, composite_offset));
        byte_swap_inplace(glyph_index);
        composite_offset += sizeof(uint16_t);
        if(component_flag & ARGS_ARE_WORDS) {
            composite_offset += 2 * sizeof(int16_t);
        } else {
            composite_offset += 2 * sizeof(int8_t);
        }
        subglyphs.push_back(glyph_index);
    } while(component_flag & MORE_COMPONENTS);
    return subglyphs;
}

rvoe<NoReturnValue>
reassign_composite_glyph_numbers(std::string &buf,
                                 const std::unordered_map<uint32_t, uint32_t> &mapping) {
    const int64_t header_size = 5 * sizeof(int16_t);

    std::string_view composite_data = std::string_view(buf).substr(header_size);
    int64_t composite_offset = 0;
    const uint16_t MORE_COMPONENTS = 0x20;
    const uint16_t ARGS_ARE_WORDS = 0x01;
    uint16_t component_flag;
    do {
        ERC(rv, extract<uint16_t>(composite_data, composite_offset));
        component_flag = rv;
        byte_swap_inplace(component_flag);
        composite_offset += sizeof(uint16_t);
        const auto index_offset = composite_offset;
        ERC(glyph_index, extract<uint16_t>(composite_data, composite_offset));
        byte_swap_inplace(glyph_index);
        composite_offset += sizeof(uint16_t);
        if(component_flag & ARGS_ARE_WORDS) {
            composite_offset += 2 * sizeof(int16_t);
        } else {
            composite_offset += 2 * sizeof(int8_t);
        }
        auto it = mapping.find(glyph_index);
        if(it == mapping.end()) {
            fprintf(stderr, "FAILfailFAIL\n");
            std::abort();
        }
        glyph_index = it->second;
        byte_swap_inplace(glyph_index);
        memcpy(buf.data() + header_size + index_offset, &glyph_index, sizeof(glyph_index));
    } while(component_flag & MORE_COMPONENTS);
    RETOK;
}

uint32_t font_id_for_glyph(const TTGlyphs &g) {
    if(std::holds_alternative<RegularGlyph>(g)) {
        auto &rg = std::get<RegularGlyph>(g);
        if(rg.unicode_codepoint == 0) {
            return 0;
        } else {
            return rg.glyph_index;
        }
    } else if(std::holds_alternative<CompositeGlyph>(g)) {
        return std::get<CompositeGlyph>(g).glyph_index;
    } else if(std::holds_alternative<LigatureGlyph>(g)) {
        return std::get<LigatureGlyph>(g).glyph_index;
    } else {
        std::abort();
    }
}

} // namespace capypdf::internal
