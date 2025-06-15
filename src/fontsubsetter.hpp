// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#pragma once

#include <ft_subsetter.hpp>

#include <stdint.h>

typedef struct FT_FaceRec_ *FT_Face;

namespace capypdf::internal {

static const std::size_t max_glyphs = 65000;

struct FontSubsetInfo {
    int32_t subset;
    int32_t offset;
};

struct FontSubsetData {
    pystd2025::Vector<TTGlyphs> glyphs;
    pystd2025::HashMap<uint32_t, uint32_t> font_index_mapping;
};

class FontSubsetter {
public:
    FontSubsetter() noexcept { face = nullptr; }
    static rvoe<FontSubsetter>
    construct(const pystd2025::Path &fontfile, FT_Face face, const FontProperties &props);

    FontSubsetter(TrueTypeFontFile ttfile, FT_Face face, FontSubsetData subset)
        : ttfile{pystd2025::move(ttfile)}, face{face}, subset{pystd2025::move(subset)} {}

    rvoe<FontSubsetInfo> get_glyph_subset(uint32_t glyph,
                                          const pystd2025::Optional<uint32_t> glyph_id);
    rvoe<FontSubsetInfo> get_glyph_subset(const u8string &text, const uint32_t glyph_id);
    rvoe<FontSubsetInfo>
    unchecked_insert_glyph_to_last_subset(const uint32_t codepoint,
                                          const pystd2025::Optional<uint32_t> glyph_id);
    rvoe<FontSubsetInfo> unchecked_insert_glyph_to_last_subset(const u8string &text,
                                                               uint32_t glyph_id);

    const pystd2025::Vector<TTGlyphs> &get_subset() const { return subset.glyphs; }

    pystd2025::Vector<TTGlyphs> &get_subset() { return subset.glyphs; }

    size_t subset_size() const { return subset.glyphs.size(); }

    rvoe<pystd2025::Bytes> generate_subset(const TrueTypeFontFile &source) const;

private:
    rvoe<NoReturnValue> handle_subglyphs(uint32_t glyph_index);

    TrueTypeFontFile ttfile;
    FT_Face face;
    pystd2025::Optional<FontSubsetInfo> find_existing_glyph(uint32_t gid) const;
    pystd2025::Optional<FontSubsetInfo> find_glyph_with_codepoint(uint32_t codepoint) const;
    pystd2025::Optional<FontSubsetInfo> find_glyph(const u8string &text) const;

    FontSubsetData subset;
};

} // namespace capypdf::internal
