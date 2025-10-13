// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <ft_subsetter.hpp>
#include <bitfiddling.hpp>
#include <fontsubsetter.hpp>
#include <cffsubsetter.hpp>
#include <utils.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <cassert>
#include <bit>
#include <cmath>

#include <variant>
#include <expected>

namespace capypdf::internal {

namespace {

template<typename T> void byte_swap_inplace(T &val) { val = std::byteswap(val); }

uint32_t ttf_checksum(std::span<const std::byte> data) {
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

void TTMaxp05::swap_endian() {
    byte_swap_inplace(version);
    byte_swap_inplace(num_glyphs);
}

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

struct TTEncodingSubtable6 {
    uint16_t format;
    uint16_t length;
    uint16_t language;
    uint16_t firstCode;
    uint16_t entryCount;

    // Followed by vector of 16 bit values.

    void swap_endian() {
        byte_swap_inplace(format);
        byte_swap_inplace(length);
        byte_swap_inplace(language);
        byte_swap_inplace(firstCode);
        byte_swap_inplace(entryCount);
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

struct TTCHeader {
    char tag[4];
    uint16_t major;
    uint16_t minor;
    uint32_t num_fonts;

    void swap_endian() {
        byte_swap_inplace(major);
        byte_swap_inplace(minor);
        byte_swap_inplace(num_fonts);
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

uint16_t TTMaxp::num_glyphs() const {
    return std::visit([](const auto &d) { return d.num_glyphs; }, data);
}

void TTMaxp::set_num_glyphs(uint16_t glyph_count) {
    std::visit([glyph_count](auto &d) { d.num_glyphs = glyph_count; }, data);
}

void TTMaxp::swap_endian() {
    std::visit([](auto &d) { d.swap_endian(); }, data);
}

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

rvoe<TTMaxp> load_maxp(const std::vector<TTDirEntry> &dir, std::span<const std::byte> buf) {
    auto e = find_entry(dir, "maxp");
    if(!e) {
        RETERR(MalformedFontFile);
    }
    uint32_t version;
    ERCV(safe_memcpy(&version, buf, e->offset));
    byte_swap_inplace(version);
    if(version == 1 << 16) {
        if(e->length < sizeof(TTMaxp10)) {
            RETERR(MalformedFontFile);
        }
        ERC(maxp, extract<TTMaxp10>(buf, e->offset));
        maxp.swap_endian();
        return TTMaxp{maxp};
    } else if(version == 0x00005000) {
        ERC(maxp, extract<TTMaxp05>(buf, e->offset));
        maxp.swap_endian();
        return TTMaxp{maxp};
    }
    RETERR(UnsupportedFormat);
}

rvoe<TTHead> load_head(const std::vector<TTDirEntry> &dir, std::span<const std::byte> buf) {
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
                                     std::span<const std::byte> buf,
                                     uint16_t index_to_loc_format,
                                     uint16_t num_glyphs) {
    auto loca = find_entry(dir, "loca");
    if(!loca) {
        // Truetype collection files do not seem to contain loca entries.
        return std::vector<int32_t>{};
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

rvoe<TTHhea> load_hhea(const std::vector<TTDirEntry> &dir, std::span<const std::byte> buf) {
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
                       std::span<const std::byte> buf,
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

rvoe<std::vector<std::span<std::byte>>> load_GLYF_glyphs(uint32_t offset,
                                                         std::span<std::byte> buf,
                                                         uint16_t num_glyphs,
                                                         const std::vector<int32_t> &loca) {
    std::vector<std::span<std::byte>> glyph_data;
    if(offset > buf.size_bytes()) {
        RETERR(MalformedFontFile);
    }
    auto glyf_start = buf.subspan(offset);
    if(num_glyphs + 1 != (int32_t)loca.size()) {
        RETERR(MalformedFontFile);
    }
    for(uint16_t i = 0; i < num_glyphs; ++i) {
        const auto data_off = loca.at(i);
        const auto data_size = loca.at(i + 1) - loca.at(i);
        if(data_off < 0 || data_size < 0 || size_t(data_off + data_size) > glyf_start.size()) {
            RETERR(IndexOutOfBounds);
        }
        glyph_data.push_back(glyf_start.subspan(data_off, data_size));
    }
    return glyph_data;
}

rvoe<CFFont> load_CFF_glyphs(uint32_t offset, std::span<std::byte> buf, uint16_t num_glyphs) {
    auto cff_span = buf.subspan(offset);
    auto cff = parse_cff_data(cff_span);
    if(cff.has_value()) {
        assert(cff->char_strings.size() == num_glyphs);
    }
    return cff;
}

rvoe<std::vector<std::byte>> load_raw_table(const std::vector<TTDirEntry> &dir,
                                            std::span<const std::byte> buf,
                                            const char *tag) {
    auto e = find_entry(dir, tag);
    if(!e) {
        return std::vector<std::byte>{};
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
    return std::vector<std::byte>(buf.data() + e->offset, buf.data() + end_offset);
}

rvoe<std::vector<std::vector<std::byte>>>
subset_glyphs(const TrueTypeFontFile &source,
              const std::vector<TTGlyphs> glyphs,
              const std::unordered_map<uint32_t, uint32_t> &comp_mapping) {
    // This does not use spans. Create a copy of all data
    // because we need to modify it before writing it
    // out to disk.
    std::vector<std::vector<std::byte>> subset;
    assert(std::get<RegularGlyph>(glyphs[0]).unicode_codepoint == 0);
    for(const auto &g : glyphs) {
        uint32_t gid = font_id_for_glyph(g);
        assert(gid < source.glyphs.size());
        const auto &current_glyph = source.glyphs[gid];
        subset.emplace_back(std::vector<std::byte>(current_glyph.begin(), current_glyph.end()));
        if(!subset.back().empty()) {
            ERC(num_contours, extract<int16_t>(subset.back(), 0));
            byte_swap_inplace(num_contours);
            if(num_contours < 0) {
                ERCV(reassign_composite_glyph_numbers(subset.back(), comp_mapping));
            }
        }
    }
    return subset;
}

TTHmtx subset_hmtx(const TrueTypeFontFile &source, const std::vector<TTGlyphs> &glyphs) {
    TTHmtx subset;
    assert(source.hmtx.longhor.size() + source.hmtx.left_side_bearings.size() ==
           source.maxp.num_glyphs());
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

TTDirEntry
write_raw_table(std::vector<std::byte> &odata, const char *tag, std::span<const std::byte> bytes) {
    TTDirEntry e;
    e.set_tag(tag);
    e.offset = odata.size();
    e.length = bytes.size();
    e.checksum = 0;
    append_bytes(odata, bytes);
    return e;
}

std::vector<std::byte> serialize_font(TrueTypeFontFile &tf,
                                      const std::vector<std::vector<std::byte>> &subglyphs) {
    std::vector<std::byte> odata;
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
    directory.push_back(write_raw_table(
        odata, "head", std::span<const std::byte>((const std::byte *)&tf.head, sizeof(tf.head))));

    tf.hhea.swap_endian();
    directory.push_back(write_raw_table(
        odata, "hhea", std::span<const std::byte>((const std::byte *)&tf.hhea, sizeof(tf.hhea))));

    tf.maxp.swap_endian();
    directory.push_back(write_raw_table(
        odata, "maxp", std::span<const std::byte>((const std::byte *)&tf.maxp, sizeof(tf.maxp))));
    // glyph time
    std::vector<int32_t> loca;
    size_t glyphs_start = odata.size();
    // All new glyph data should be in the "subglyphs" parameter.
    assert(tf.glyphs.empty());
    for(const auto &g : subglyphs) {
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
    std::byte *directory_start = odata.data() + sizeof(TTOffsetTable);
    std::span<std::byte> full_view(odata.data(), odata.size());
    for(int i = 0; i < (int)directory.size(); ++i) {
        e.checksum = ttf_checksum(full_view.subspan(e.offset, e.length));
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

std::vector<std::byte> gen_cmap(const std::vector<TTGlyphs> &glyphs) {
    assert(glyphs.size() < 65535);
    TTEncodingSubtable6 glyphencoding;
    glyphencoding.format = 6;
    glyphencoding.language = 0;
    glyphencoding.length = sizeof(glyphencoding) + sizeof(uint16_t) * glyphs.size();
    glyphencoding.firstCode = 0;
    glyphencoding.entryCount = glyphs.size();
    std::vector<uint16_t> glyphids;
    glyphids.reserve(glyphs.size());
    for(size_t i = 0; i < glyphs.size(); ++i) {
        glyphids.push_back(std::byteswap(uint16_t(i)));
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

    std::vector<std::byte> buf;
    append_bytes(buf, cmap_head);
    append_bytes(buf, enc);
    append_bytes(buf, glyphencoding);
    append_bytes(buf, glyphids);

    return buf;
}

rvoe<int16_t> num_contours(std::span<const std::byte> buf) {
    ERC(num_contours, extract<int16_t>(buf, 0));
    byte_swap_inplace(num_contours);
    return num_contours;
}

} // namespace

rvoe<TrueTypeFontFile> parse_truetype_file(DataSource backing, uint64_t header_offset) {
    TrueTypeFontFile tf;
    tf.original_data = std::move(backing);
    ERC(original_data, span_of_source(tf.original_data));
    auto header_span = original_data.subspan(header_offset);
    if(header_span.size() < sizeof(TTOffsetTable)) {
        RETERR(MalformedFontFile);
    }
    ERC(off, extract<TTOffsetTable>(header_span, 0));
    off.swap_endian();
    std::vector<TTDirEntry> directory;
    for(int i = 0; i < off.num_tables; ++i) {
        ERC(e, extract<TTDirEntry>(header_span, sizeof(off) + i * sizeof(TTDirEntry)));
        e.swap_endian();
        if(e.offset + e.length > original_data.size()) {
            RETERR(IndexOutOfBounds);
        }
#ifndef CAPYFUZZING
        auto checksum =
            ttf_checksum(std::span<const std::byte>(original_data.data() + e.offset, e.length));
        (void)checksum;
#endif
        directory.emplace_back(std::move(e));
    }
    ERC(head, load_head(directory, original_data));
    tf.head = head;
    ERC(maxp, load_maxp(directory, original_data));
    tf.maxp = maxp;
#ifdef CAPYFUZZING
    if(tf.maxp.num_glyphs() > 1024) {
        RETERR(MalformedFontFile);
    }
#endif
    ERC(loca,
        load_loca(directory, original_data, tf.head.index_to_loc_format, tf.maxp.num_glyphs()));
    ERC(hhea, load_hhea(directory, original_data))
    tf.hhea = hhea;
    ERC(hmtx, load_hmtx(directory, original_data, tf.maxp.num_glyphs(), tf.hhea.num_hmetrics))
    tf.hmtx = hmtx;
    auto *glyf = find_entry(directory, "glyf");
    if(glyf) {
        ERC(glyphs, load_GLYF_glyphs(glyf->offset, original_data, tf.maxp.num_glyphs(), loca))
        tf.glyphs = glyphs;
    }
    auto *cff = find_entry(directory, "CFF ");
    if(cff) {
        ERC(cffg, load_CFF_glyphs(cff->offset, original_data, tf.maxp.num_glyphs()))
        tf.cff = std::move(cffg);
    }
    ERC(cvt, load_raw_table(directory, original_data, "cvt "));
    tf.cvt = cvt;
    ERC(fpgm, load_raw_table(directory, original_data, "fpgm"));
    tf.fpgm = fpgm;
    ERC(prep, load_raw_table(directory, original_data, "prep"))
    tf.prep = prep;
    ERC(cmap, load_raw_table(directory, original_data, "cmap"));
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
    return tf;
}

rvoe<TrueTypeFontFile> parse_ttc_file(DataSource backing, const FontProperties &props) {
    ERC(original_data, span_of_source(backing));
    ERC(header, extract<TTCHeader>(original_data, 0));
    header.swap_endian();
    std::vector<uint32_t> offsets;
    offsets.reserve(header.num_fonts);
    for(uint32_t i = 0; i < header.num_fonts; ++i) {
        ERC(off, extract<uint32_t>(original_data, sizeof(TTCHeader) + i * sizeof(uint32_t)));
        offsets.push_back(std::byteswap(off));
    }
    return parse_truetype_file(std::move(backing), offsets.at(props.subfont));
}

rvoe<FontData> parse_font_file(DataSource backing, const FontProperties &props) {
    ERC(view, view_of_source(backing));
    if(view.starts_with("ttcf")) {
        ERC(ttc, parse_ttc_file(std::move(backing), props))
        return FontData{std::move(ttc)};
    }
    ERC(ttf, parse_truetype_file(std::move(backing)));
    return FontData{std::move(ttf)};
}

rvoe<std::vector<std::byte>>
generate_truetype_font(const TrueTypeFontFile &source,
                       const std::vector<TTGlyphs> &glyphs,
                       const std::unordered_map<uint32_t, uint32_t> &comp_mapping) {
    TrueTypeFontFile dest;
    assert(std::get<RegularGlyph>(glyphs[0]).unicode_codepoint == 0);
    ERC(subglyphs, subset_glyphs(source, glyphs, comp_mapping));

    dest.head = source.head;
    // https://learn.microsoft.com/en-us/typography/opentype/spec/otff#calculating-checksums
    dest.head.checksum_adjustment = 0;
    dest.hhea = source.hhea;
    dest.maxp = source.maxp;
    dest.maxp.set_num_glyphs(subglyphs.size());
    dest.hmtx = subset_hmtx(source, glyphs);
    dest.hhea.num_hmetrics = dest.hmtx.longhor.size();
    dest.head.index_to_loc_format = 1;
    dest.cvt = source.cvt;
    dest.fpgm = source.fpgm;
    dest.prep = source.prep;
    dest.cmap = gen_cmap(glyphs);

    auto bytes = serialize_font(dest, subglyphs);
    return bytes;
}

rvoe<std::vector<std::byte>> generate_cff_font(const TrueTypeFontFile &source,
                                               const std::vector<TTGlyphs> &glyphs) {
    const auto &source_data = source.cff.value();
    std::vector<SubsetGlyphs> converted;
    converted.reserve(glyphs.size());
    for(const auto &g : glyphs) {
        const auto &tmp = std::get<RegularGlyph>(g);
        if(tmp.unicode_codepoint == 0) {
            converted.emplace_back(0, 0);
        } else {
            converted.emplace_back(tmp.unicode_codepoint, tmp.glyph_index);
        }
    }
    CFFWriter w(source_data, converted);
    w.create();
    return w.steal();
}

rvoe<std::vector<std::byte>>
generate_font(const TrueTypeFontFile &source,
              const std::vector<TTGlyphs> &glyphs,
              const std::unordered_map<uint32_t, uint32_t> &comp_mapping) {
    if(source.in_cff_format()) {
        return generate_cff_font(source, glyphs);
    } else {
        return generate_truetype_font(source, glyphs, comp_mapping);
    }
}

rvoe<FontData> load_and_parse_font_file(const char *fname, const FontProperties &props) {
    ERC(mmapdata, mmap_file(fname));
    return parse_font_file(std::move(mmapdata), props);
}

rvoe<bool> is_composite_glyph(std::span<const std::byte> buf) {
    if(buf.empty()) {
        return false;
    }
    ERC(numc, num_contours(buf));
    return numc < 0;
}

rvoe<bool> is_composite_glyph(const std::vector<std::byte> &buf) {
    std::span<std::byte> sp((std::byte *)buf.data(), (std::byte *)buf.data() + buf.size());
    return is_composite_glyph(sp);
}

rvoe<std::vector<uint32_t>> composite_subglyphs(std::span<const std::byte> buf) {
    std::vector<uint32_t> subglyphs;
    assert(num_contours(buf).value() < 0);
    auto composite_data = std::span<const std::byte>(buf).subspan(5 * sizeof(int16_t));
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
reassign_composite_glyph_numbers(std::span<std::byte> buf,
                                 const std::unordered_map<uint32_t, uint32_t> &mapping) {
    const int64_t header_size = 5 * sizeof(int16_t);

    auto composite_data = buf.subspan(header_size);
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
