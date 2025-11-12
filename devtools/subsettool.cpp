// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

import std;

#include <ft_subsetter.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H

#include <cassert>

using namespace capypdf::internal;

int main(int argc, char **argv) {
    if(argc != 3) {
        printf("%s <input_font.ttf> <output_font.ttf>\n", argv[0]);
        return 1;
    }
    FontProperties fprop;
    fprop.variations["wght"] = 800;
    std::string letters("ABCdef123");
    FT_Library ft;
    FT_Init_FreeType(&ft);
    FT_Face face;
    FT_New_Face(ft, argv[1], 0, &face);

    FT_MM_Var *mmvar;
    if(FT_Get_MM_Var(face, &mmvar) != 0) {
        std::abort();
    }
    const auto num_axis = mmvar->num_axis;

    std::vector<FT_Fixed> var_coords(num_axis);
    if(FT_Get_Var_Design_Coordinates(face, num_axis, var_coords.data()) != 0) {
        std::abort();
    }

    for(const auto &[key, value] : fprop.variations) {
        bool matched = false;
        char unpacked_tag[5] = {0, 0, 0, 0};
        for(unsigned int i = 0; i < num_axis; ++i) {
            auto &ca = mmvar->axis[i];
            const auto swapped = std::byteswap(int32_t(ca.tag));
            memcpy(unpacked_tag, &swapped, 4);
            if(key == unpacked_tag) {
                matched = true;
                var_coords[i] = value << 16; // Stored in OpenType as 16x16 fixed point.
                break;
            }
        }
        if(!matched) {
            std::abort();
        }
    }
    if(FT_Set_Var_Design_Coordinates(face, num_axis, var_coords.data()) != 0) {
        std::abort();
    }

    auto rc = load_and_parse_font_file(argv[1], fprop);
    auto &font = std::get<TrueTypeFontFile>(rc.value());

    std::vector<TTGlyphs> glyphs;
    std::unordered_map<uint32_t, uint32_t> composite_mapping;

    glyphs.emplace_back(RegularGlyph{0, 0});
    for(const char c : letters) {
        auto glyph_id = FT_Get_Char_Index(face, c);
        glyphs.emplace_back(RegularGlyph(c, glyph_id));
    }

    auto rc2 = generate_font(font, fprop, face, glyphs, composite_mapping);
    const auto &bytes = rc2.value();

    FILE *f = fopen(argv[2], "wb");
    assert(f);
    fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);

    FT_Done_MM_Var(ft, mmvar);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return 0;
}
