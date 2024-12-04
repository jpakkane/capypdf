// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <generator.hpp>
#include <pdftext.hpp>
#include <cmath>

using namespace capypdf::internal;

void center_test() {
    u8string text = u8string::from_cstr("Centered text!").value();
    const double pt = 12;
    DocumentProperties opts;
    opts.output_colorspace = CAPY_DEVICE_CS_GRAY;
    opts.default_page_properties.mediabox->x2 = 200;
    opts.default_page_properties.mediabox->y2 = 200;
    GenPopper genpop("centering.pdf", opts);
    PdfGen &gen = *genpop.g;
    auto f1 = gen.load_font("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf").value();
    auto f2 = gen.load_font("/usr/share/fonts/truetype/noto/NotoMono-Regular.ttf").value();
    auto f3 =
        gen.load_font("/usr/share/fonts/truetype/gentiumplus/GentiumBookPlus-Regular.ttf").value();
    auto ctxpop = gen.guarded_page_context();
    auto &ctx = ctxpop.ctx;
    ctx.cmd_w(1.0);
    ctx.cmd_m(100, 0);
    ctx.cmd_l(100, 200);
    ctx.cmd_S();

    auto w = gen.utf8_text_width(text, f1, pt).value();
    ctx.render_text(text, f1, pt, 100 - w / 2, 120);

    w = gen.utf8_text_width(text, f2, pt).value();
    ctx.render_text(text, f2, pt, 100 - w / 2, 100);

    w = gen.utf8_text_width(text, f3, pt).value();
    ctx.render_text(text, f3, pt, 100 - w / 2, 80);
}

int test1(int argc, char **argv) {
    DocumentProperties opts;
    opts.output_colorspace = CAPY_DEVICE_CS_RGB;
    const char *regularfont;
    const char *italicfont;
    if(argc > 1) {
        regularfont = argv[1];
    } else {
        regularfont = "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf";
    }
    if(argc > 2) {
        italicfont = argv[2];
    } else {
        italicfont = "/usr/share/fonts/truetype/noto/NotoSans-Italic.ttf";
    }
    if(false) {
        center_test();
    }
    /*
    opts.mediabox.x = opts.mediabox.y = 0;
    opts.mediabox.w = 200;
    opts.mediabox.h = 200;
    */
    opts.title = u8string::from_cstr("Over 255 letters").value();
    GenPopper genpop("fonttest.pdf", opts);
    PdfGen &gen = *genpop.g;
    auto regular_fid = gen.load_font(regularfont).value();
    auto italic_fid = gen.load_font(italicfont).value();
    auto ctxguard = gen.guarded_page_context();
    auto &ctx = ctxguard.ctx;
    ctx.set_nonstroke_color(DeviceGrayColor{0.0});
    ctx.render_text(
        u8string::from_cstr("ABCDEFGHIJKLMNOPQRSTUVWXYZÅÄÖ").value(), regular_fid, 12, 20, 800);
    ctx.render_text(
        u8string::from_cstr("abcdefghijklmnopqrstuvwxyzåäö").value(), regular_fid, 12, 20, 780);
    ctx.render_text(
        u8string::from_cstr("0123456789!\"#¤%&/()=+?-.,;:'*~").value(), regular_fid, 12, 20, 760);
    ctx.render_text(u8string::from_cstr("бгджзиклмнптфцч").value(), regular_fid, 12, 20, 740);
    ctx.render_text(u8string::from_cstr("ΓΔΖΗΛΞΠΣΥΦΧΨΩ").value(), regular_fid, 12, 20, 720);
    {
        auto statepop = ctx.push_gstate();
        PdfText text(&ctx);
        text.cmd_Tf(regular_fid, 24);
        text.cmd_Td(20, 650);
        text.nonstroke_color(DeviceRGBColor{1.0, 0.0, 0.0});
        text.render_text(u8string::from_cstr("C").value());
        text.nonstroke_color(DeviceRGBColor{0.0, 1.0, 0.0});
        text.render_text(u8string::from_cstr("o").value());
        text.nonstroke_color(DeviceRGBColor{0.0, 0.0, 1.0});
        text.render_text(u8string::from_cstr("l").value());
        text.nonstroke_color(DeviceRGBColor{1.0, 1.0, 0.0});
        text.render_text(u8string::from_cstr("o").value());
        text.nonstroke_color(DeviceRGBColor{1.0, 0.0, 1.0});
        text.render_text(u8string::from_cstr("r").value());
        text.nonstroke_color(DeviceRGBColor{0.0, 1.0, 0.0});
        text.render_text(u8string::from_cstr("!").value());
        ctx.render_text(text);
    }
    {
        PdfText text(&ctx);
        text.cmd_Tf(regular_fid, 12);
        text.cmd_Td(20, 700);
        TextSequence kerned_text;

        kerned_text.append_unicode(uint32_t('A'));
        kerned_text.append_kerning(-100.0);
        kerned_text.append_unicode(uint32_t('V'));

        kerned_text.append_unicode(uint32_t(' '));

        kerned_text.append_unicode(uint32_t('A'));
        kerned_text.append_unicode(uint32_t('V'));

        kerned_text.append_unicode(uint32_t(' '));

        kerned_text.append_unicode(uint32_t('A'));
        kerned_text.append_kerning(100.0);
        kerned_text.append_unicode(uint32_t('V'));

        text.cmd_TL(14);
        text.cmd_TJ(kerned_text);
        text.cmd_Tstar();
        text.render_text(
            u8string::from_cstr(
                "This is some text using a text object. It uses Freetype kerning (i.e. not GPOS).")
                .value());
        ctx.render_text(text);
    }
    {
        PdfText text(&ctx);
        text.cmd_Tf(regular_fid, 12);
        text.cmd_Td(20, 600);
        text.render_text(u8string::from_cstr("How about some ").value());
        text.cmd_Tf(italic_fid, 12);
        text.render_text(u8string::from_cstr("italic").value());
        text.cmd_Tf(regular_fid, 12);
        text.render_text(u8string::from_cstr(" text?").value());
        ctx.render_text(text);
    }

    {
        PdfText text(&ctx);
        text.cmd_Tf(regular_fid, 12);
        text.cmd_Td(20, 550);
        text.render_text(u8string::from_cstr("How about some ").value());
        text.cmd_Ts(4);
        text.render_text(u8string::from_cstr("raised").value());
        text.cmd_Ts(0);
        text.render_text(u8string::from_cstr(" text?").value());
        ctx.render_text(text);
    }

    {
        PdfText text(&ctx);
        text.cmd_Tf(regular_fid, 12);
        text.cmd_Td(20, 500);
        text.render_text(u8string::from_cstr("Character spacing").value());
        text.cmd_Tstar();
        text.cmd_Tc(1);
        text.render_text(u8string::from_cstr("Character spacing").value());
        ctx.render_text(text);
    }

    {
        PdfText text(&ctx);
        text.cmd_Tf(regular_fid, 12);
        text.cmd_Td(20, 450);
        text.render_text(u8string::from_cstr("Word spacing word spacing word spacing.").value());
        text.cmd_Tstar();
        text.cmd_Tw(4);
        text.render_text(u8string::from_cstr("Word spacing word spacing word spacing.").value());
        ctx.render_text(text);
    }

    {
        PdfText text(&ctx);
        text.cmd_Tf(regular_fid, 12);
        text.cmd_Td(20, 400);
        text.render_text(u8string::from_cstr("Character scaling.").value());
        text.cmd_Tstar();
        text.cmd_Tz(150);
        text.render_text(u8string::from_cstr("Character scaling.").value());
        text.cmd_Tz(100);
        ctx.render_text(text);
    }

    {
        PdfText text(&ctx);
        text.cmd_Tf(regular_fid, 12);
        text.cmd_Td(20, 300);
        for(int i = 1; i < 20; ++i) {
            text.cmd_Tf(regular_fid, 2 * i);
            text.render_text(u8string::from_cstr("X").value());
        }
        ctx.render_text(text);
    }

    /*
    std::vector<PdfGlyph> glyphs;
    const int num_glyphs = 26;
    for(int i = 0; i < num_glyphs; ++i) {
        const double x = 100 + 40 * sin(2 * M_PI * double(i) / num_glyphs);
        const double y = 50 + 40 * cos(2 * M_PI * double(i) / num_glyphs);
        glyphs.emplace_back(PdfGlyph{uint32_t('a' + i), x, y});
    }
    ctx.render_glyphs(glyphs, fid, 10);
    */
    /*
    gen.new_page();
    for(int page_num = 0; page_num < 2; ++page_num) {
        for(int i = 0; i < 16; ++i) {
            for(int j = 0; j < 16; ++j) {
                char buf[10];
                const uint32_t cur_char = 256 * page_num + 16 * i + j;
                snprintf(buf, 10, "0x%04X", cur_char);
                ctx.render_utf8_text(buf, fid, 8, 10 + 45 * i, opts.page_size.h - (10 + 10 *
    j)); ctx.render_raw_glyph( (uint32_t)cur_char, fid, 8, 10 + 30 + 45 * i, opts.page_size.h -
    (10 + 10 * j));
            }
        }
    }
    */
    return 0;
}

int test2(int argc, char **argv) {
    DocumentProperties opts;
    opts.output_colorspace = CAPY_DEVICE_CS_RGB;
    const char *regularfont;
    if(argc > 1) {
        regularfont = argv[1];
    } else {
        regularfont = "/usr/share/fonts/truetype/noto/NotoSerif-Regular.ttf";
    }
    opts.title = u8string::from_cstr("Ligaturing").value();
    GenPopper genpop("ligaturetest.pdf", opts);
    PdfGen &gen = *genpop.g;
    auto regular_fid = gen.load_font(regularfont).value();
    auto ctxguard = gen.guarded_page_context();
    auto &ctx = ctxguard.ctx;
    {
        PdfText text(&ctx);
        TextSequence ts;
        ts.append_unicode('A');
        auto ffi = u8string::from_cstr("ffi").value();

        ts.append_ligature_glyph(2132, ffi);
        ts.append_unicode('x');
        text.cmd_Tf(regular_fid, 12);
        text.cmd_Td(20, 300);
        text.cmd_TJ(ts);
        ctx.render_text(text);
    }
    return 0;
}

int main(int argc, char **argv) {
    if(false) {
        return test1(argc, argv);
    } else {
        return test2(argc, argv);
    }
}
