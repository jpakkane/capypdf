// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#pragma once

import std;

#include <pdfcommon.hpp>
#include <errorhandling.hpp>
#include <mmapper.hpp>
#include <cffsubsetter.hpp>

typedef struct FT_FaceRec_ *FT_Face;

namespace capypdf::internal {

#pragma pack(push, r1, 1)

struct TTHead {
    int32_t version;
    int32_t revision;
    uint32_t checksum_adjustment;
    uint32_t magic;
    uint16_t flags;
    uint16_t units_per_em;
    uint64_t created;
    uint64_t modified;
    int16_t x_min;
    int16_t y_min;
    int16_t x_max;
    int16_t y_max;
    uint16_t mac_style;
    uint16_t lowest_rec_pppem;
    int16_t font_direction_hint;
    int16_t index_to_loc_format;
    int16_t glyph_data_format;

    void swap_endian();
};
static_assert(sizeof(TTHead) == 54);

struct TTLongHorMetric {
    uint16_t advance_width;
    int16_t lsb;

    void swap_endian();
};

struct TTHhea {
    uint32_t version;
    int16_t ascender;
    int16_t descender;
    int16_t linegap;
    uint16_t advance_width_max;
    int16_t min_left_side_bearing;
    int16_t min_right_side_bearing;
    int16_t x_max_extent;
    int16_t caret_slope_rise;
    int16_t caret_slope_run;
    int16_t caret_offset;
    int16_t reserved0 = 0;
    int16_t reserved1 = 0;
    int16_t reserved2 = 0;
    int16_t reserved3 = 0;
    int16_t metric_data_format;
    uint16_t num_hmetrics;

    void swap_endian();
};

struct TTMaxp05 {
    uint32_t version;
    uint16_t num_glyphs;

    void swap_endian();
};

struct TTMaxp10 {
    uint32_t version;
    uint16_t num_glyphs;
    uint16_t max_points;
    uint16_t max_contours;
    uint16_t max_composite_points;
    uint16_t max_composite_contours;
    uint16_t max_zones;
    uint16_t max_twilight_points;
    uint16_t max_storage;
    uint16_t max_function_defs;
    uint16_t max_instruction_defs;
    uint16_t max_stack_elements;
    uint16_t max_sizeof_instructions;
    uint16_t max_component_elements;
    uint16_t max_component_depth;

    void swap_endian();
};

#pragma pack(pop, r1)

class TTMaxp {
public:
    TTMaxp() : data{TTMaxp10{}} {}
    explicit TTMaxp(const TTMaxp05 &t) : data{t} {}
    explicit TTMaxp(const TTMaxp10 &t) : data{t} {}

    uint16_t num_glyphs() const;
    void set_num_glyphs(uint16_t glyph_count);
    void swap_endian();

private:
    std::variant<TTMaxp05, TTMaxp10> data;
};

typedef std::variant<uint8_t, int16_t> CoordInfo;

struct TTHmtx {
    std::vector<TTLongHorMetric> longhor;
    std::vector<int16_t> left_side_bearings;
};

struct SimpleGlyph {
    std::vector<uint16_t> contour_end_points;
    uint16_t instruction_length;
    std::vector<uint8_t> instructions;
    std::vector<uint8_t> flags;
    std::vector<CoordInfo> xcoord;
    std::vector<CoordInfo> ycoord;
};

struct RegularGlyph {
    uint32_t unicode_codepoint;
    uint32_t glyph_index;
};

// A glyph that represents a unit of more than one Unicode point.
// As an example the source text "ffi" might map to a custom
// ligature glyph 0xFB03 in some fonts.
// We need to store this info in the cmap table so that
// text extraction and copypaste work.
struct LigatureGlyph {
    u8string text;
    uint32_t glyph_index;
};

struct CompositeGlyph {
    uint32_t glyph_index;
};

typedef std::variant<RegularGlyph, CompositeGlyph, LigatureGlyph> TTGlyphs;

/* Mandatory TTF tables according to The Internet.
 *
 * 'cmap' character to glyph mapping <- LO does not create this table.
 * 'glyf' glyph data
 * 'head' font header
 * 'hhea' horizontal header
 * 'hmtx' horizontal metrics
 * 'loca' index to location
 * 'maxp' maximum profile
 * 'name' naming                     <-Cairo and LO do not create this table
 * 'post' postscript                 <-Cairo and LO do not create this table
 */

/* In addition, the following may be in files created by Cairo and LO:
 *
 * cvt
 * fpgm
 * prep
 */

struct TrueTypeFontFile {
    DataSource original_data;

    // A font has hundreds or thousands of glyphs, of which a
    // typical PDF file uses only a subset. Reading all of them
    // into their own vectors would have a lot of memory overhead.
    // Thus we point to the original data instead.
    std::vector<std::span<std::byte>> glyphs;
    // A TrueType file can be just a container for a
    // CFF file. Note that if it has cff glyphs then it should
    // not have "glyf" glyphs from above.
    std::optional<CFFont> cff;
    TTHead head;
    TTHhea hhea;
    TTHmtx hmtx;
    // std::vector<int32_t> loca;
    TTMaxp maxp;
    std::vector<std::byte> cvt;
    std::vector<std::byte> fpgm;
    std::vector<std::byte> prep;
    std::vector<std::byte> cmap;

    int32_t num_glyphs() const {
        if(in_cff_format()) {
            return cff->char_strings.size();
        } else {
            return glyphs.size();
        }
    }

    bool in_cff_format() const { return cff.has_value(); }

    int num_directory_entries() const {
        int entries = 6;
        if(!cmap.empty()) {
            ++entries;
        }
        if(!cvt.empty()) {
            ++entries;
        }
        if(!fpgm.empty()) {
            ++entries;
        }
        if(!prep.empty()) {
            ++entries;
        }
        return entries;
    }
};

struct TrueTypeCollection {
    DataSource original_data;
    std::vector<TrueTypeFontFile> entries;
};

// In case of TTC, only return the requested subfont.
typedef std::variant<TrueTypeFontFile, CFFont> FontData;

rvoe<bool> is_composite_glyph(std::span<const std::byte> buf);
rvoe<std::vector<uint32_t>> composite_subglyphs(std::span<const std::byte> buf);

rvoe<NoReturnValue>
reassign_composite_glyph_numbers(std::span<std::byte> buf,
                                 const std::unordered_map<uint32_t, uint32_t> &mapping);

rvoe<std::vector<std::byte>>
generate_font(const TrueTypeFontFile &source,
              const FontProperties &props,
              FT_Face face,
              const std::vector<TTGlyphs> &glyphs,
              const std::unordered_map<uint32_t, uint32_t> &comp_mapping);

rvoe<FontData> parse_font_file(DataSource original_data, uint16_t subfont);
rvoe<FontData> load_and_parse_font_file(const char *fname, const FontProperties &props);
rvoe<TrueTypeFontFile> parse_truetype_file(DataSource backing, uint64_t header_offset = 0);

uint32_t font_id_for_glyph(const TTGlyphs &g);

} // namespace capypdf::internal
