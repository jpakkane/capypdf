// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <ft_subsetter.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cassert>

#include <bit>

using namespace capypdf::internal;

int main(int argc, char **argv) {
    if(argc != 3) {
        printf("%s <input_font.ttf> <output_font.ttf>\n", argv[0]);
        return 1;
    }
    std::string letters("ABCdef123");
    FT_Library ft;
    FT_Init_FreeType(&ft);
    FT_Face face;
    FT_New_Face(ft, argv[1], 0, &face);

    FontProperties fprop;
    auto rc = load_and_parse_font_file(argv[1], fprop);
    auto &font = std::get<TrueTypeFontFile>(rc.value());

    std::vector<TTGlyphs> glyphs;
    std::unordered_map<uint32_t, uint32_t> composite_mapping;

    glyphs.emplace_back(RegularGlyph{0, 0});
    for(const char c : letters) {
        auto glyph_id = FT_Get_Char_Index(face, c);
        glyphs.emplace_back(RegularGlyph(c, glyph_id));
    }

    auto rc2 = generate_font(font, face, glyphs, composite_mapping);
    const auto &bytes = rc2.value();

    FILE *f = fopen(argv[2], "wb");
    assert(f);
    fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return 0;
}
