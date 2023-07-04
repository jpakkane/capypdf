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

#include <capypdf.h>
#include <variant>
#include <cstdint>
#include <errorhandling.hpp>
#include <vector>
#include <string>

namespace capypdf {

typedef std::variant<uint32_t, double> CharItem;

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
    std::string utf8_text;
};

struct TJ_arg {
    std::vector<CharItem> elements;
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
                     CapyPDF_StructureItemId,
                     Emc_arg,
                     Stroke_arg,
                     Nonstroke_arg>
    TextEvent;

class PdfText {
public:
    PdfText(){};

    ErrorCode cmd_BDC(CapyPDF_StructureItemId sid) {
        events.emplace_back(sid);
        return ErrorCode::NoError;
    }

    ErrorCode cmd_EMC() {
        events.emplace_back(Emc_arg{});
        return ErrorCode::NoError;
    }

    ErrorCode cmd_Tstar() {
        events.emplace_back(TStar_arg{});
        return ErrorCode::NoError;
    }

    ErrorCode cmd_Tc(double char_spacing) {
        events.emplace_back(Tc_arg{char_spacing});
        return ErrorCode::NoError;
    }

    ErrorCode cmd_Td(double tx, double ty) {
        events.emplace_back(Td_arg{tx, ty});
        return ErrorCode::NoError;
    }

    ErrorCode cmd_TD(double tx, double ty) {
        events.emplace_back(TD_arg{tx, ty});
        return ErrorCode::NoError;
    }

    ErrorCode cmd_Tf(CapyPDF_FontId font, double pointsize) {
        events.emplace_back(Tf_arg{font, pointsize});
        return ErrorCode::NoError;
    }

    ErrorCode render_text(std::string_view utf8_text) {
        events.emplace_back(Text_arg{std::string{utf8_text}});
        return ErrorCode::NoError;
    }

    // cmd_Tj missing. It might not be needed at all.

    ErrorCode cmd_TJ(std::vector<CharItem> chars) {
        events.emplace_back(TJ_arg{std::move(chars)});
        return ErrorCode::NoError;
    }

    ErrorCode cmd_TL(double leading) {
        events.emplace_back(TL_arg{leading});
        return ErrorCode::NoError;
    }

    ErrorCode cmd_TM(double a, double b, double c, double d, double e, double f) {
        events.emplace_back(Tm_arg{a, b, c, d, e, f});
        return ErrorCode::NoError;
    }

    ErrorCode cmd_Tr(CapyPDF_Text_Mode rmode) {
        events.emplace_back(Tr_arg{rmode});
        return ErrorCode::NoError;
    }

    ErrorCode cmd_Ts(double rise) {
        events.emplace_back(Ts_arg{rise});
        return ErrorCode::NoError;
    }

    ErrorCode cmd_Tw(double spacing) {
        events.emplace_back(Tw_arg{spacing});
        return ErrorCode::NoError;
    }

    ErrorCode cmd_Tz(double scaling) {
        events.emplace_back(Tz_arg{scaling});
        return ErrorCode::NoError;
    }

    ErrorCode stroke_color(const Color &c) {
        events.emplace_back(Stroke_arg{c});
        return ErrorCode::NoError;
    }

    ErrorCode nonstroke_color(const Color &c) {
        events.emplace_back(Nonstroke_arg{c});
        return ErrorCode::NoError;
    }

    const std::vector<TextEvent> &get_events() const { return events; }

private:
    std::vector<TextEvent> events;
};

} // namespace capypdf
