#!/usr/bin/env python3

# SPDX-License-Identifier: Apache-2.0
# Copyright 2023-2024 Jussi Pakkanen

import pathlib, os, sys, json
ename = 'CAPYPDF_SO_OVERRIDE'
if ename not in os.environ:
    os.environ[ename] = 'build/src'
source_root = pathlib.Path(__file__).parent / '..'
sys.path.append(str(source_root / 'python'))

try:
    import capypdf
except Exception:
    print('You might need to edit the search paths at the top of this file to get it to find the dependencies.')
    raise

def cm2pt(pts):
    return pts*28.346;


class PrinterTest:
    def __init__(self, ofilename):
        self.ofilename = ofilename
        self.fontfile = '/usr/share/fonts/truetype/noto/NotoSerif-Regular.ttf'
        self.titlesize = 20
        self.textsize = 10
        opts = capypdf.DocumentMetadata()
        props = capypdf.PageProperties()
        self.w = 595.0
        self.h = 841.89
        props.set_pagebox(capypdf.PageBox.Media, 0, 0, self.w, self.h)
        opts.set_title('Printer test document')
        opts.set_author('CapyPDF developers')
        opts.set_default_page_properties(props)
        self.pdfgen = capypdf.Generator(self.ofilename, opts)
        self.basefont = self.pdfgen.load_font(self.fontfile)

    def render_centered(self, text, font, pointsize, x, y):
        text_w = self.pdfgen.text_width(text, font, pointsize)
        self.ctx.render_text(text, font, pointsize, x -text_w/2, y)

    def create(self):
        with self.pdfgen.page_draw_context() as page1:
            self.ctx = page1
            self.render_centered("CapyPDF printer test page", self.basefont, 20, self.w/2, 800)
            with self.ctx.push_gstate():
                self.ctx.translate(10, 700)
                self.draw_grays()
            with self.ctx.push_gstate():
                self.ctx.translate(10, 600)
                self.draw_richblacks()
            with self.ctx.push_gstate():
                self.ctx.translate(50, 500)
                self.draw_op()
            with self.ctx.push_gstate():
                self.ctx.translate(50, 250)
                self.draw_lab()
        del self.ctx

    def draw_lab(self):
        import math
        c = capypdf.Color()
        labid = self.pdfgen.add_lab_colorspace(0.9505, 1.0, 1.08, -128, 127, -128, 127)
        boxw = 15
        boxh = 15
        gridw = 20
        gridh = 20
        num_steps = 20
        num_rows = 10
        gridstart = 30
        for rowid in range(num_rows+1):
            L = rowid/num_rows*100
            self.ctx.cmd_g(0.0)
            self.ctx.render_text(f"L={int(L)}", self.basefont, 10, 0, rowid*gridh + 3)
            for i in range(num_steps):
                a = 128*math.cos(i/num_steps*2*3.141)
                b = 128*math.sin(i/num_steps*2*3.141)
                c.set_lab(labid, L, a, b)
                self.ctx.set_nonstroke(c)
                self.ctx.cmd_re(gridstart + i*gridw, rowid*gridh, boxw, boxh)
                self.ctx.cmd_f()

    def draw_op(self):
        with self.ctx.push_gstate():
            self.draw_tridot('No overprint')
        with self.ctx.push_gstate():
            gs = capypdf.GraphicsState()
            gs.set_OP(True)
            gs.set_OPM(0)
            gsid = self.pdfgen.add_graphics_state(gs)
            self.ctx.translate(150, 0)
            self.ctx.cmd_gs(gsid)
            self.draw_tridot('Overprint mode 0')
        with self.ctx.push_gstate():
            gs = capypdf.GraphicsState()
            gs.set_OP(True)
            gs.set_OPM(1)
            gsid = self.pdfgen.add_graphics_state(gs)
            self.ctx.translate(300, 0)
            self.ctx.cmd_gs(gsid)
            self.draw_tridot('Overprint mode 1')

    def draw_tridot(self, text):
        self.render_centered(text, self.basefont, 10, 50, 0)
        self.ctx.cmd_k(1.0, 0, 0, 0)
        self.draw_circle(50, 50, 50+10)
        self.ctx.cmd_k(0.0, 1.0, 0, 0)
        self.draw_circle(50, 35, 25+10)
        self.ctx.cmd_k(0.0, 0.0, 1.0, 0)
        self.draw_circle(50, 65, 25+10)

    def draw_unit_circle(self):
        control = 0.5523 / 2;
        self.ctx.cmd_m(0, 0.5);
        self.ctx.cmd_c(control, 0.5, 0.5, control, 0.5, 0);
        self.ctx.cmd_c(0.5, -control, control, -0.5, 0, -0.5);
        self.ctx.cmd_c(-control, -0.5, -0.5, -control, -0.5, 0);
        self.ctx.cmd_c(-0.5, control, -control, 0.5, 0, 0.5);

    def draw_and_fill_unit_circle(self):
        self.draw_unit_circle()
        self.ctx.cmd_f()

    def draw_circle(self, scale, offx, offy):
        with self.ctx.push_gstate():
            self.ctx.translate(offx, offy)
            self.ctx.scale(scale, scale)
            self.draw_and_fill_unit_circle()

    def draw_grays(self):
        self.ctx.cmd_g(0.5)
        deltax = 100
        self.draw_gray_and_text(50, 80, 50, "G gray")
        self.ctx.cmd_k(0.5, 0.5, 0.5, 0)
        self.draw_gray_and_text(50, 80 + deltax, 50, "CMY gray")
        self.ctx.cmd_k(0.5, 0.5, 0.5, 0.5)
        self.draw_gray_and_text(50, 80 + 2*deltax, 50, "CMYK gray")
        self.ctx.cmd_k(0.0, 0.0, 0.0, 0.5)
        self.draw_gray_and_text(50, 80 + 3*deltax, 50, "K gray")
        self.ctx.cmd_rg(0.5, 0.5, 0.5)
        self.draw_gray_and_text(50, 80 + 4*deltax, 50, "RGB gray")

    def draw_richblacks(self):
        self.ctx.cmd_k(0.1, 0, 0, 1.0)
        deltax = 100
        self.draw_gray_and_text(50, 80, 50, "C rich black")
        self.ctx.cmd_k(0.0, 0.1, 0.0, 1.0)
        self.draw_gray_and_text(50, 80 + deltax, 50, "M rich black")
        self.ctx.cmd_k(0.0, 0.0, 0.1, 1.0)
        self.draw_gray_and_text(50, 80 + 2*deltax, 50, "Y rich black")
        self.ctx.cmd_k(0.1, 0.1, 0.1, 1.0)
        self.draw_gray_and_text(50, 80 + 3*deltax, 50, "All rich black")

    def draw_gray_and_text(self, s, transx, transy, text):
        self.draw_circle(s, transx, transy)
        self.ctx.cmd_g(0)
        self.render_centered(text, self.basefont, 10, transx, transy - 40)

    def finish(self):
        self.pdfgen.write()

if __name__ == '__main__':
    p = PrinterTest(    'capy_printer_test.pdf')
    p.create()
    p.finish()
