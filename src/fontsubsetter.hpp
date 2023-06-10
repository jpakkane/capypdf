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

#include <filesystem>
#include <ft_subsetter.hpp>

#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <cstdint>
#include <unordered_map>
#include <variant>

typedef struct FT_FaceRec_ *FT_Face;

namespace capypdf {

static const std::size_t max_glyphs = 255;

struct FontSubsetInfo {
    int32_t subset;
    int32_t offset;
};

struct FontSubsetData {
    std::vector<TTGlyphs> glyphs;
    std::unordered_map<uint32_t, uint32_t> font_index_mapping;
};

class FontSubsetter {
public:
    static rvoe<FontSubsetter> construct(const std::filesystem::path &fontfile, FT_Face face);

    FontSubsetter(TrueTypeFontFile ttfile, FT_Face face, std::vector<FontSubsetData> subsets)
        : ttfile{ttfile}, face{face}, subsets{subsets} {}

    rvoe<FontSubsetInfo> get_glyph_subset(uint32_t glyph);
    rvoe<FontSubsetInfo> unchecked_insert_glyph_to_last_subset(uint32_t glyph);

    const std::vector<TTGlyphs> &get_subset(int32_t subset_number) const {
        return subsets.at(subset_number).glyphs;
    }

    std::vector<TTGlyphs> &get_subset(int32_t subset_number) {
        return subsets.at(subset_number).glyphs;
    }

    size_t num_subsets() const { return subsets.size(); }
    size_t subset_size(size_t subset) const { return subsets.at(subset).glyphs.size(); }

    rvoe<std::string>
    generate_subset(FT_Face face, const TrueTypeFontFile &source, int32_t subset_number) const;

private:
    TrueTypeFontFile ttfile;
    FT_Face face;
    std::optional<FontSubsetInfo> find_glyph(uint32_t glyph) const;

    std::vector<FontSubsetData> subsets;
};

} // namespace capypdf
