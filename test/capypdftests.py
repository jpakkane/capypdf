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


import unittest
import os, sys, pathlib, shutil, subprocess
import PIL.Image, PIL.ImageChops

if shutil.which('gs') is None:
    sys.exit('Ghostscript not found, test suite can not be run.')

os.environ['CAPYPDF_SO_OVERRIDE'] = 'src' # Sucks, but there does not seem to be a better injection point.
source_root = pathlib.Path(sys.argv[1])
testdata_dir = source_root / 'testoutput'
image_dir = source_root / 'images'
icc_dir = source_root / 'icc'
sys.path.append(str(source_root / 'python'))

noto_fontdir = pathlib.Path('/usr/share/fonts/truetype/noto')

sys.argv = sys.argv[0:1] + sys.argv[2:]

import capypdf

def draw_intersect_shape(ctx):
    ctx.cmd_m(50, 90)
    ctx.cmd_l(80, 10)
    ctx.cmd_l(10, 60)
    ctx.cmd_l(90, 60)
    ctx.cmd_l(20, 10)
    ctx.cmd_h()

def validate_image(basename, w, h):
    import functools
    def decorator_validate(func):
        @functools.wraps(func)
        def wrapper_validate(*args, **kwargs):
            assert(len(args) == 1)
            utobj = args[0]
            pngname = pathlib.Path(basename + '.png')
            pdfname = pathlib.Path(basename + '.pdf')
            args = (args[0], pdfname, w, h)
            try:
                pdfname.unlink()
            except Exception:
                pass
            utobj.assertFalse(os.path.exists(pdfname), 'PDF file already exists.')
            value = func(*args, **kwargs)
            the_truth = testdata_dir / pngname
            utobj.assertTrue(os.path.exists(pdfname), 'Test did not generate a PDF file.')
            utobj.assertEqual(subprocess.run(['gs',
                                              '-q',
                                              '-dNOPAUSE',
                                              '-dBATCH',
                                              '-sDEVICE=png16m',
                                              f'-g{w}x{h}',
                                              #'-dPDFFitPage',
                                              f'-sOutputFile={pngname}',
                                              str(pdfname)]).returncode, 0)
            oracle_image = PIL.Image.open(the_truth)
            gen_image = PIL.Image.open(pngname)
            diff = PIL.ImageChops.difference(oracle_image, gen_image)
            utobj.assertFalse(diff.getbbox(), 'Rendered image is different.')
            pdfname.unlink()
            pngname.unlink()
            return value
        return wrapper_validate
    return decorator_validate

def cleanup(ofilename):
    import functools
    def decorator_validate(func):
        @functools.wraps(func)
        def wrapper_validate(*args, **kwargs):
            args = tuple([args[0], ofilename] + list(args)[1:])
            value = func(*args, **kwargs)
            os.unlink(ofilename)
            return value
        return wrapper_validate
    return decorator_validate

sh6_coords = [50, 50,
              50 - 30, 50 + 30,
              50 + 20, 150 - 10,
              50, 150,
              50 + 20, 150 + 20,
              150 - 10, 150 - 5,
              150, 150,
              150 - 40, 150 - 20,
              150 + 20, 50 + 20,
              150, 50,
              150 - 15, 50 - 15,
              50 + 20, 50 + 20]
sh6_coords = [x/2 for x in sh6_coords]

class TestPDFCreation(unittest.TestCase):

    @validate_image('python_simple', 480, 640)
    def test_simple(self, ofilename, w, h):
        ofile = pathlib.Path(ofilename)
        with capypdf.Generator(ofile) as g:
            with g.page_draw_context() as ctx:
                ctx.cmd_rg(1.0, 0.0, 0.0)
                ctx.cmd_re(10, 10, 100, 100)
                ctx.cmd_f()

    @validate_image('python_text', 400, 400)
    def test_text(self, ofilename, w, h):
        opts = capypdf.Options()
        props = capypdf.PageProperties()
        props.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opts.set_default_page_properties(props)
        with capypdf.Generator(ofilename, opts) as g:
            fid = g.load_font(noto_fontdir / 'NotoSans-Regular.ttf')
            with g.page_draw_context() as ctx:
                ctx.render_text('Av, Tv, kerning yo.', fid, 12, 50, 150)

    def test_error(self):
        ofile = pathlib.Path('delme.pdf')
        if ofile.exists():
            ofile.unlink()
        with self.assertRaises(capypdf.CapyPDFException) as cm_outer:
            with capypdf.Generator(ofile) as g:
                ctx = g.page_draw_context()
                with self.assertRaises(capypdf.CapyPDFException) as cm:
                    ctx.cmd_w(-0.1)
                self.assertEqual(str(cm.exception), 'Negative line width.')
                ctx = None # Destroy without adding page, so there should be no output.
        self.assertEqual(str(cm_outer.exception), 'No pages defined.')
        self.assertFalse(ofile.exists())

    def test_line_drawing(self):
        ofile = pathlib.Path('nope.pdf')
        with capypdf.Generator(ofile) as g:
            with g.page_draw_context() as ctx:
                ctx.cmd_J(capypdf.LineCapStyle.Round)
                ctx.cmd_j(capypdf.LineJoinStyle.Bevel)
        ofile.unlink()

    @validate_image('python_image', 200, 200)
    def test_images(self, ofilename, w, h):
        opts = capypdf.Options()
        props = capypdf.PageProperties()
        props.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opts.set_default_page_properties(props)
        with capypdf.Generator(ofilename, opts) as g:
            bg_img = g.embed_jpg(image_dir / 'simple.jpg')
            mono_img = g.load_image(image_dir / '1bit_noalpha.png')
            gray_img = g.load_image(image_dir / 'gray_alpha.png')
            rgb_tif_img = g.load_image(image_dir / 'rgb_tiff.tif')
            with g.page_draw_context() as ctx:
                with ctx.push_gstate():
                    ctx.translate(10, 10)
                    ctx.scale(80, 80)
                    ctx.draw_image(bg_img)
                with ctx.push_gstate():
                    ctx.translate(0, 100)
                    ctx.translate(10, 10)
                    ctx.scale(80, 80)
                    ctx.draw_image(mono_img)
                with ctx.push_gstate():
                    ctx.translate(110, 110)
                    ctx.scale(80, 80)
                    ctx.draw_image(gray_img)
                with ctx.push_gstate():
                    ctx.translate(110, 10)
                    ctx.scale(80, 80)
                    ctx.draw_image(rgb_tif_img)

    @validate_image('python_path', 200, 200)
    def test_path(self, ofilename, w, h):
        opts = capypdf.Options()
        props = capypdf.PageProperties()
        props.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opts.set_default_page_properties(props)
        with capypdf.Generator(ofilename, opts) as g:
            with g.page_draw_context() as ctx:
                with ctx.push_gstate():
                    ctx.cmd_w(5)
                    ctx.cmd_J(capypdf.LineCapStyle.Round)
                    ctx.cmd_m(10, 10);
                    ctx.cmd_c(80, 10, 20, 90, 90, 90);
                    ctx.cmd_S();
                with ctx.push_gstate():
                    ctx.cmd_w(10)
                    ctx.translate(100, 0)
                    ctx.cmd_RG(1.0, 0.0, 0.0)
                    ctx.cmd_rg(0.9, 0.9, 0.0)
                    ctx.cmd_j(capypdf.LineJoinStyle.Bevel)
                    ctx.cmd_m(50, 90)
                    ctx.cmd_l(10, 10)
                    ctx.cmd_l(90, 10)
                    ctx.cmd_h()
                    ctx.cmd_B()
                with ctx.push_gstate():
                    ctx.translate(0, 100)
                    draw_intersect_shape(ctx)
                    ctx.cmd_w(3)
                    ctx.cmd_rg(0, 1, 0)
                    ctx.cmd_RG(0.5, 0.1, 0.5)
                    ctx.cmd_j(capypdf.LineJoinStyle.Round)
                    ctx.cmd_B()
                with ctx.push_gstate():
                    ctx.translate(100, 100)
                    ctx.cmd_w(2)
                    ctx.cmd_rg(0, 1, 0);
                    ctx.cmd_RG(0.5, 0.1, 0.5)
                    draw_intersect_shape(ctx)
                    ctx.cmd_Bstar()

    @validate_image('python_textobj', 200, 200)
    def test_text(self, ofilename, w, h):
        opts = capypdf.Options()
        props = capypdf.PageProperties()
        props.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opts.set_default_page_properties(props)
        with capypdf.Generator(ofilename, opts) as g:
            font = g.load_font(noto_fontdir / 'NotoSerif-Regular.ttf')
            with g.page_draw_context() as ctx:
                t = ctx.text_new()
                t.cmd_Tf(font, 12.0)
                t.cmd_Td(10.0, 100.0)
                t.render_text('Using text object!')
                ctx.render_text_obj(t)

    @validate_image('python_gstate', 200, 200)
    def test_gstate(self, ofilename, w, h):
        opts = capypdf.Options()
        props = capypdf.PageProperties()
        props.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opts.set_default_page_properties(props)
        with capypdf.Generator(ofilename, opts) as g:
            gstate = capypdf.GraphicsState()
            gstate.set_CA(0.1)
            gstate.set_ca(0.5)
            gsid = g.add_graphics_state(gstate)
            with g.page_draw_context() as ctx:
                ctx.cmd_gs(gsid)
                ctx.cmd_w(5)
                ctx.cmd_rg(1.0, 0.0, 0.0)
                ctx.cmd_re(10, 10, 180, 50)
                ctx.cmd_B()
                ctx.cmd_rg(0.0, 1.0, 0.0)
                ctx.cmd_re(30, 20, 100, 150)
                ctx.cmd_B()
                ctx.cmd_rg(0.0, 0.0, 1.0)
                ctx.cmd_re(80, 30, 100, 150)
                ctx.cmd_B()


    @validate_image('python_icccolor', 200, 200)
    def test_icc(self, ofilename, w, h):
        opts = capypdf.Options()
        props = capypdf.PageProperties()
        props.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opts.set_default_page_properties(props)
        with capypdf.Generator(ofilename, opts) as g:
            cs = g.load_icc_profile('/usr/share/color/icc/ghostscript/a98.icc')
            sc = capypdf.Color()
            sc.set_icc(cs, [0.1, 0.2, 0.8])
            nsc = capypdf.Color()
            nsc.set_icc(cs, [0.7, 0.2, 0.6])
            with g.page_draw_context() as ctx:
                ctx.set_stroke(sc)
                ctx.set_nonstroke(nsc)
                ctx.cmd_w(2)
                ctx.cmd_re(10, 10, 80, 80)
                ctx.cmd_B()

    @cleanup('transitions.pdf')
    def test_transitions(self, ofilename):
        opts = capypdf.Options()
        props = capypdf.PageProperties()
        props.set_pagebox(capypdf.PageBox.Media, 0, 0, 160, 90)
        opts.set_default_page_properties(props)
        with capypdf.Generator(ofilename, opts) as g:
            with g.page_draw_context() as ctx:
                pass
            with g.page_draw_context() as ctx:
                tr = capypdf.Transition(capypdf.TransitionType.Blinds, 1.0)
                ctx.set_page_transition(tr)
                ctx.cmd_rg(0.5, 0.5, 0.5)
                ctx.cmd_re(0, 0, 160, 90)
                ctx.cmd_f()

    @validate_image('python_rasterimage', 200, 200)
    def test_raster_image(self, ofilename, w, h):
        opts = capypdf.Options()
        props = capypdf.PageProperties()
        props.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opts.set_default_page_properties(props)
        with capypdf.Generator(ofilename, opts) as g:
            image = capypdf.RasterImage()
            image.set_size(2, 3)
            ba = bytearray()
            ba.append(127)
            ba.append(0)
            ba.append(0)
            ba.append(255)
            ba.append(0)
            ba.append(0)

            ba.append(0)
            ba.append(127)
            ba.append(0)
            ba.append(0)
            ba.append(255)
            ba.append(0)

            ba.append(0)
            ba.append(0)
            ba.append(127)
            ba.append(0)
            ba.append(0)
            ba.append(255)

            image.set_pixel_data(bytes(ba))
            iid = g.add_image(image)
            with g.page_draw_context() as ctx:
                ctx.translate(10, 10);
                ctx.scale(20, 30);
                ctx.draw_image(iid)

    @validate_image('python_linestyles', 200, 200)
    def test_line_styles(self, ofilename, w, h):
        prop = capypdf.PageProperties()
        prop.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opt = capypdf.Options()
        opt.set_default_page_properties(prop)
        with capypdf.Generator(ofilename, opt) as gen:
            with gen.page_draw_context() as ctx:
                ctx.scale(2.8*2.75, 2.75*2.83)
                ctx.cmd_cm(1, 0, 0, -1, 0, 25.4)
                with ctx.push_gstate():
                    ctx.cmd_cm(1, 0, 0, 1, 24.9, -134)
                    with ctx.push_gstate() as state2:
                        ctx.cmd_cm(1, 0, 0, 1, -0.0303, 0.00501)
                        with ctx.push_gstate():
                            ctx.cmd_w(1)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_m(-22.9, 136)
                            ctx.cmd_l(-12.9, 136)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(2)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_m(-9.81, 135)
                            ctx.cmd_l(-9.81, 145)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(1)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Round)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_m(-22.9, 139)
                            ctx.cmd_l(-12.9, 139)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(1)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Projection_Square)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_m(-22.9, 142)
                            ctx.cmd_l(-12.9, 142)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(1)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_d([1, 1], 0)
                            ctx.cmd_m(-22.8, 152)
                            ctx.cmd_l(-1, 152)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(1)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_m(-22.8, 150)
                            ctx.cmd_l(-16.3, 150)
                            ctx.cmd_m(-7.51, 150)
                            ctx.cmd_l(-0.886, 150)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(1)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_m(-18.5, 144)
                            ctx.cmd_l(-17.3, 146)
                            ctx.cmd_l(-18.5, 148)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(1)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Bevel)
                            ctx.cmd_m(-20.4, 144)
                            ctx.cmd_l(-19.2, 146)
                            ctx.cmd_l(-20.5, 148)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(1)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Round)
                            ctx.cmd_m(-22.6, 144)
                            ctx.cmd_l(-21.4, 146)
                            ctx.cmd_l(-22.6, 148)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(3)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_m(-6.65, 135)
                            ctx.cmd_l(-6.65, 145)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(4)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_m(-2.48, 135)
                            ctx.cmd_l(-2.48, 145)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(1)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_m(-16.2, 144)
                            ctx.cmd_l(-14.2, 146)
                            ctx.cmd_l(-16.3, 148)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(1)
                            ctx.cmd_M(1)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_m(-14.1, 144)
                            ctx.cmd_l(-12.1, 146)
                            ctx.cmd_l(-14.2, 148)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(1)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_d([2], 0)
                            ctx.cmd_m(-22.8, 154)
                            ctx.cmd_l(-1, 154)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(0.5)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_d([1], 0)
                            ctx.cmd_m(-22.8, 156)
                            ctx.cmd_l(-1, 156)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(0.5)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_d([1], 0.5)
                            ctx.cmd_m(-22.8, 158)
                            ctx.cmd_l(-0.994, 158)
                            ctx.cmd_S()
                        with ctx.push_gstate():
                            ctx.cmd_w(0.5)
                            ctx.cmd_M(10)
                            ctx.cmd_J(capypdf.LineCapStyle.Butt)
                            ctx.cmd_j(capypdf.LineJoinStyle.Miter)
                            ctx.cmd_d([1], 0.25)
                            ctx.cmd_m(-22.9, 157)
                            ctx.cmd_l(-1.02, 157)
                            ctx.cmd_S()

    @validate_image('python_shading_rgb', 200, 200)
    def test_shading_rgb(self, ofilename, w, h):
        prop = capypdf.PageProperties()
        prop.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opt = capypdf.Options()
        opt.set_default_page_properties(prop)
        with capypdf.Generator(ofilename, opt) as gen:
            c1 = capypdf.Color()
            c1.set_rgb(0.0, 1.0, 0.0)
            c2 = capypdf.Color()
            c2.set_rgb(1.0, 0.0, 1.0)
            f2 = capypdf.Type2Function([0.0, 1.0], c1, c2, 1.0)
            f2id = gen.add_type2_function(f2)
            sh2 = capypdf.Type2Shading(capypdf.Colorspace.DeviceRGB,
                                       10.0,
                                       50.0,
                                       90.0,
                                       50.0,
                                       f2id,
                                       False,
                                       False)
            sh2id = gen.add_type2_shading(sh2)

            sh3 = capypdf.Type3Shading(capypdf.Colorspace.DeviceRGB,
                                       [50, 50, 40, 40, 30, 10],
                                       f2id,
                                       False,
                                       True)
            sh3id = gen.add_type3_shading(sh3)

            sh4 = capypdf.Type4Shading(capypdf.Colorspace.DeviceRGB, 0, 0, 100, 100)
            c1 = capypdf.Color()
            c1.set_rgb(1, 0, 0)
            c2 = capypdf.Color()
            c2.set_rgb(0, 1, 0)
            c3 = capypdf.Color()
            c3.set_rgb(0, 0, 1)
            sh4.add_triangle([50, 90,
                              10, 10,
                              90, 10],
                             [c1, c2, c3])
            sh4.extend(2, [90, 90], c2)
            sh4id = gen.add_type4_shading(sh4)

            sh6 = capypdf.Type6Shading(capypdf.Colorspace.DeviceRGB, 0, 0, 100, 100)
            sh6_colors = [capypdf.Color(), capypdf.Color(), capypdf.Color(), capypdf.Color()]
            sh6_colors[0].set_rgb(1, 0, 0)
            sh6_colors[1].set_rgb(0, 1, 0)
            sh6_colors[2].set_rgb(0, 0, 1)
            sh6_colors[3].set_rgb(1, 0, 1)
            sh6.add_patch(sh6_coords, sh6_colors)
            sh6id = gen.add_type6_shading(sh6)

            with gen.page_draw_context() as ctx:
                with ctx.push_gstate():
                    ctx.cmd_re(10, 10, 80, 80)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh2id)
                with ctx.push_gstate():
                    ctx.translate(100, 0)
                    ctx.cmd_re(10, 10, 80, 80)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh3id)
                with ctx.push_gstate():
                    ctx.translate(0, 100)
                    ctx.cmd_re(10, 10, 80, 80)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh4id)
                with ctx.push_gstate():
                    ctx.translate(100, 100)
                    ctx.cmd_re(0, 0, 100, 100)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh6id)

    @validate_image('python_shading_gray', 200, 200)
    def test_shading_gray(self, ofilename, w, h):
        prop = capypdf.PageProperties()
        prop.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opt = capypdf.Options()
        opt.set_default_page_properties(prop)
        opt.set_colorspace(capypdf.Colorspace.DeviceGray)
        with capypdf.Generator(ofilename, opt) as gen:
            c1 = capypdf.Color()
            c1.set_gray(0.0)
            c2 = capypdf.Color()
            c2.set_gray(1.0)
            f2 = capypdf.Type2Function([0.0, 1.0], c1, c2, 1.0)
            f2id = gen.add_type2_function(f2)
            sh2 = capypdf.Type2Shading(capypdf.Colorspace.DeviceGray,
                                       10.0,
                                       50.0,
                                       90.0,
                                       50.0,
                                       f2id,
                                       False,
                                       False)
            sh2id = gen.add_type2_shading(sh2)

            sh3 = capypdf.Type3Shading(capypdf.Colorspace.DeviceGray,
                                       [50, 50, 40, 40, 30, 10],
                                       f2id,
                                       False,
                                       True)
            sh3id = gen.add_type3_shading(sh3)

            sh4 = capypdf.Type4Shading(capypdf.Colorspace.DeviceGray, 0, 0, 100, 100)
            c1 = capypdf.Color()
            c1.set_gray(1)
            c2 = capypdf.Color()
            c2.set_gray(0)
            c3 = capypdf.Color()
            c3.set_gray(0.5)
            sh4.add_triangle([50, 90,
                              10, 10,
                              90, 10],
                             [c1, c2, c3])
            sh4.extend(2, [90, 90], c2)
            sh4id = gen.add_type4_shading(sh4)

            sh6 = capypdf.Type6Shading(capypdf.Colorspace.DeviceGray, 0, 0, 100, 100)
            sh6_colors = [capypdf.Color(), capypdf.Color(), capypdf.Color(), capypdf.Color()]
            sh6_colors[0].set_gray(0)
            sh6_colors[1].set_gray(0.3)
            sh6_colors[2].set_gray(0.6)
            sh6_colors[3].set_gray(1)
            sh6.add_patch(sh6_coords, sh6_colors)
            sh6id = gen.add_type6_shading(sh6)

            with gen.page_draw_context() as ctx:
                with ctx.push_gstate():
                    ctx.cmd_re(10, 10, 80, 80)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh2id)
                with ctx.push_gstate():
                    ctx.translate(100, 0)
                    ctx.cmd_re(10, 10, 80, 80)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh3id)
                with ctx.push_gstate():
                    ctx.translate(0, 100)
                    ctx.cmd_re(10, 10, 80, 80)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh4id)
                with ctx.push_gstate():
                    ctx.translate(100, 100)
                    ctx.cmd_re(0, 0, 100, 100)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh6id)

    @validate_image('python_shading_cmyk', 200, 200)
    def test_shading_cmyk(self, ofilename, w, h):
        prop = capypdf.PageProperties()
        prop.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opt = capypdf.Options()
        opt.set_default_page_properties(prop)
        opt.set_colorspace(capypdf.Colorspace.DeviceCMYK)
        opt.set_device_profile(capypdf.Colorspace.DeviceCMYK, icc_dir / 'FOGRA29L.icc')
        with capypdf.Generator(ofilename, opt) as gen:
            c1 = capypdf.Color()
            c1.set_cmyk(0.9, 0, 0.9, 0)
            c2 = capypdf.Color()
            c2.set_cmyk(0, 0.9, 0, 0.9)
            f2 = capypdf.Type2Function([0.0, 1.0], c1, c2, 1.0)
            f2id = gen.add_type2_function(f2)
            sh2 = capypdf.Type2Shading(capypdf.Colorspace.DeviceCMYK,
                                       10.0,
                                       50.0,
                                       90.0,
                                       50.0,
                                       f2id,
                                       False,
                                       False)
            sh2id = gen.add_type2_shading(sh2)

            sh3 = capypdf.Type3Shading(capypdf.Colorspace.DeviceCMYK,
                                       [50, 50, 40, 40, 30, 10],
                                       f2id,
                                       False,
                                       True)
            sh3id = gen.add_type3_shading(sh3)

            sh4 = capypdf.Type4Shading(capypdf.Colorspace.DeviceCMYK, 0, 0, 100, 100)
            c1 = capypdf.Color()
            c1.set_cmyk(1, 0, 0, 0)
            c2 = capypdf.Color()
            c2.set_cmyk(0, 1, 0, 0)
            c3 = capypdf.Color()
            c3.set_cmyk(0, 0, 1, 0)
            c4 = capypdf.Color()
            c4.set_cmyk(0, 0, 0, 1)
            sh4.add_triangle([50, 90,
                              10, 10,
                              90, 10],
                             [c1, c2, c3])
            sh4.extend(2, [90, 90], c4)
            sh4id = gen.add_type4_shading(sh4)

            sh6 = capypdf.Type6Shading(capypdf.Colorspace.DeviceCMYK, 0, 0, 100, 100)
            sh6_colors = [capypdf.Color(), capypdf.Color(), capypdf.Color(), capypdf.Color()]
            sh6_colors[0].set_cmyk(1, 0, 0, 0)
            sh6_colors[1].set_cmyk(0, 1, 0, 0)
            sh6_colors[2].set_cmyk(0, 0, 1, 0)
            sh6_colors[3].set_cmyk(0, 0, 0, 1)
            sh6.add_patch(sh6_coords, sh6_colors)
            sh6id = gen.add_type6_shading(sh6)

            with gen.page_draw_context() as ctx:
                with ctx.push_gstate():
                    ctx.cmd_re(10, 10, 80, 80)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh2id)
                with ctx.push_gstate():
                    ctx.translate(100, 0)
                    ctx.cmd_re(10, 10, 80, 80)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh3id)
                with ctx.push_gstate():
                    ctx.translate(0, 100)
                    ctx.cmd_re(10, 10, 80, 80)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh4id)
                with ctx.push_gstate():
                    ctx.translate(100, 100)
                    ctx.cmd_re(0, 0, 100, 100)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh6id)

    @validate_image('python_imagemask', 200, 200)
    def test_imagemask(self, ofilename, w, h):
        prop = capypdf.PageProperties()
        prop.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opt = capypdf.Options()
        opt.set_default_page_properties(prop)
        with capypdf.Generator(ofilename, opt) as gen:
            artfile = image_dir / 'comic-lines.png'
            self.assertTrue(artfile.exists())
            maskid = gen.load_mask_image(artfile)
            with gen.page_draw_context() as ctx:
                ctx.cmd_re(0, 0, 100, 200)
                ctx.cmd_rg(0.9, 0.4, 0.25)
                ctx.cmd_f()
                ctx.cmd_re(100, 0, 100, 200)
                ctx.cmd_rg(0.9, 0.85, 0)
                ctx.cmd_f()
                with ctx.push_gstate():
                    ctx.cmd_rg(0.06, 0.26, 0.05)
                    ctx.translate(50, 85)
                    ctx.scale(100, 100)
                    ctx.draw_image(maskid)
                with ctx.push_gstate():
                    alphags = capypdf.GraphicsState()
                    alphags.set_ca(0.5)
                    gsid = gen.add_graphics_state(alphags)
                    ctx.cmd_rg(0.06, 0.26, 0.05)
                    ctx.translate(50, 10)
                    ctx.scale(100, 100)
                    ctx.translate(0, 0.5)
                    ctx.scale(1, -1)
                    ctx.translate(0, -0.5)
                    ctx.cmd_gs(gsid)
                    ctx.draw_image(maskid)

    @cleanup('outlines')
    def test_outline(self, ofilename):
        w = 200
        h = 200
        prop = capypdf.PageProperties()
        prop.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opt = capypdf.Options()
        opt.set_default_page_properties(prop)
        with capypdf.Generator(ofilename, opt) as gen:
            with gen.page_draw_context() as ctx:
                ctx.cmd_re(50, 50, 100, 100)
                ctx.cmd_f()
            t1 = gen.add_outline('First toplevel', 0)
            t2 = gen.add_outline('Second toplevel', 0)
            t3 = gen.add_outline('Third toplevel', 0)
            t1c1 = gen.add_outline('Top1, child1', 0, t1)
            t1c2 = gen.add_outline('Top1, child2', 0, t1)
            gen.add_outline('Top1, child2, child1', 0, t1c2)
            gen.add_outline('Fourth toplevel', 0)
        # FIXME, validate that the outline tree is correct.

    @validate_image('python_separation', 200, 200)
    def test_separation(self, ofilename, w, h):
        prop = capypdf.PageProperties()
        prop.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opt = capypdf.Options()
        opt.set_default_page_properties(prop)
        opt.set_colorspace(capypdf.Colorspace.DeviceCMYK)
        opt.set_device_profile(capypdf.Colorspace.DeviceCMYK, icc_dir / 'FOGRA29L.icc')
        with capypdf.Generator(ofilename, opt) as gen:
            red = capypdf.Color()
            red.set_cmyk(0.2, 1, 0.8, 0)
            gold = capypdf.Color()
            gold.set_cmyk(0, 0.03, 0.55, 0.08)
            sepid = gen.create_separation_simple("gold", gold)
            gold.set_separation(sepid, 1.0)
            with gen.page_draw_context() as ctx:
                ctx.set_nonstroke(red)
                ctx.cmd_re(10, 10, 180, 180)
                ctx.cmd_f()
                ctx.set_nonstroke(gold)
                ctx.cmd_re(50, 90, 100, 20)
                ctx.cmd_f()

    @validate_image('python_blendmodes', 200, 200)
    def test_blendmodes(self, ofilename, w, h):
        prop = capypdf.PageProperties()
        prop.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opt = capypdf.Options()
        opt.set_default_page_properties(prop)
        with capypdf.Generator(ofilename, opt) as gen:
            bgimage = gen.load_image(image_dir / 'flame_gradient.png')
            fgimage = gen.load_image(image_dir / 'object_gradient.png')
            with gen.page_draw_context() as ctx:
                with ctx.push_gstate():
                    ctx.scale(200, 200)
                    ctx.draw_image(bgimage)
                for j in range(4):
                    for i in range(4):
                        bm = capypdf.BlendMode(j*4+i)
                        g = capypdf.GraphicsState()
                        g.set_BM(bm)
                        bmid = gen.add_graphics_state(g)
                        with ctx.push_gstate():
                            ctx.cmd_gs(bmid)
                            ctx.translate(50*i, 150-j*50)
                            ctx.scale(50, 50)
                            ctx.draw_image(fgimage)

    @validate_image('python_colorpattern', 200, 200)
    def test_colorpattern(self, ofilename, w, h):
        prop = capypdf.PageProperties()
        prop.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opt = capypdf.Options()
        opt.set_default_page_properties(prop)
        with capypdf.Generator(ofilename, opt) as gen:
            font = gen.load_font(noto_fontdir / 'NotoSerif-Regular.ttf')
            # Repeating pattern.
            pctx = gen.create_color_pattern_context(10, 10)
            pctx.cmd_rg(0.9, 0.8, 0.8)
            pctx.cmd_re(0, 0, 10, 10)
            pctx.cmd_f()
            pctx.cmd_rg(0.9, 0.1, 0.1)
            pctx.cmd_re(0, 2.5, 2.5, 5)
            pctx.cmd_f()
            pctx.cmd_re(5, 0, 2.5, 2.5)
            pctx.cmd_f()
            pctx.cmd_re(5, 7.5, 2.5, 2.5)
            pctx.cmd_f()
            patternid = gen.add_color_pattern(pctx)
            # Text
            textctx = gen.create_color_pattern_context(3, 4)
            textctx.render_text("g", font, 3, 0, 2)
            textpatternid = gen.add_color_pattern(textctx)

            with gen.page_draw_context() as ctx:
                with ctx.push_gstate():
                    ctx.cmd_re(10, 10, 80, 80)
                    ctx.set_nonstroke(patternid)
                    ctx.cmd_RG(0, 0, 0);
                    ctx.cmd_j(capypdf.LineJoinStyle.Round)
                    ctx.cmd_w(1.5)
                    ctx.cmd_B()
                with ctx.push_gstate():
                    ctx.translate(110, 10)
                    ctx.set_nonstroke(textpatternid)
                    ctx.render_text("C", font, 100, 0, 5)

    @validate_image('python_annotate', 200, 200)
    def test_annotate(self, ofilename, w, h):
        prop = capypdf.PageProperties()
        prop.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        opt = capypdf.Options()
        opt.set_default_page_properties(prop)
        with capypdf.Generator(ofilename, opt) as gen:
            ta = capypdf.Annotation.new_text_annotation('This is a text ännotation.')
            ta.set_rectangle(30, 180, 40, 190)
            taid = gen.create_annotation(ta)
            fid = gen.load_font(noto_fontdir / 'NotoSans-Regular.ttf')
            with gen.page_draw_context() as ctx:
                ctx.annotate(taid)
                ctx.render_text("<- This is a text annotation", fid, 11, 50, 180)

if __name__ == "__main__":
    unittest.main()
