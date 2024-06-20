// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#pragma once

#include <capypdf.h>
#include <pdfcommon.hpp>
#include <variant>
#include <cstdint>
#include <errorhandling.hpp>
#include <vector>
#include <string>

namespace capypdf {

class PdfDrawContext;

struct KerningValue {
    int32_t v; // In PDF font space units, which is by definition is 1/1000 pt.
};

struct UnicodeCharacter {
    uint32_t codepoint;
};

struct GlyphItem {
    uint32_t glyph_id;
    uint32_t unicode_codepoint;
};

struct ActualTextStart {
    u8string text;
};

struct ActualTextEnd {};

typedef std::variant<KerningValue, UnicodeCharacter, GlyphItem, ActualTextStart, ActualTextEnd>
    TextAtom;
typedef std::vector<TextAtom> TextEvents;

class TextSequence {

public:
    rvoe<NoReturnValue> append_kerning(int32_t k) {
        e.emplace_back(KerningValue{k});
        return NoReturnValue{};
    }

    rvoe<NoReturnValue> append_unicode(uint32_t codepoint) {
        e.emplace_back(UnicodeCharacter{codepoint});
        return NoReturnValue{};
    }

    rvoe<NoReturnValue> append_raw_glyph(uint32_t glyph_id, uint32_t unicode_codepoint) {
        e.emplace_back(GlyphItem{glyph_id, unicode_codepoint});
        return NoReturnValue{};
    }

    rvoe<NoReturnValue> append_actualtext_start(const u8string &at) {
        if(is_actualtext()) {
            RETERR(DrawStateEndMismatch);
        }
        e.emplace_back(ActualTextStart{at});
        in_actualtext = true;
        return NoReturnValue{};
    }

    rvoe<NoReturnValue> append_actualtext_end() {
        if(!is_actualtext()) {
            RETERR(DrawStateEndMismatch);
        }
        e.emplace_back(ActualTextEnd{});
        in_actualtext = false;
        return NoReturnValue{};
    }

    TextEvents &&steal_guts() { return std::move(e); }

    bool is_actualtext() const { return in_actualtext; }

    void clear() {
        e.clear();
        in_actualtext = false;
    }

private:
    TextEvents e;
    bool in_actualtext = false;
};

struct TStar_arg {};

struct Tc_arg {
    double val;
};

struct Td_arg {
    double tx, ty;
};

struct TD_arg {
    double tx, ty;
};

struct Tf_arg {
    CapyPDF_FontId font;
    double pointsize;
};

struct Text_arg {
    u8string text;
};

struct TJ_arg {
    TextEvents elements;
};

struct TL_arg {
    double leading;
};

struct Tm_arg {
    double a, b, c, d, e, f;
};

struct Tr_arg {
    CapyPDF_Text_Mode rmode;
};

struct Ts_arg {
    double rise;
};

struct Tw_arg {
    double width;
};

struct Tz_arg {
    double scaling;
};

struct Emc_arg {};

struct Stroke_arg {
    Color c;
};

struct Nonstroke_arg {
    Color c;
};

struct StructureItem {
    CapyPDF_StructureItemId sid;
};

typedef std::variant<TStar_arg,
                     Tc_arg,
                     Td_arg,
                     TD_arg,
                     Tf_arg,
                     Text_arg,
                     TJ_arg,
                     TL_arg,
                     Tm_arg,
                     Tr_arg,
                     Ts_arg,
                     Tw_arg,
                     Tz_arg,
                     StructureItem,
                     Emc_arg,
                     Stroke_arg,
                     Nonstroke_arg>
    TextEvent;

class PdfText {
public:
    explicit PdfText(PdfDrawContext *dc) : dc{dc} {};

    rvoe<NoReturnValue> cmd_BDC(CapyPDF_StructureItemId sid) {
        events.emplace_back(StructureItem{sid});
        RETOK;
    }

    rvoe<NoReturnValue> cmd_EMC() {
        events.emplace_back(Emc_arg{});
        RETOK;
    }

    rvoe<NoReturnValue> cmd_Tstar() {
        events.emplace_back(TStar_arg{});
        RETOK;
    }

    rvoe<NoReturnValue> cmd_Tc(double char_spacing) {
        events.emplace_back(Tc_arg{char_spacing});
        RETOK;
    }

    rvoe<NoReturnValue> cmd_Td(double tx, double ty) {
        events.emplace_back(Td_arg{tx, ty});
        RETOK;
    }

    rvoe<NoReturnValue> cmd_TD(double tx, double ty) {
        events.emplace_back(TD_arg{tx, ty});
        RETOK;
    }

    rvoe<NoReturnValue> cmd_Tf(CapyPDF_FontId font, double pointsize) {
        events.emplace_back(Tf_arg{font, pointsize});
        RETOK;
    }

    rvoe<NoReturnValue> render_text(const u8string &text) {
        events.emplace_back(Text_arg{text});
        RETOK;
    }

    // cmd_Tj missing. It might not be needed at all.

    rvoe<NoReturnValue> cmd_TJ(TextSequence &seq) {
        if(seq.is_actualtext()) {
            RETERR(DrawStateEndMismatch);
        }
        events.emplace_back(TJ_arg{seq.steal_guts()});
        seq.clear();
        RETOK;
    }

    rvoe<NoReturnValue> cmd_TL(double leading) {
        events.emplace_back(TL_arg{leading});
        RETOK;
    }

    rvoe<NoReturnValue> cmd_Tm(double a, double b, double c, double d, double e, double f) {
        events.emplace_back(Tm_arg{a, b, c, d, e, f});
        RETOK;
    }

    rvoe<NoReturnValue> cmd_Tr(CapyPDF_Text_Mode rmode) {
        events.emplace_back(Tr_arg{rmode});
        RETOK;
    }

    rvoe<NoReturnValue> cmd_Ts(double rise) {
        events.emplace_back(Ts_arg{rise});
        RETOK;
    }

    rvoe<NoReturnValue> cmd_Tw(double spacing) {
        events.emplace_back(Tw_arg{spacing});
        RETOK;
    }

    rvoe<NoReturnValue> cmd_Tz(double scaling) {
        events.emplace_back(Tz_arg{scaling});
        RETOK;
    }

    rvoe<NoReturnValue> stroke_color(const Color &c) {
        events.emplace_back(Stroke_arg{c});
        RETOK;
    }

    rvoe<NoReturnValue> nonstroke_color(const Color &c) {
        events.emplace_back(Nonstroke_arg{c});
        RETOK;
    }

    PdfDrawContext *creator() const { return dc; }
    const std::vector<TextEvent> &get_events() const { return events; }

private:
    PdfDrawContext *dc;
    std::vector<TextEvent> events;
};

} // namespace capypdf
