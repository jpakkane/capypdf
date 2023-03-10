/*
 * Copyright 2023 Jussi Pakkanen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ft_subsetter.hpp>
#include <fontsubsetter.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <cassert>
#include <byteswap.h>
#include <cmath>

#include <stdexcept>
#include <variant>

namespace {

void byte_swap(int16_t &val) { val = bswap_16(val); }
void byte_swap(uint16_t &val) { val = bswap_16(val); }
void byte_swap(int32_t &val) { val = bswap_32(val); }
void byte_swap(uint32_t &val) { val = bswap_32(val); }
// void byte_swap(int64_t &val) { val = bswap_64(val); }
void byte_swap(uint64_t &val) { val = bswap_64(val); }

uint32_t ttf_checksum(std::string_view data) {
    uint32_t checksum = 0;
    uint32_t current;
    uint32_t offset = 0;
    for(; (offset + sizeof(uint32_t)) <= data.size(); offset += sizeof(uint32_t)) {
        memcpy(&current, data.data() + offset, sizeof(uint32_t));
        byte_swap(current);
        checksum += current;
    }
    current = 0;
    const auto tail_size = data.size() % sizeof(uint32_t);
    if(tail_size != 0) {
        memcpy(&current, data.data() + offset, tail_size);
        byte_swap(current);
        checksum += current;
    }
    return checksum;
}

} // namespace

namespace A4PDF {

#pragma pack(push, r1, 1)

struct TTOffsetTable {
    int32_t scaler = 0x10000;
    int16_t num_tables;
    int16_t search_range;
    int16_t entry_selector;
    int16_t range_shift;

    void swap_endian() {
        byte_swap(scaler);
        byte_swap(num_tables);
        byte_swap(search_range);
        byte_swap(entry_selector);
        byte_swap(range_shift);
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
    byte_swap(version);
    byte_swap(revision);
    byte_swap(checksum_adjustment);
    byte_swap(magic);
    byte_swap(flags);
    byte_swap(units_per_em);
    byte_swap(created);
    byte_swap(modified);
    byte_swap(x_min);
    byte_swap(y_min);
    byte_swap(x_max);
    byte_swap(y_max);
    byte_swap(mac_style);
    byte_swap(lowest_rec_pppem);
    byte_swap(font_direction_hint);
    byte_swap(index_to_loc_format);
    byte_swap(glyph_data_format);
}

struct TTDirEntry {
    char tag[4];
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;

    void clear() { memset(this, 0, sizeof(TTDirEntry)); }

    bool tag_is(const char *txt) const { return strncmp(tag, txt, 4) == 0; }

    void set_tag(const char *txt) {
        memset(tag, ' ', 4);
        strncpy(tag, txt, 4);
    }

    void swap_endian() {
        // byte_swap(tag);
        byte_swap(checksum);
        byte_swap(offset);
        byte_swap(length);
    }
};

struct TTDsig {
    uint32_t version;
    uint16_t num_signatures;
    uint16_t flags;
    // Signature array follows

    void swap_endian() {
        byte_swap(version);
        byte_swap(num_signatures);
        byte_swap(flags);
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
        byte_swap(major);
        byte_swap(minor);
        byte_swap(glyph_class_offset);
        byte_swap(attach_list_offset);
        byte_swap(lig_caret_offset);
        byte_swap(mark_attach_offset);
    }
};

struct TTClassRangeRecord {
    uint16_t start_glyph_id;
    uint16_t end_glyph_id;
    uint16_t gclass;

    void swap_endian() {
        byte_swap(start_glyph_id);
        byte_swap(end_glyph_id);
        byte_swap(gclass);
    }
};

void TTMaxp10::swap_endian() {
    byte_swap(version);
    byte_swap(num_glyphs);
    byte_swap(max_points);
    byte_swap(max_contours);
    byte_swap(max_composite_points);
    byte_swap(max_composite_contours);
    byte_swap(max_zones);
    byte_swap(max_twilight_points);
    byte_swap(max_storage);
    byte_swap(max_function_defs);
    byte_swap(max_instruction_defs);
    byte_swap(max_stack_elements);
    byte_swap(max_sizeof_instructions);
    byte_swap(max_component_elements);
    byte_swap(max_component_depth);
}

struct TTGlyphHeader {
    int16_t num_contours;
    int16_t x_min;
    int16_t y_min;
    int16_t x_max;
    int16_t y_max;

    void swap_endian() {
        byte_swap(num_contours);
        byte_swap(x_min);
        byte_swap(y_min);
        byte_swap(x_max);
        byte_swap(y_max);
    }
};

void TTHhea::swap_endian() {
    byte_swap(version);
    byte_swap(ascender);
    byte_swap(descender);
    byte_swap(linegap);
    byte_swap(advance_width_max);
    byte_swap(min_left_side_bearing);
    byte_swap(min_right_side_bearing);
    byte_swap(x_max_extent);
    byte_swap(caret_slope_rise);
    byte_swap(caret_slope_run);
    byte_swap(caret_offset);
    byte_swap(reserved0);
    byte_swap(reserved1);
    byte_swap(reserved2);
    byte_swap(reserved3);
    byte_swap(metric_data_format);
    byte_swap(num_hmetrics);
}

void TTLongHorMetric::swap_endian() {
    byte_swap(advance_width);
    byte_swap(lsb);
}

struct TTCmapHeader {
    uint16_t version;
    uint16_t num_tables;

    void swap_endian() {
        byte_swap(version);
        byte_swap(num_tables);
    }
};

struct TTEncodingRecord {
    uint16_t platform_id;
    uint16_t encoding_id;
    int32_t subtable_offset;

    void swap_endian() {
        byte_swap(platform_id);
        byte_swap(encoding_id);
        byte_swap(subtable_offset);
    }
};

struct TTEncodingSubtable0 {
    uint16_t format;
    uint16_t length;
    uint16_t language;
    uint8_t glyphids[256];

    void swap_endian() {
        byte_swap(format);
        byte_swap(length);
        byte_swap(language);
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
        byte_swap(version_major);
        byte_swap(version_minor);
        byte_swap(italic_angle);
        byte_swap(underline_position);
        byte_swap(underline_thickness);
        byte_swap(is_fixed_pitch);
        byte_swap(min_mem_type_42);
        byte_swap(max_mem_type_42);
        byte_swap(min_mem_type_1);
        byte_swap(max_mem_type_1);
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

TTMaxp10 load_maxp(const std::vector<TTDirEntry> &dir, std::string_view buf) {
    auto e = find_entry(dir, "maxp");
    uint32_t version;
    memcpy(&version, buf.data() + e->offset, sizeof(uint32_t));
    byte_swap(version);
    assert(version == 1 << 16);
    TTMaxp10 maxp;
    assert(e->length >= sizeof(maxp));
    memcpy(&maxp, buf.data() + e->offset, sizeof(maxp));
    maxp.swap_endian();
    return maxp;
    std::abort();
}

TTHead load_head(const std::vector<TTDirEntry> &dir, std::string_view buf) {
    auto e = find_entry(dir, "head");
    TTHead head;
    memcpy(&head, buf.data() + e->offset, sizeof(head));
    head.swap_endian();
    assert(head.magic == 0x5f0f3cf5);
    return head;
}

std::vector<int32_t> load_loca(const std::vector<TTDirEntry> &dir,
                               std::string_view buf,
                               uint16_t index_to_loc_format,
                               uint16_t num_glyphs) {
    auto loca = find_entry(dir, "loca");
    std::vector<int32_t> offsets;
    offsets.reserve(num_glyphs);
    if(index_to_loc_format == 0) {
        for(uint16_t i = 0; i <= num_glyphs; ++i) {
            uint16_t offset;
            memcpy(&offset, buf.data() + loca->offset + i * sizeof(uint16_t), sizeof(uint16_t));
            byte_swap(offset);
            offsets.push_back(offset * 2);
        }
    } else {
        assert(index_to_loc_format == 1);
        for(uint16_t i = 0; i <= num_glyphs; ++i) {
            int32_t offset;
            memcpy(&offset, buf.data() + loca->offset + i * sizeof(int32_t), sizeof(int32_t));
            byte_swap(offset);
            offsets.push_back(offset);
        }
    }
    return offsets;
}

TTHhea load_hhea(const std::vector<TTDirEntry> &dir, std::string_view buf) {
    auto e = find_entry(dir, "hhea");

    TTHhea hhea;
    assert(sizeof(TTHhea) == e->length);
    memcpy(&hhea, buf.data() + e->offset, sizeof(hhea));
    hhea.swap_endian();
    assert(hhea.version == 1 << 16);
    assert(hhea.metric_data_format == 0);
    return hhea;
}

TTHmtx load_hmtx(const std::vector<TTDirEntry> &dir,
                 std::string_view buf,
                 uint16_t num_glyphs,
                 uint16_t num_hmetrics) {
    auto e = find_entry(dir, "hmtx");
    TTHmtx hmtx;
    for(uint16_t i = 0; i < num_hmetrics; ++i) {
        TTLongHorMetric hm;
        memcpy(&hm, buf.data() + e->offset + i * sizeof(TTLongHorMetric), sizeof(TTLongHorMetric));
        hm.swap_endian();
        hmtx.longhor.push_back(hm);
    }
    for(int i = 0; i < num_glyphs - num_hmetrics; ++i) {
        int16_t lsb;
        memcpy(&lsb,
               buf.data() + num_hmetrics * sizeof(TTLongHorMetric) + i * sizeof(int16_t),
               sizeof(int16_t));
        byte_swap(lsb);
        hmtx.left_side_bearings.push_back(lsb);
    }
    return hmtx;
}

std::vector<std::string> load_glyphs(const std::vector<TTDirEntry> &dir,
                                     std::string_view buf,
                                     uint16_t num_glyphs,
                                     const std::vector<int32_t> &loca) {
    std::vector<std::string> glyph_data;
    auto e = find_entry(dir, "glyf");
    assert(e);
    const char *glyf_start = buf.data() + e->offset;
    for(uint16_t i = 0; i < num_glyphs; ++i) {
        glyph_data.emplace_back(std::string(glyf_start + loca.at(i), loca.at(i + 1) - loca.at(i)));
        // Eventually unpack to normal glyph or composite.
        /*
        int16_t num_contours;
        memcpy(&num_contours, glyph_data.back().data(), sizeof(num_contours));
        byte_swap(num_contours);
        if(num_contours >= 0) {
        } else {
            const char *composite_data = glyph_data.back().data() + sizeof(int16_t);
            const uint16_t MORE_COMPONENTS = 0x20;
            const uint16_t ARGS_ARE_WORDS = 0x01;
            uint16_t component_flag;
            uint16_t glyph_index;
            do {
                memcpy(&component_flag, composite_data, sizeof(uint16_t));
                byte_swap(component_flag);
                composite_data += sizeof(uint16_t);
                memcpy(&glyph_index, composite_data, sizeof(uint16_t));
                byte_swap(glyph_index);
                composite_data += sizeof(uint16_t);
                if(component_flag & ARGS_ARE_WORDS) {
                    composite_data += 2 * sizeof(int16_t);
                } else {
                    composite_data += 2 * sizeof(int8_t);
                }
            } while(component_flag & MORE_COMPONENTS);
            // Instruction data would be here, but we don't need to parse it.
        }
        */
    }
    return glyph_data;
}

std::string
load_raw_table(const std::vector<TTDirEntry> &dir, std::string_view buf, const char *tag) {
    auto e = find_entry(dir, tag);
    if(!e) {
        return "";
    }
    auto end_offset = e->offset + e->length;
    if(end_offset >= buf.size()) {
        fprintf(stderr, "Directory entry points past the end of file.\n");
        std::abort();
    }
    return std::string(buf.data() + e->offset, buf.data() + end_offset);
}

std::vector<std::string> subset_glyphs(FT_Face face,
                                       const TrueTypeFontFile &source,
                                       const std::vector<TTGlyphs> glyphs,
                                       const std::unordered_map<uint32_t, uint32_t> &comp_mapping) {
    std::vector<std::string> subset;
    assert(std::get<RegularGlyph>(glyphs[0]).unicode_codepoint == 0);
    assert(glyphs.size() < 255);
    for(const auto &g : glyphs) {
        uint32_t gid = font_id_for_glyph(face, g);
        assert(gid < source.glyphs.size());
        subset.push_back(source.glyphs[gid]);
        if(!subset.back().empty()) {
            int16_t num_contours;
            assert(subset.back().size() >= sizeof(int16_t));
            memcpy(&num_contours, subset.back().data(), sizeof(int16_t));
            byte_swap(num_contours);
            if(num_contours < 0) {
                reassign_composite_glyph_numbers(subset.back(), comp_mapping);
            }
        }
    }

    return subset;
}

TTHmtx
subset_hmtx(FT_Face face, const TrueTypeFontFile &source, const std::vector<TTGlyphs> &glyphs) {
    TTHmtx subset;
    assert(source.hmtx.longhor.size() + 1 == source.maxp.num_glyphs);
    for(const auto &g : glyphs) {
        const auto gid = font_id_for_glyph(face, g);
        assert(gid < source.hmtx.longhor.size());
        subset.longhor.push_back(source.hmtx.longhor[gid]);
    }
    subset.left_side_bearings.push_back(source.hmtx.left_side_bearings.front());
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
        byte_swap(offset);
        append_bytes(odata, offset);
    }
    e.length = odata.size() - e.offset;
    directory.push_back(e);

    e.set_tag("hmtx");
    e.offset = odata.size();
    for(auto &hm : tf.hmtx.longhor) {
        hm.swap_endian();
        append_bytes(odata, hm);
    }
    for(auto lsb : tf.hmtx.left_side_bearings) {
        byte_swap(lsb);
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
    byte_swap(adjustment);
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

int16_t num_contours(std::string_view buf) {
    int16_t num_contours;
    memcpy(&num_contours, buf.data(), sizeof(num_contours));
    byte_swap(num_contours);
    return num_contours;
}

} // namespace

TrueTypeFontFile parse_truetype_font(std::string_view buf) {
    TrueTypeFontFile tf;
    TTOffsetTable off;
    memcpy(&off, buf.data(), sizeof(off));
    off.swap_endian();
    std::vector<TTDirEntry> directory;
    for(int i = 0; i < off.num_tables; ++i) {
        TTDirEntry e;
        memcpy(&e, buf.data() + sizeof(off) + i * sizeof(e), sizeof(e));
        e.swap_endian();
        if(e.offset + e.length > buf.length()) {
            throw std::runtime_error("TTF directory entry points outside of file.");
        }
        auto checksum = ttf_checksum(buf.substr(e.offset, e.length));
        (void)checksum;
        directory.emplace_back(std::move(e));
    }
    tf.head = load_head(directory, buf);
    tf.maxp = load_maxp(directory, buf);
    const auto loca = load_loca(directory, buf, tf.head.index_to_loc_format, tf.maxp.num_glyphs);
    tf.hhea = load_hhea(directory, buf);
    tf.hmtx = load_hmtx(directory, buf, tf.maxp.num_glyphs, tf.hhea.num_hmetrics);
    tf.glyphs = load_glyphs(directory, buf, tf.maxp.num_glyphs, loca);

    tf.cvt = load_raw_table(directory, buf, "cvt ");
    tf.fpgm = load_raw_table(directory, buf, "fpgm");
    tf.prep = load_raw_table(directory, buf, "prep");
    auto cmap = load_raw_table(directory, buf, "cmap");
    TTCmapHeader cmap_head;
    memcpy(&cmap_head, cmap.data(), sizeof(TTCmapHeader));
    cmap_head.swap_endian();
    for(uint16_t table_num = 0; table_num < cmap_head.num_tables; ++table_num) {
        TTEncodingRecord enc;
        memcpy(&enc,
               cmap.data() + 2 * sizeof(uint16_t) + table_num * sizeof(TTEncodingRecord),
               sizeof(TTEncodingRecord));
        enc.swap_endian();
        uint16_t subtable_format;
        memcpy(&subtable_format, cmap.data() + enc.subtable_offset, sizeof(subtable_format));
        byte_swap(subtable_format);
        assert(subtable_format < 15);
        if(subtable_format == 0) {
            TTEncodingSubtable0 enctable;
            memcpy(&enctable, cmap.data() + enc.subtable_offset, sizeof(TTEncodingSubtable0));
            enctable.swap_endian();
            assert(enctable.format == 0);
        }
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
            byte_swap(classdef_version);
            assert(classdef_version == 2);
            uint16_t num_records;
            memcpy(&num_records,
                   buf.data() + e.offset + gdef.glyph_class_offset + sizeof(classdef_version),
                   sizeof(num_records));
            byte_swap(num_records);
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

std::string generate_font(FT_Face face,
                          std::string_view buf,
                          const std::vector<TTGlyphs> &glyphs,
                          const std::unordered_map<uint32_t, uint32_t> &comp_mapping) {
    auto source = parse_truetype_font(buf);
    TrueTypeFontFile dest;
    assert(std::get<RegularGlyph>(glyphs[0]).unicode_codepoint == 0);
    dest.glyphs = subset_glyphs(face, source, glyphs, comp_mapping);

    dest.head = source.head;
    // https://learn.microsoft.com/en-us/typography/opentype/spec/otff#calculating-checksums
    dest.head.checksum_adjustment = 0;
    dest.hhea = source.hhea;
    dest.maxp = source.maxp;
    dest.maxp.num_glyphs = dest.glyphs.size();
    dest.hmtx = subset_hmtx(face, source, glyphs);
    dest.hhea.num_hmetrics = dest.hmtx.longhor.size();
    dest.head.index_to_loc_format = 1;
    dest.cvt = source.cvt;
    dest.fpgm = source.fpgm;
    dest.prep = source.prep;
    dest.cmap = gen_cmap(glyphs);

    auto bytes = serialize_font(dest);
    return bytes;
}

TrueTypeFontFile load_and_parse_truetype_font(const char *fname) {
    FILE *f = fopen(fname, "rb");
    if(!f) {
        std::abort();
    }
    fseek(f, 0, SEEK_END);
    const auto fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buf(fsize, 0);
    if(fread(buf.data(), 1, fsize, f) != (size_t)fsize) {
        std::abort();
    }
    fclose(f);
    return parse_truetype_font(std::string_view{buf.data(), buf.size()});
}

bool is_composite_glyph(std::string_view buf) { return num_contours(buf) < 0; }

std::vector<uint32_t> composite_subglyphs(std::string_view buf) {
    std::vector<uint32_t> subglyphs;
    assert(num_contours(buf) < 0);
    const char *composite_data = buf.data() + 5 * sizeof(int16_t);
    const uint16_t MORE_COMPONENTS = 0x20;
    const uint16_t ARGS_ARE_WORDS = 0x01;
    uint16_t component_flag;
    uint16_t glyph_index;
    do {
        memcpy(&component_flag, composite_data, sizeof(uint16_t));
        byte_swap(component_flag);
        composite_data += sizeof(uint16_t);
        memcpy(&glyph_index, composite_data, sizeof(uint16_t));
        byte_swap(glyph_index);
        composite_data += sizeof(uint16_t);
        if(component_flag & ARGS_ARE_WORDS) {
            composite_data += 2 * sizeof(int16_t);
        } else {
            composite_data += 2 * sizeof(int8_t);
        }
        subglyphs.push_back(glyph_index);
    } while(component_flag & MORE_COMPONENTS);
    return subglyphs;
}

void reassign_composite_glyph_numbers(std::string &buf,
                                      const std::unordered_map<uint32_t, uint32_t> &mapping) {
    const char *composite_data = buf.data() + 5 * sizeof(int16_t);
    const uint16_t MORE_COMPONENTS = 0x20;
    const uint16_t ARGS_ARE_WORDS = 0x01;
    uint16_t component_flag;
    uint16_t glyph_index;
    do {
        memcpy(&component_flag, composite_data, sizeof(uint16_t));
        byte_swap(component_flag);
        composite_data += sizeof(uint16_t);
        auto index_address = (char *)composite_data;
        memcpy(&glyph_index, composite_data, sizeof(uint16_t));
        byte_swap(glyph_index);
        composite_data += sizeof(uint16_t);
        if(component_flag & ARGS_ARE_WORDS) {
            composite_data += 2 * sizeof(int16_t);
        } else {
            composite_data += 2 * sizeof(int8_t);
        }
        auto it = mapping.find(glyph_index);
        if(it == mapping.end()) {
            fprintf(stderr, "FAILfailFAIL\n");
            std::abort();
        }
        glyph_index = it->second;
        byte_swap(glyph_index);
        memcpy(index_address, &glyph_index, sizeof(glyph_index));
    } while(component_flag & MORE_COMPONENTS);
}

uint32_t font_id_for_glyph(FT_Face face, const A4PDF::TTGlyphs &g) {
    if(std::holds_alternative<A4PDF::RegularGlyph>(g)) {
        auto &rg = std::get<A4PDF::RegularGlyph>(g);
        if(rg.unicode_codepoint == 0) {
            return 0;
        } else {
            return FT_Get_Char_Index(face, rg.unicode_codepoint);
        }
    } else if(std::holds_alternative<A4PDF::CompositeGlyph>(g)) {
        return std::get<A4PDF::CompositeGlyph>(g).font_index;
    } else {
        std::abort();
    }
}

} // namespace A4PDF
