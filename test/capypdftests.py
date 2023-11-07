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

gs_exes = ['gs', 'gswin64', 'gswin32']
gs_exe = None
gswin_path = None
for exe in gs_exes:
    if gs_exe is None and shutil.which(exe) is not None:
        # For Windows Ghostscript installations, find installation path
        if os.name == 'nt':
            gswin_path = os.path.dirname(os.path.dirname(shutil.which(exe)))
        gs_exe = exe

if gs_exe is None:
    sys.exit('Ghostscript not found, test suite can not be run.')

os.environ['CAPYPDF_SO_OVERRIDE'] = 'src' # Sucks, but there does not seem to be a better injection point.
source_root = pathlib.Path(sys.argv[1])
testdata_dir = source_root / 'testoutput'
image_dir = source_root / 'images'
icc_dir = source_root / 'icc'
sys.path.append(str(source_root / 'python'))

if os.name == 'nt':
    noto_fontdir = pathlib.Path('c:/Windows/fonts')
else:
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
            utobj.assertEqual(subprocess.run([gs_exe,
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
            if os.name == 'nt':
                a98icc = os.path.join(gswin_path, 'iccprofiles', 'a98.icc')
            else:
                a98icc = '/usr/share/color/icc/ghostscript/a98.icc'

            cs = g.load_icc_profile(a98icc)
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

    @validate_image('python_shading', 200, 200)
    def test_shading(self, ofilename, w, h):
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

if __name__ == "__main__":
    unittest.main()
