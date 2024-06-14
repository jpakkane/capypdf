// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <pdfgen.hpp>
#include <pdftext.hpp>
#include <pdfcommon.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>

#include <cstdio>
#include <cassert>
#include <cstring>

#define CHCK(command)                                                                              \
    {                                                                                              \
        auto rc = command;                                                                         \
        if(!rc) {                                                                                  \
            fprintf(stderr, "%s\n", error_text(rc.error()));                                       \
            std::abort();                                                                          \
        }                                                                                          \
    }

using namespace capypdf;

// const char sampletext[] = "This is sample text. AV To.";
const char sampletext[] = "VA To MM.";
// const char fontfile[] = "/usr/share/fonts/truetype/noto/NotoSerif-Regular.ttf";
const char fontfile[] = "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf";
const double ptsize = 12;

void do_harfbuzz(PdfGen &gen, PdfDrawContext &ctx, CapyPDF_FontId pdffont) {
    FT_Library ft;
    FT_Face ftface;
    FT_Error fte;
    fte = FT_Init_FreeType(&ft);
    assert(fte == 0);
    fte = FT_New_Face(ft, fontfile, 0, &ftface);
    assert(fte == 0);

    hb_buffer_t *buf;
    const double num_steps = 64;
    const double hbscale = ptsize * num_steps;
    buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, sampletext, -1, 0, -1);
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    hb_buffer_guess_segment_properties(buf);

    hb_blob_t *blob = hb_blob_create_from_file(fontfile);
    hb_face_t *face = hb_face_create(blob, 0);
    hb_font_t *font = hb_font_create(face);
    hb_font_set_scale(font, hbscale, hbscale);

    hb_shape(font, buf, nullptr, 0);
    unsigned int glyph_count;
    hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);
    double cursor_x = 10;
    double cursor_y = 100;
    assert(strlen(sampletext) == glyph_count);

    KernSequence full_line;
    for(unsigned int i = 0; i < glyph_count; i++) {
        hb_codepoint_t glyphid = glyph_info[i].codepoint;
        hb_position_t x_offset = glyph_pos[i].x_offset;
        // hb_position_t y_offset = glyph_pos[i].y_offset;
        hb_position_t x_advance = glyph_pos[i].x_advance;
        hb_position_t y_advance = glyph_pos[i].y_advance;
        KernSequence ks;
        // FIXME: max inefficient. Creates a text object per glyph.
        PdfText txt(&ctx);
        CHCK(txt.cmd_Tf(pdffont, ptsize));
        txt.cmd_Td(cursor_x, cursor_y);
        ks.push_back(uint32_t{(unsigned char)sampletext[i]});
        txt.cmd_TJ(ks);
        auto ft_glyphid = FT_Get_Char_Index(ftface, sampletext[i]);
        assert(ft_glyphid == glyphid);
        fte = FT_Load_Glyph(ftface, glyphid, 0);
        assert(fte == 0);
        const auto hb_advance_in_font_units = x_advance / hbscale * ftface->units_per_EM;
        printf("%c(%u) %u %d %.2f\n",
               sampletext[i],
               uint32_t{(unsigned char)sampletext[i]},
               glyphid,
               x_offset,
               hb_advance_in_font_units);
        printf("  %f\n",
               ftface->glyph->advance.x - hb_advance_in_font_units); // / ftface->units_per_EM);
        cursor_x += x_advance / num_steps;
        cursor_y += y_advance / num_steps;
        CHCK(ctx.render_text(txt));
        full_line.push_back(uint32_t{(unsigned char)sampletext[i]});
        full_line.push_back((ftface->glyph->advance.x - hb_advance_in_font_units) /
                            ftface->units_per_EM * 1000);
    }
    {
        auto p = ctx.push_gstate();
        ctx.translate(10, 90);
        PdfText txt(&ctx);
        CHCK(txt.cmd_Tf(pdffont, ptsize));
        CHCK(txt.cmd_TJ(full_line));
        CHCK(ctx.render_text(txt));
    }
    hb_buffer_destroy(buf);
    hb_font_destroy(font);
    hb_face_destroy(face);
    hb_blob_destroy(blob);
}

int main(int, char **) {
    PdfGenerationData opts;
    opts.default_page_properties.mediabox = PdfRectangle(0, 0, 200, 200);
    opts.lang = asciistring::from_cstr("en-US").value();
    GenPopper genpop("harfbuzz.pdf", opts);
    PdfGen &gen = *genpop.g;

    auto ctxguard = gen.guarded_page_context();
    auto &ctx = ctxguard.ctx;

    auto pdffont = gen.load_font(fontfile).value();
    ctx.render_text(capypdf::u8string::from_cstr(sampletext).value(), pdffont, ptsize, 10, 110);
    do_harfbuzz(gen, ctx, pdffont);
    return 0;
}
