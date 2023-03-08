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

#pragma once

#include <ft_subsetter.hpp>

#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <cstdint>
#include <unordered_map>

typedef struct FT_FaceRec_ *FT_Face;

namespace A4PDF {

static const std::size_t max_glyphs = 255;

struct FontSubsetInfo {
    int32_t subset;
    int32_t offset;
};

struct FontBlahblah {
    std::vector<uint32_t> codepoints;
    std::unordered_map<uint32_t, uint32_t> font_index_mapping;
};

class FontSubsetter {
public:
    FontSubsetter(const char *fontfile, FT_Face face);

    FontSubsetInfo get_glyph_subset(uint32_t glyph);

    const std::vector<uint32_t> &get_subset(int32_t subset_number) const {
        return subsets.at(subset_number).codepoints;
    }

    std::string generate_subset(FT_Face face, std::string_view data, int32_t subset_number) const;

private:
    TrueTypeFontFile ttfile;
    FT_Face face;
    std::optional<FontSubsetInfo> find_glyph(uint32_t glyph) const;

    std::vector<FontBlahblah> subsets;
};

} // namespace A4PDF
