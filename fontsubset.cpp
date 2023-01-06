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

struct TTOffsetTable {
    int32_t scaler = 0x10000;
    int16_t num_tables;
    int16_t search_range;
    int16_t entry_selector;
    int16_t range_shift;

    void swap_endian() {
        scaler = bswap_32(scaler);
        num_tables = bswap_16(num_tables);
        search_range = bswap_16(search_range);
        entry_selector = bswap_16(entry_selector);
        range_shift = bswap_16(range_shift);
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

struct TTDirEntry {
    uint32_t tag;
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;

    void swap_endian() {
        tag = bswap_32(tag);
        checksum = bswap_32(checksum);
        offset = bswap_32(offset);
        length = bswap_32(length);
    }
};

struct SubsetFont {
    TTOffsetTable offset;
    std::vector<TTDirEntry> directory;

    void swap_endian() {
        offset.swap_endian();
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

void write_font(const char *ofname, SubsetFont &sf) {
    FILE *f = fopen(ofname, "w");
    sf.swap_endian();
    fwrite(&sf.offset, 1, sizeof(TTOffsetTable), f);
    for(const auto &e : sf.directory) {
        fwrite(&e, 1, sizeof(TTDirEntry), f);
    }
    fclose(f);
}

} // namespace

int main(int, char **) {
    const char *fontfile = "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf";
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
    SubsetFont sf;
    sf.offset.set_table_size(0xa);
    sf.directory.emplace_back(TTDirEntry{str2tag("cmap"), 0, 0, 0});
    write_font(outfile, sf);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return 0;
}
