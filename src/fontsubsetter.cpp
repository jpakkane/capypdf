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

FontSubsetData create_startstate() {
    std::vector<TTGlyphs> start_state{RegularGlyph{0}};
    std::unordered_map<uint32_t, uint32_t> start_mapping{};
    return FontSubsetData{std::move(start_state), std::move(start_mapping)};
}

void add_subglyphs(std::unordered_set<uint32_t> &new_subglyphs,
                   uint32_t glyph_id,
                   const TrueTypeFontFile &ttfile) {
    const auto &cur_glyph = ttfile.glyphs.at(glyph_id);
    if(!is_composite_glyph(cur_glyph)) {
        return;
    }
    for(const auto &g : composite_subglyphs(cur_glyph)) {
        if(new_subglyphs.insert(g).second) {
            add_subglyphs(new_subglyphs, g, ttfile);
        }
    }
}

std::vector<uint32_t> get_all_subglyphs(uint32_t glyph_id, const TrueTypeFontFile &ttfile) {
    std::unordered_set<uint32_t> new_subglyphs;

    add_subglyphs(new_subglyphs, glyph_id, ttfile);
    std::vector<uint32_t> glyphs(new_subglyphs.cbegin(), new_subglyphs.cend());
    return glyphs;
}

} // namespace

FontSubsetter::FontSubsetter(const char *fontfile, FT_Face face)
    : ttfile(load_and_parse_truetype_font(fontfile)), face{face} {
    subsets.emplace_back(create_startstate());
}

FontSubsetInfo FontSubsetter::get_glyph_subset(uint32_t glyph) {
    auto trial = find_glyph(glyph);
    if(trial) {
        return trial.value();
    }
    if(subsets.back().glyphs.size() == max_glyphs) {
        subsets.emplace_back(create_startstate());
    }
    const auto font_index = FT_Get_Char_Index(face, glyph);
    if(is_composite_glyph(ttfile.glyphs.at(font_index))) {
        auto subglyphs = get_all_subglyphs(font_index, ttfile);
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

std::string
FontSubsetter::generate_subset(FT_Face face, std::string_view data, int32_t subset_number) const {
    const auto &glyphs = subsets.at(subset_number);
    return generate_font(face, data, glyphs.glyphs, glyphs.font_index_mapping);
}

} // namespace A4PDF
