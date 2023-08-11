#!/usr/bin/env python3

# Copyright 2023 Jussi Pakkanen
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os, sys, pathlib

os.environ['CAPYPDF_SO_OVERRIDE'] = 'src'
source_root = pathlib.Path('/home/jpakkane/projects/pdfgen')
sys.path.append(str(source_root / 'python'))

import capypdf

def cm2pt(pts):
    return pts*28.346

def lerp(ratio, c1, c2):
    v = []
    for i in range(len(c1)):
        v.append(ratio*c1[i] + (1.0-ratio)*c2[i])
    return v

class X3Creator:
    def __init__(self, ofile):
        self.ofile = ofile
        self.artw = cm2pt(21)
        self.arth = cm2pt(29.7)
        self.bleed = cm2pt(1)
        self.border = cm2pt(1)
        self.mediaw = self.artw + 2*(self.bleed + self.border)
        self.mediah = self.arth + 2*(self.bleed + self.border)
        self.trim_corner = self.border + self.bleed

        self.options = capypdf.Options()
        props = capypdf.PageProperties()
        self.fontfile = '/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf'
        self.imagefile = 'sampleimage.tif'
        self.profile = 'UncoatedFOGRA29.icc'
        self.options.set_colorspace(capypdf.Colorspace.DeviceCMYK)
        props.set_pagebox(capypdf.PageBox.Media,
                          0,
                          0,
                          self.mediaw,
                          self.mediah)
        props.set_pagebox(capypdf.PageBox.Trim,
                          self.border + self.bleed,
                          self.border + self.bleed,
                          self.border + self.bleed + self.artw,
                          self.border + self.bleed + self.arth)
        props.set_pagebox(capypdf.PageBox.Bleed,
                          self.border,
                          self.border,
                          self.border + 2 * self.bleed + self.artw,
                          self.border + 2 * self.bleed + self.arth)
        self.options.set_default_page_properties(props)
        self.options.set_author('Experimental Man')
        self.options.set_title('PDF/X3 sample document')
        self.options.set_device_profile(capypdf.Colorspace.DeviceCMYK,
                                        self.profile)
        self.options.set_output_intent(capypdf.IntentSubtype.PDFX, 'Uncoated Fogra 29')
        self.gen = capypdf.Generator(self.ofile, self.options)

    def load_resources(self):
        self.font = self.gen.load_font(self.fontfile)
        self.image = self.gen.load_image(self.imagefile)

    def draw_trim_marks(self, ctx):
        # Bottom
        ctx.cmd_m(self.trim_corner, 0)
        ctx.cmd_l(self.trim_corner, self.border)
        ctx.cmd_m(0, self.trim_corner)
        ctx.cmd_l(self.border, self.trim_corner)
        ctx.cmd_m(self.mediaw - self.trim_corner, 0)
        ctx.cmd_l(self.mediaw - self.trim_corner, self.border)
        ctx.cmd_m(self.mediaw, self.trim_corner)
        ctx.cmd_l(self.mediaw - self.border, self.trim_corner)

        # Top
        ctx.cmd_m(self.trim_corner, self.mediah)
        ctx.cmd_l(self.trim_corner, self.mediah - self.border)
        ctx.cmd_m(0, self.mediah - self.trim_corner)
        ctx.cmd_l(self.border, self.mediah - self.trim_corner)
        ctx.cmd_m(self.mediaw - self.trim_corner, self.mediah)
        ctx.cmd_l(self.mediaw - self.trim_corner, self.mediah - self.border)
        ctx.cmd_m(self.mediaw, self.mediah - self.trim_corner)
        ctx.cmd_l(self.mediaw - self.border, self.mediah - self.trim_corner)
        ctx.cmd_S()

    def draw_single_registration_mark(self, ctx):
        ctx.cmd_m(-0.7, 0)
        ctx.cmd_l(0.7, 0)
        ctx.cmd_m(0, -0.7)
        ctx.cmd_l(0, 0.7)
        control = 0.5523 / 2;
        ctx.cmd_m(0, 0.5);
        ctx.cmd_c(control, 0.5, 0.5, control, 0.5, 0);
        ctx.cmd_c(0.5, -control, control, -0.5, 0, -0.5);
        ctx.cmd_c(-control, -0.5, -0.5, -control, -0.5, 0);
        ctx.cmd_c(-0.5, control, -control, 0.5, 0, 0.5);
        ctx.cmd_S()

    def draw_registration_marks(self, ctx):
        coords = ((self.mediaw/2, self.trim_corner/3),
                  (self.mediaw/2, self.mediah - self.trim_corner/3),
                  (self.trim_corner/3, self.mediah/2),
                  (self.mediaw - self.trim_corner/3, self.mediah/2),
                  )
        for c in coords:
            with ctx.push_gstate():
                ctx.translate(*c)
                scale = 15
                ctx.scale(scale, scale)
                ctx.cmd_w(1/scale)
                self.draw_single_registration_mark(ctx)

    def draw_color_boxes(self, ctx):
        c = capypdf.Color()
        xoff = 2*self.trim_corner
        boxsize = 20
        yoff = 10
        boxcolors = ((1.0, 0.0, 0.0, 0.0),
                     (0.0, 1.0, 0.0, 0.0),
                     (0.0, 0.0, 1.0, 0.0),
                     (1.0, 1.0, 0.0, 0.0),
                     (1.0, 0.0, 1.0, 0.0),
                     (0.0, 1.0, 1.0, 0.0),
                     )
        for boxcolor in boxcolors:
            c.set_cmyk(*boxcolor)
            ctx.set_nonstroke(c)
            ctx.cmd_re(xoff, yoff, boxsize, boxsize)
            ctx.cmd_f()
            xoff += boxsize

    def draw_gradient_boxes(self, ctx, xoff, yoff, c1, c2):
        num_slots = 100
        height = 100
        slot_height = height / num_slots
        c = capypdf.Color()
        for i in range(num_slots+1):
            curcolor = lerp(i/num_slots, c1, c2)
            c.set_cmyk(*tuple(curcolor))
            ctx.set_nonstroke(c)
            ctx.cmd_re(xoff, yoff, 20, slot_height)
            ctx.cmd_f()
            yoff -= slot_height

    def draw_colorbars(self, ctx):
        self.draw_color_boxes(ctx)
        self.draw_gradient_boxes(ctx,
                                 10,
                                 self.mediah - 2*self.trim_corner,
                                 [1, 0, 0, 0],
                                 [0, 0, 0, 0])
        self.draw_gradient_boxes(ctx,
                                 self.mediaw - 30,
                                 self.mediah - 2*self.trim_corner,
                                 [0, 1, 0, 0],
                                 [0, 0, 0, 0])
        self.draw_gradient_boxes(ctx,
                                 10,
                                 self.mediah/2 - 2*self.trim_corner,
                                 [0, 0, 1, 0],
                                 [0, 0, 0, 0])
        self.draw_gradient_boxes(ctx,
                                 self.mediaw - 30,
                                 self.mediah/2 - 2*self.trim_corner,
                                 [0, 0, 0, 1],
                                 [0, 0, 0, 0])

    def draw_printer_marks(self, ctx):
        with ctx.push_gstate():
            c = capypdf.Color()
            c.set_cmyk(1, 1, 1, 1)
            ctx.set_stroke(c)
            self.draw_trim_marks(ctx)
            self.draw_registration_marks(ctx)
            self.draw_colorbars(ctx)

    def draw_content(self, ctx):
        with ctx.push_gstate():
            ctx.translate(self.trim_corner, self.trim_corner)
            c = capypdf.Color()
            c.set_cmyk(79/100, 0, 60/100, 22/100)
            ctx.set_nonstroke(c)
            ctx.cmd_re(-self.bleed,
                       self.arth + self.bleed,
                       self.artw + 2*self.bleed,
                       -6*self.bleed)
            ctx.cmd_f()
            c.set_cmyk(0, 0, 0, 1)
            ctx.set_nonstroke(c)
            ctx.cmd_w(1.5)
            ptsize = 48
            title = ctx.text_new()
            c.set_cmyk(0.1, 0, 1, 0)
            title.nonstroke_color(c)
            title.cmd_Tf(self.font, ptsize)
            title.cmd_Td(30, self.arth -2*ptsize)
            title.cmd_Tr(capypdf.TextMode.FillStroke)
            title.render_text('Document title')
            with ctx.push_gstate():
                ctx.render_text_obj(title)
            description = 'The image above is a color managed CMYK tiff.'
            dwidth = self.gen.text_width(description, self.font, 18)
            c.set_cmyk(0, 0, 0, 1)
            ctx.set_nonstroke(c)
            ctx.render_text(description,
                            self.font,
                            18,
                            self.artw/2 - dwidth/2,
                            100)
            imsize = cm2pt(10)
            ctx.translate(self.artw/2 - imsize/2,
                          200)
            ctx.scale(imsize, imsize)
            ctx.draw_image(self.image)

    def create(self):
        self.load_resources()
        with self.gen.page_draw_context() as ctx:
            self.draw_content(ctx)
            self.draw_printer_marks(ctx)
        self.gen.write()

if __name__ == '__main__':
    print('Creating a sample PDF/X3 document.')
    print('Due to license restrictions we can not commit the ICC profiles or images embedding them in the Git repo.')
    print('Thus you need to provide them yourself.')
    x3 = X3Creator('x3sample.pdf')
    x3.create()

