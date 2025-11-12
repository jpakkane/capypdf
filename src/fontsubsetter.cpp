// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

import std;

#include <fontsubsetter.hpp>
#include <ft_subsetter.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdio>
#include <cassert>

namespace capypdf::internal {

namespace {

FontSubsetData create_startstate() {
    std::vector<TTGlyphs> start_state{RegularGlyph{0, (uint32_t)-1}};
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

rvoe<FontSubsetter>
FontSubsetter::construct(const char *&fontfile, FT_Face face, const FontProperties &props) {
    ERC(font, load_and_parse_font_file(fontfile, props));
    if(auto *ttffile = std::get_if<TrueTypeFontFile>(&font)) {
        return FontSubsetter(std::move(*ttffile), face, create_startstate(), props);
    } else {
        fprintf(stderr, "Only basic Truetype fonts supported currently.\n");
        RETERR(UnsupportedFormat);
    }
}

rvoe<FontSubsetInfo> FontSubsetter::get_glyph_subset(uint32_t codepoint,
                                                     const std::optional<uint32_t> glyph_id) {
    if(glyph_id) {
        auto existing_gid = find_existing_glyph(*glyph_id);
        if(existing_gid) {
            const auto &existing_definition = subset.glyphs.at(existing_gid->offset);
            if(const auto *d = std::get_if<RegularGlyph>(&existing_definition)) {
                if(d->unicode_codepoint != codepoint) {
                    fprintf(
                        stderr,
                        "Tried to map glyph %d twice.\n Old value: %d\n New value: %d.\nUsing the "
                        "old value.\n",
                        *glyph_id,
                        d->unicode_codepoint,
                        codepoint);
                }
            } else {
                fprintf(stderr,
                        "Tried to map existing glyph %d to a different codepoint.\n",
                        *glyph_id);
            }
            return *existing_gid;
        }
    } else {
        auto trial = find_glyph_with_codepoint(codepoint);
        if(trial) {
            return trial.value();
        }
    }
    return unchecked_insert_glyph_to_last_subset(codepoint, glyph_id);
}

rvoe<FontSubsetInfo> FontSubsetter::get_glyph_subset(const u8string &text,
                                                     const uint32_t glyph_id) {
    auto existing_gid = find_existing_glyph(glyph_id);
    if(existing_gid) {
        const auto &existing_definition = subset.glyphs.at(existing_gid->offset);
        if(const auto *d = std::get_if<LigatureGlyph>(&existing_definition)) {
            if(d->text != text) {
                fprintf(stderr,
                        "Tried to map glyph %d twice.\n Old value: %s\n New value: %s.\nUsing the "
                        "old value.\n",
                        glyph_id,
                        d->text.c_str(),
                        text.c_str());
            }
        } else {
            fprintf(stderr,
                    "Tried to map existing glyph %d to a different text representation.\n",
                    glyph_id);
        }
        return *existing_gid;
    }
    auto trial = find_glyph(text);
    if(trial) {
        return trial.value();
    }
    return unchecked_insert_glyph_to_last_subset(text, glyph_id);
}

rvoe<FontSubsetInfo>
FontSubsetter::unchecked_insert_glyph_to_last_subset(const uint32_t codepoint,
                                                     const std::optional<uint32_t> glyph_id) {
    if(subset.glyphs.size() >= max_glyphs) {
        RETERR(TooManyGlyphsUsed);
    }
    const uint32_t glyph_index = glyph_id ? glyph_id.value() : FT_Get_Char_Index(face, codepoint);
    if(glyph_index == 0) {
        fprintf(stderr, "Missing glyph for codepoint %d, glyph id %d", codepoint, glyph_index);
        RETERR(MissingGlyph);
    }
    ERCV(handle_subglyphs(glyph_index));
    subset.glyphs.push_back(RegularGlyph{codepoint, glyph_index});
    subset.font_index_mapping[glyph_index] = (uint32_t)subset.glyphs.size() - 1;
    return FontSubsetInfo{int32_t(subset.glyphs.size() - 1)};
}

rvoe<NoReturnValue> FontSubsetter::handle_subglyphs(uint32_t glyph_index) {
    if(ttfile.in_cff_format()) {
        RETOK;
    }
    if(glyph_index == 0) {
        fprintf(stderr, "Glyph id zero is not valid.\n");
        RETERR(MissingGlyph);
    }
    if(glyph_index >= ttfile.glyphs.size()) {
        fprintf(stderr,
                "Glyph index points past the end of glyps: %d/%d.\n",
                glyph_index,
                (int)ttfile.glyphs.size());
        RETERR(MissingGlyph);
    }
    ERC(iscomp, is_composite_glyph(ttfile.glyphs.at(glyph_index)));
    if(iscomp) {
        ERC(subglyphs, get_all_subglyphs(glyph_index, ttfile));
        if(subglyphs.size() + subset.glyphs.size() >= max_glyphs) {
            fprintf(stderr, "Composite glyph overflow not yet implemented.");
            std::abort();
        }
        for(const auto &new_glyph : subglyphs) {
            if(subset.font_index_mapping.find(new_glyph) != subset.font_index_mapping.end()) {
                continue;
            }
            // Composite glyph parts do not necessarily correspond to any Unicode codepoint.
            subset.glyphs.push_back(CompositeGlyph{new_glyph});
            subset.font_index_mapping[new_glyph] = (uint32_t)subset.glyphs.size() - 1;
        }
    }
    RETOK;
}

rvoe<FontSubsetInfo> FontSubsetter::unchecked_insert_glyph_to_last_subset(const u8string &text,
                                                                          uint32_t glyph_id) {
    if(subset.glyphs.size() >= max_glyphs) {
        RETERR(TooManyGlyphsUsed);
    }
    ERCV(handle_subglyphs(glyph_id));
    subset.glyphs.push_back(LigatureGlyph{text, glyph_id});
    subset.font_index_mapping[glyph_id] = (uint32_t)subset.glyphs.size() - 1;
    return FontSubsetInfo{int32_t(subset.glyphs.size() - 1)};
}

std::optional<FontSubsetInfo> FontSubsetter::find_existing_glyph(uint32_t gid) const {
    auto loc =
        std::find_if(subset.glyphs.cbegin(), subset.glyphs.cend(), [&gid](const TTGlyphs &ttg) {
            if(std::holds_alternative<RegularGlyph>(ttg)) {
                if(std::get<RegularGlyph>(ttg).glyph_index == gid) {
                    return true;
                }
            }
            return false;
        });
    if(loc != subset.glyphs.cend()) {
        return FontSubsetInfo{int32_t(loc - subset.glyphs.cbegin())};
    }
    return {};
}

std::optional<FontSubsetInfo> FontSubsetter::find_glyph_with_codepoint(uint32_t codepoint) const {
    auto loc = std::find_if(
        subset.glyphs.cbegin(), subset.glyphs.cend(), [&codepoint](const TTGlyphs &ttg) {
            if(std::holds_alternative<RegularGlyph>(ttg)) {
                return std::get<RegularGlyph>(ttg).unicode_codepoint == codepoint;
            }
            return false;
        });
    if(loc != subset.glyphs.cend()) {
        return FontSubsetInfo{int32_t(loc - subset.glyphs.cbegin())};
    }
    return {};
}

std::optional<FontSubsetInfo> FontSubsetter::find_glyph(const u8string &text) const {
    auto loc =
        std::find_if(subset.glyphs.cbegin(), subset.glyphs.cend(), [&text](const TTGlyphs &ttg) {
            if(std::holds_alternative<LigatureGlyph>(ttg)) {
                return std::get<LigatureGlyph>(ttg).text == text;
            }
            return false;
        });
    if(loc != subset.glyphs.cend()) {
        return FontSubsetInfo{int32_t(loc - subset.glyphs.cbegin())};
    }
    return {};
}

rvoe<std::vector<std::byte>> FontSubsetter::generate_subset(const TrueTypeFontFile &source) const {
    return generate_font(source, props, face, subset.glyphs, subset.font_index_mapping);
}

} // namespace capypdf::internal
