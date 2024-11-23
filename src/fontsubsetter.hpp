// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

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

namespace capypdf::internal {

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

    rvoe<FontSubsetInfo> get_glyph_subset(uint32_t glyph, const std::optional<uint32_t> glyph_id);
    rvoe<FontSubsetInfo> get_glyph_subset(const u8string &text, const uint32_t glyph_id);
    rvoe<FontSubsetInfo>
    unchecked_insert_glyph_to_last_subset(const uint32_t codepoint,
                                          const std::optional<uint32_t> glyph_id);
    rvoe<FontSubsetInfo> unchecked_insert_glyph_to_last_subset(const u8string &text,
                                                               uint32_t glyph_id);

    const std::vector<TTGlyphs> &get_subset(int32_t subset_number) const {
        return subsets.at(subset_number).glyphs;
    }

    std::vector<TTGlyphs> &get_subset(int32_t subset_number) {
        return subsets.at(subset_number).glyphs;
    }

    size_t num_subsets() const { return subsets.size(); }
    size_t subset_size(size_t subset) const { return subsets.at(subset).glyphs.size(); }

    rvoe<std::string> generate_subset(const TrueTypeFontFile &source, int32_t subset_number) const;

private:
    rvoe<NoReturnValue> handle_subglyphs(uint32_t glyph_index);

    TrueTypeFontFile ttfile;
    FT_Face face;
    std::optional<FontSubsetInfo> find_glyph(uint32_t glyph) const;
    std::optional<FontSubsetInfo> find_glyph(const u8string &text) const;

    std::vector<FontSubsetData> subsets;
};

} // namespace capypdf::internal
