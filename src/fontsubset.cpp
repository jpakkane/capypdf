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

#include <ft_subsetter.hpp>

#include <vector>
#include <string>
#include <variant>

#include <cassert>
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

void write_font(const char *ofname,
                FT_Face face,
                std::string_view source,
                const std::vector<uint32_t> &glyphs) {
    auto bytes = generate_font(face, source, glyphs);
    FILE *f = fopen(ofname, "w");
    if(fwrite(bytes.data(), 1, bytes.length(), f) != bytes.length()) {
        printf("Writing to file failed: %s\n", strerror(errno));
        std::abort();
    }

    if(fclose(f) != 0) {
        printf("Closing output file failed: %s\n", strerror(errno));
        std::abort();
    }
}

} // namespace

int main(int argc, char **argv) {
    const char *fontfile;
    if(argc > 1) {
        fontfile = argv[1];
    } else {
        fontfile = "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf";
    }
    //  const char *fontfile = "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf";
    //   const char *fontfile =
    //   "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf";
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

    FILE *f = fopen(fontfile, "r");
    fseek(f, 0, SEEK_END);
    const auto fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buf(fsize, 0);
    fread(buf.data(), 1, fsize, f);
    fclose(f);

    std::vector<uint32_t> glyphs{0, 'A', 'B', '0', '&', '+', 'z'};
    // std::abort();
    write_font(outfile, face, std::string_view{buf.data(), buf.size()}, glyphs);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return 0;
}
