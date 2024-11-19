// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <fontsubsetter.hpp>
#include <ft_subsetter.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdio>

#include <unordered_set>
#include <algorithm>

namespace capypdf::internal {

namespace {

const uint32_t SPACE = ' ';

FontSubsetData create_startstate() {
    std::vector<TTGlyphs> start_state{RegularGlyph{0}};
    std::unordered_map<uint32_t, uint32_t> start_mapping{};
    return FontSubsetData{std::move(start_state), std::move(start_mapping)};
}

rvoe<NoReturnValue> add_subglyphs(std::unordered_set<uint32_t> &new_subglyphs,
                                  uint32_t glyph_id,
                                  const TrueTypeFontFile &ttfile) {
    const auto &cur_glyph = ttfile.glyphs.at(glyph_id);
    ERC(iscomp, is_composite_glyph(cur_glyph));
    if(!iscomp) {
        RETOK;
    }
    ERC(subglyphs, composite_subglyphs(cur_glyph));
    for(const auto &g : subglyphs) {
        if(new_subglyphs.insert(g).second) {
            ERCV(add_subglyphs(new_subglyphs, g, ttfile));
        }
    }
    RETOK;
}

rvoe<std::vector<uint32_t>> get_all_subglyphs(uint32_t glyph_id, const TrueTypeFontFile &ttfile) {
    std::unordered_set<uint32_t> new_subglyphs;

    ERCV(add_subglyphs(new_subglyphs, glyph_id, ttfile));
    std::vector<uint32_t> glyphs(new_subglyphs.cbegin(), new_subglyphs.cend());
    return glyphs;
}

} // namespace

rvoe<FontSubsetter> FontSubsetter::construct(const std::filesystem::path &fontfile, FT_Face face) {
    ERC(ttfile, load_and_parse_truetype_font(fontfile));
    std::vector<FontSubsetData> subsets;
    subsets.emplace_back(create_startstate());
    return FontSubsetter(std::move(ttfile), face, std::move(subsets));
}

rvoe<FontSubsetInfo> FontSubsetter::get_glyph_subset(uint32_t codepoint,
                                                     const std::optional<uint32_t> glyph_id) {
    // FIXME, look for glyph id.
    auto trial = find_glyph(codepoint);
    if(trial) {
        return trial.value();
    }
    return unchecked_insert_glyph_to_last_subset(codepoint, glyph_id);
}

rvoe<FontSubsetInfo> FontSubsetter::get_glyph_subset(const u8string &text,
                                                     const uint32_t glyph_id) {
    auto trial = find_glyph(text);
    if(trial) {
        return trial.value();
    }
    return unchecked_insert_glyph_to_last_subset(text, glyph_id);
}

rvoe<FontSubsetInfo>
FontSubsetter::unchecked_insert_glyph_to_last_subset(const uint32_t codepoint,
                                                     const std::optional<uint32_t> glyph_id) {
    if(subsets.back().glyphs.size() == max_glyphs) {
        subsets.emplace_back(create_startstate());
    }
    const uint32_t glyph_index = glyph_id ? glyph_id.value() : FT_Get_Char_Index(face, codepoint);
    if(glyph_index == 0) {
        RETERR(MissingGlyph);
    }
    if(codepoint == SPACE) {
        // In the PDF document model the space character is special.
        // Every subset font _must_ have the space character in
        // location 32.
        if(subsets.back().glyphs.size() == SPACE) {
            subsets.back().glyphs.push_back(RegularGlyph{codepoint, glyph_index});
            subsets.back().font_index_mapping[glyph_index] =
                (uint32_t)subsets.back().glyphs.size() - 1;
        }
        return FontSubsetInfo{int32_t(subsets.size() - 1), SPACE};
    }
    ERCV(handle_subglyphs(glyph_index));
    if(subsets.back().glyphs.size() == SPACE) {
        // NOTE: the case where the subset font has fewer than 32 characters
        // is handled when serializing the font.
        subsets.back().glyphs.emplace_back(RegularGlyph{SPACE, FT_Get_Char_Index(face, SPACE)});
        subsets.back().font_index_mapping[glyph_index] = SPACE;
    }
    subsets.back().glyphs.push_back(RegularGlyph{codepoint, glyph_index});
    subsets.back().font_index_mapping[glyph_index] = (uint32_t)subsets.back().glyphs.size() - 1;
    return FontSubsetInfo{int32_t(subsets.size() - 1), int32_t(subsets.back().glyphs.size() - 1)};
}

rvoe<NoReturnValue> FontSubsetter::handle_subglyphs(uint32_t glyph_index) {
    if(glyph_index == 0 || glyph_index >= ttfile.glyphs.size()) {
        RETERR(MissingGlyph);
    }
    ERC(iscomp, is_composite_glyph(ttfile.glyphs.at(glyph_index)));
    if(iscomp) {
        ERC(subglyphs, get_all_subglyphs(glyph_index, ttfile));
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
    }
    RETOK;
}

rvoe<FontSubsetInfo> FontSubsetter::unchecked_insert_glyph_to_last_subset(const u8string &text,
                                                                          uint32_t glyph_id) {
    if(subsets.back().glyphs.size() == max_glyphs) {
        subsets.emplace_back(create_startstate());
    }
    if(subsets.back().glyphs.size() == SPACE) {
        // NOTE: the case where the subset font has fewer than 32 characters
        // is handled when serializing the font.
        subsets.back().glyphs.emplace_back(RegularGlyph{SPACE, FT_Get_Char_Index(face, SPACE)});
        subsets.back().font_index_mapping[glyph_id] = SPACE;
    }
    ERCV(handle_subglyphs(glyph_id));
    subsets.back().glyphs.push_back(LigatureGlyph{text, glyph_id});
    subsets.back().font_index_mapping[glyph_id] = (uint32_t)subsets.back().glyphs.size() - 1;
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

std::optional<FontSubsetInfo> FontSubsetter::find_glyph(const u8string &text) const {
    for(size_t subset = 0; subset < subsets.size(); ++subset) {
        auto loc = std::find_if(subsets.at(subset).glyphs.cbegin(),
                                subsets.at(subset).glyphs.cend(),
                                [&text](const TTGlyphs &ttg) {
                                    if(std::holds_alternative<LigatureGlyph>(ttg)) {
                                        return std::get<LigatureGlyph>(ttg).text == text;
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

rvoe<std::string> FontSubsetter::generate_subset(FT_Face face,
                                                 const TrueTypeFontFile &source,
                                                 int32_t subset_number) const {
    const auto &glyphs = subsets.at(subset_number);
    return generate_font(face, source, glyphs.glyphs, glyphs.font_index_mapping);
}

} // namespace capypdf::internal
