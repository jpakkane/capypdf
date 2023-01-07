/*
 * Copyright 2022 Jussi Pakkanen
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

#include <vector>
#include <string>

#include <cassert>
#include <byteswap.h>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include FT_OPENTYPE_VALIDATE_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H

namespace {

void byte_swap(int16_t &val) { val = bswap_16(val); }
void byte_swap(uint16_t &val) { val = bswap_16(val); }
void byte_swap(int32_t &val) { val = bswap_32(val); }
void byte_swap(uint32_t &val) { val = bswap_32(val); }
void byte_swap(int64_t &val) { val = bswap_64(val); }
void byte_swap(uint64_t &val) { val = bswap_64(val); }

} // namespace

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

struct TTHead {
    int32_t version;
    int32_t revision;
    uint32_t checksum_adjustment;
    uint32_t magic;
    uint16_t flags;
    uint16_t units_per_em;
    int64_t created;
    int64_t modified;
    int16_t x_min;
    int16_t y_min;
    int16_t x_max;
    int16_t y_max;
    uint16_t mac_style;
    uint16_t lowest_rec_pppem;
    int16_t font_direction_hint;
    int16_t index_to_loc_format;
    int16_t glyph_data_format;

    void swap_endian() {
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
};

struct TTDirEntry {
    char tag[4];
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;

    void swap_endian() {
        // byte_swap(tag);
        byte_swap(checksum);
        byte_swap(offset);
        byte_swap(length);
    }
};

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

uint32_t str2tag(const unsigned char *txt) {
    return (txt[0] << 24) + (txt[1] << 16) + (txt[2] << 8) + txt[3];
}

uint32_t str2tag(const char *txt) { return str2tag((const unsigned char *)txt); }

void write_font(const char *ofname, FT_Face face, const std::vector<uint32_t> &glyphs) {
    SubsetFont sf;
    FT_ULong bufsize = 0; // sizeof(sf.head);
    TT_Header *tmp = static_cast<TT_Header *>(FT_Get_Sfnt_Table(face, FT_SFNT_HEAD));
    // static_assert(sizeof(TT_Header) == sizeof(TTHead));
    static_assert(sizeof(TT_Header) == 96);
    printf("Sizeof TT_HEADER: %d\n", (int)sizeof(TT_Header));
    printf("Sizeof TTHead: %d\n", (int)sizeof(TTHead));
    printf("Sizeof long: %d\n", (int)sizeof(long));
    auto ec =
        FT_Load_Sfnt_Table(face, TTAG_head, 0, reinterpret_cast<FT_Byte *>(&sf.head), nullptr);
    assert(ec == 0);
    sf.head.swap_endian();
    assert(sf.head.magic == 0x5f0f3cf5);
    sf.offset.set_table_size(0xa);
    sf.directory.emplace_back(TTDirEntry{{'c', 'm', 'a', 'p'}, 0, 0, 0});
    FILE *f = fopen(ofname, "w");
    sf.swap_endian();
    fwrite(&sf.offset, 1, sizeof(TTOffsetTable), f);
    for(const auto &e : sf.directory) {
        fwrite(&e, 1, sizeof(TTDirEntry), f);
    }
    fclose(f);
}

void debug_font(const char *ifile) {
    FILE *f = fopen(ifile, "r");
    fseek(f, 0, SEEK_END);
    const auto fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buf(fsize, 0);
    fread(buf.data(), 1, fsize, f);
    TTOffsetTable off;
    memcpy(&off, buf.data(), sizeof(off));
    off.swap_endian();
    std::vector<TTDirEntry> directory;
    for(int i = 0; i < off.num_tables; ++i) {
        TTDirEntry e;
        memcpy(&e, buf.data() + sizeof(off) + i * sizeof(e), sizeof(e));
        e.swap_endian();
        directory.emplace_back(std::move(e));
    }
    for(const auto &e : directory) {
        char tagbuf[5];
        tagbuf[4] = 0;
        memcpy(tagbuf, e.tag, 4);
        printf("%s off: %d size: %d\n", tagbuf, e.offset, e.length);
        if(strncmp(e.tag, "head", 4) == 0) {
            TTHead head;
            memcpy(&head, buf.data() + e.offset, sizeof(head));
            head.swap_endian();
            assert(head.magic == 0x5f0f3cf5);
        }
    }
    fclose(f);
}

} // namespace

int main(int, char **) {
    const char *fontfile = "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf";
    //  const char *fontfile = "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf";
    //   const char *fontfile = "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf";
    // const char *fontfile = "font2.ttf";
    const char *outfile = "font_dump.ttf";
    FT_Library ft;
    auto ec = FT_Init_FreeType(&ft);
    if(ec) {
        printf("FT init fail.\n");
        return 1;
    }
    FT_Face face;
    ec = FT_New_Face(ft, fontfile, 0, &face);
    if(ec) {
        printf("Font opening failed.\n");
        return 1;
    }
    printf("Font opened successfully.\n");
    std::vector<uint32_t> glyphs{'A'};
    debug_font(fontfile);
    write_font(outfile, face, glyphs);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return 0;
}
