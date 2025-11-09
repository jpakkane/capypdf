// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <type3font.hpp>

namespace capypdf::internal {

Type3Font::Type3Font() {

}

void Type3Font::set_font_matrix(double a, double b, double c, double d, double e, double f) {
    font_matrix[0] = a;
    font_matrix[1] = b;
    font_matrix[2] = c;
    font_matrix[3] = d;
    font_matrix[4] = e;
    font_matrix[5] = f;
}

rvoe<NoReturnValue> Type3Font::add_glyph(uint32_t codepoint, PdfDrawContext &ctx) {
    if(ctx.draw_context_type() != CAPY_DC_TYPE3_FONT) {
        RETERR(WrongDrawContext);
    }
    for(const auto &g: glyphs) {
        if(g.codepoint == codepoint) {
            RETERR(CodepointAlreadyExists);
        }
    }
    ERC(command_stream, ctx.steal_command_stream());

    // FIXME: update resource usage here.
    glyphs.emplace_back(codepoint, std::move(command_stream));
    RETOK;
}


}
