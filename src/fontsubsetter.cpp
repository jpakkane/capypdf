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

#include <fontsubsetter.hpp>
#include <ft_subsetter.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdio>

#include <unordered_set>
#include <algorithm>

namespace A4PDF {

namespace {

const uint32_t SPACE = ' ';

FontSubsetData create_startstate() {
    std::vector<TTGlyphs> start_state{RegularGlyph{0}};
    std::unordered_map<uint32_t, uint32_t> start_mapping{};
    return FontSubsetData{std::move(start_state), std::move(start_mapping)};
}

std::expected<NoReturnValue, ErrorCode> add_subglyphs(std::unordered_set<uint32_t> &new_subglyphs,
                                                      uint32_t glyph_id,
                                                      const TrueTypeFontFile &ttfile) {
    const auto &cur_glyph = ttfile.glyphs.at(glyph_id);
    ERC(iscomp, is_composite_glyph(cur_glyph));
    if(!iscomp) {
        return NoReturnValue{};
    }
    ERC(subglyphs, composite_subglyphs(cur_glyph));
    for(const auto &g : subglyphs) {
        if(new_subglyphs.insert(g).second) {
            ERCV(add_subglyphs(new_subglyphs, g, ttfile));
        }
    }
    return NoReturnValue{};
}

std::expected<std::vector<uint32_t>, ErrorCode> get_all_subglyphs(uint32_t glyph_id,
                                                                  const TrueTypeFontFile &ttfile) {
    std::unordered_set<uint32_t> new_subglyphs;

    ERCV(add_subglyphs(new_subglyphs, glyph_id, ttfile));
    std::vector<uint32_t> glyphs(new_subglyphs.cbegin(), new_subglyphs.cend());
    return glyphs;
}

} // namespace

std::expected<FontSubsetter, ErrorCode> FontSubsetter::construct(const char *fontfile,
                                                                 FT_Face face) {
    ERC(ttfile, load_and_parse_truetype_font(fontfile));
    std::vector<FontSubsetData> subsets;
    subsets.emplace_back(create_startstate());
    return FontSubsetter(std::move(ttfile), face, std::move(subsets));
}

std::expected<FontSubsetInfo, ErrorCode> FontSubsetter::get_glyph_subset(uint32_t glyph) {
    auto trial = find_glyph(glyph);
    if(trial) {
        return trial.value();
    }
    return unchecked_insert_glyph_to_last_subset(glyph);
}

std::expected<FontSubsetInfo, ErrorCode>
FontSubsetter::unchecked_insert_glyph_to_last_subset(uint32_t glyph) {
    if(subsets.back().glyphs.size() == max_glyphs) {
        subsets.emplace_back(create_startstate());
    }
    if(glyph == SPACE) {
        // In the PDF document model the space character is special.
        // Every subset font _must_ have the space character in
        // location 32.
        if(subsets.back().glyphs.size() == SPACE) {
            const auto space_index = FT_Get_Char_Index(face, glyph);
            subsets.back().glyphs.push_back(RegularGlyph{glyph});
            subsets.back().font_index_mapping[space_index] =
                (uint32_t)subsets.back().glyphs.size() - 1;
        }
        return FontSubsetInfo{int32_t(subsets.size() - 1), SPACE};
    }
    const auto font_index = FT_Get_Char_Index(face, glyph);
    if(subsets.back().glyphs.size() == SPACE) {
        // NOTE: the case where the subset font has fewer than 32 characters
        // is handled when serializing the font.
        subsets.back().glyphs.emplace_back(RegularGlyph{32});
        subsets.back().font_index_mapping[font_index] = SPACE;
    }
    ERC(iscomp, is_composite_glyph(ttfile.glyphs.at(font_index)));
    if(iscomp) {
        ERC(subglyphs, get_all_subglyphs(font_index, ttfile));
        if(subglyphs.size() + subsets.back().glyphs.size() >= max_glyphs) {
            fprintf(stderr, "Composite glyph overflow not yet implemented.");
            std::abort();
        }
        for(const auto &new_glyph : subglyphs) {
            if(subsets.back().font_index_mapping.find(new_glyph) !=
               subsets.back().font_index_mapping.end()) {
                continue;
            }
            // Composite glyph parts do not necessarily correspond to any Unicode codepoint.
            subsets.back().glyphs.push_back(CompositeGlyph{new_glyph});
            subsets.back().font_index_mapping[new_glyph] =
                (uint32_t)subsets.back().glyphs.size() - 1;
        }
        subsets.back().glyphs.push_back(RegularGlyph{glyph});
        subsets.back().font_index_mapping[font_index] = (uint32_t)subsets.back().glyphs.size() - 1;
    } else {
        subsets.back().glyphs.push_back(RegularGlyph{glyph});
        subsets.back().font_index_mapping[font_index] = (uint32_t)subsets.back().glyphs.size() - 1;
    }
    return FontSubsetInfo{int32_t(subsets.size() - 1), int32_t(subsets.back().glyphs.size() - 1)};
}

std::optional<FontSubsetInfo> FontSubsetter::find_glyph(uint32_t glyph) const {
    for(size_t subset = 0; subset < subsets.size(); ++subset) {
        auto loc =
            std::find_if(subsets.at(subset).glyphs.cbegin(),
                         subsets.at(subset).glyphs.cend(),
                         [&glyph](const TTGlyphs &ttg) {
                             if(std::holds_alternative<RegularGlyph>(ttg)) {
                                 return std::get<RegularGlyph>(ttg).unicode_codepoint == glyph;
                             }
                             return false;
                         });
        if(loc != subsets[subset].glyphs.cend()) {
            return FontSubsetInfo{int32_t(subset),
                                  int32_t(loc - subsets.at(subset).glyphs.cbegin())};
        }
    }
    return {};
}

std::expected<std::string, ErrorCode> FontSubsetter::generate_subset(FT_Face face,
                                                                     const TrueTypeFontFile &source,
                                                                     int32_t subset_number) const {
    const auto &glyphs = subsets.at(subset_number);
    return generate_font(face, source, glyphs.glyphs, glyphs.font_index_mapping);
}

} // namespace A4PDF
