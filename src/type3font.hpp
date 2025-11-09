// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <drawcontext.hpp>
#include <cstdint>
#include <errorhandling.hpp>
#include <vector>

namespace capypdf::internal {

struct Type3Glyph {
    uint32_t codepoint;
    std::string stream;
};

class Type3Font {
public:

    Type3Font();

    void set_font_matrix(double a, double b, double c, double d, double e, double f);

    rvoe<NoReturnValue> add_glyph(uint32_t codepoint, PdfDrawContext &ctx);

private:

    double font_matrix[6] = {0.001, 0, 0, 0.001, 0, 0};
    std::vector<Type3Glyph> glyphs;

};

}
