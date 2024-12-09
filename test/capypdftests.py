#!/usr/bin/env python3

# SPDX-License-Identifier: Apache-2.0
# Copyright 2023-2024 Jussi Pakkanen


import unittest
import os, sys, pathlib, shutil, subprocess
try:
    import PIL.Image, PIL.ImageChops
except ModuleNotFoundError:
    sys.exit('PIL not found, test suite can not be run.')

if shutil.which('gs') is None:
    sys.exit('Ghostscript not found, test suite can not be run.')

os.environ['CAPYPDF_SO_OVERRIDE'] = 'src' # Sucks, but there does not seem to be a better injection point.
source_root = pathlib.Path(__file__).parent.parent
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
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        dprops.set_language('en-US')
        with capypdf.Generator(ofilename, dprops) as g:
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

    @validate_image('python_image', 200, 200)
    def test_images(self, ofilename, w, h):
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
            params = capypdf.ImagePdfProperties()
            bg_img_ri = g.load_image(image_dir / 'simple.jpg')
            bg_img = g.add_image(bg_img_ri, params)
            mono_img_ri = g.load_image(image_dir / '1bit_noalpha.png')
            mono_img = g.add_image(mono_img_ri, params)
            gray_img_ri = g.load_image(image_dir / 'gray_alpha.png')
            gray_img = g.add_image(gray_img_ri, params)
            rgb_tif_img_ri = g.load_image(image_dir / 'rgb_tiff.tif')
            rgb_tif_img = g.add_image(rgb_tif_img_ri, params)
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
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
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
    def test_textobj(self, ofilename, w, h):
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
            font = g.load_font(noto_fontdir / 'NotoSerif-Regular.ttf')
            with g.page_draw_context() as ctx:
                t = ctx.text_new()
                t.cmd_Tf(font, 12.0)
                t.cmd_Td(10.0, 100.0)
                t.render_text('Using text object!')
                ctx.render_text_obj(t)

    @validate_image('python_kerning', 200, 200)
    def test_kerning(self, ofilename, w, h):
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
            font = g.load_font(noto_fontdir / 'NotoSerif-Regular.ttf')
            with g.page_draw_context() as ctx:
                t = ctx.text_new()
                ks = capypdf.TextSequence()
                t.cmd_Tf(font, 24.0)
                t.cmd_Td(10.0, 120.0)
                ks.append_codepoint(ord('A'))
                ks.append_codepoint(ord('V'))
                t.cmd_TJ(ks)
                t.cmd_Td(0, -40)
                ks.append_codepoint('A')
                ks.append_kerning(200)
                ks.append_codepoint('V')
                t.cmd_TJ(ks)
                ctx.render_text_obj(t)

    @validate_image('python_shaping', 200, 200)
    def test_shaping(self, ofilename, w, h):
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
            font = g.load_font(noto_fontdir / 'NotoSerif-Regular.ttf')
            with g.page_draw_context() as ctx:
                t = ctx.text_new()
                t.cmd_Tf(font, 48)
                t.cmd_Td(10, 100)
                ts = capypdf.TextSequence()
                ts.append_codepoint(ord('A'))
                ts.append_actualtext_start('ffi')
                ts.append_codepoint(0xFB03)
                ts.append_actualtext_end()
                ts.append_codepoint(ord(('x')))
                t.cmd_TJ(ts)
                ctx.render_text_obj(t)

    @validate_image('python_shaping2', 200, 200)
    def test_shaping2(self, ofilename, w, h):
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
            font = g.load_font(noto_fontdir / 'NotoSerif-Regular.ttf')
            with g.page_draw_context() as ctx:
                t = ctx.text_new()
                t.cmd_Tf(font, 48)
                t.cmd_Td(10, 100)
                ts = capypdf.TextSequence()
                ts.append_codepoint(ord('A'))
                ts.append_ligature_glyph(2132, 'ffi') # Should have CMAP entry with multiple source characters. Not actually checked ATM.
                ts.append_codepoint(ord(('x')))
                t.cmd_TJ(ts)
                ctx.render_text_obj(t)

    @validate_image('python_smallcaps', 200, 200)
    def test_smallcaps(self, ofilename, w, h):
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
            font = g.load_font(noto_fontdir / 'NotoSerif-Regular.ttf')
            seq = ((54, 'S'),
                   (2200, 'm'),
                   (2136, 'a'),
                   (2194, 'l'),
                   (2194, 'l'),
                   (3, ' '),
                   (38, 'C'),
                   (2156, 'a'),
                   (2219, 'p'),
                   (2226, 's'))

            with g.page_draw_context() as ctx:
                t = ctx.text_new()
                t.cmd_Tf(font, 24)
                t.cmd_Td(10, 100)
                ts = capypdf.TextSequence()
                for gid, codepoint in seq:
                    ts.append_raw_glyph(gid, codepoint)
                t.cmd_TJ(ts)
                ctx.render_text_obj(t)

    @validate_image('python_unusualfont', 200, 200)
    def test_unusualfont(self, ofilename, w, h):
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
            font = g.load_font(noto_fontdir / 'NotoSansSymbols2-Regular.ttf')
            with g.page_draw_context() as ctx:
                ctx.render_text("🖙", font, 100, 30, 80)


    @validate_image('python_lab', 200, 200)
    def test_lab(self, ofilename, w, h):
        from math import sin, cos
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
            lab = g.add_lab_colorspace(0.9505, 1.0, 1.089, -128, 127, -128, 127)
            with g.page_draw_context() as ctx:
                ctx.cmd_w(4)
                ctx.translate(100, 100)
                num_boxes = 16
                radius = 80
                box_size = 20
                max_ab = 127
                color = capypdf.Color()
                for i in range(num_boxes):
                    with ctx.push_gstate():
                        l = 50
                        darkl = 40
                        angle = 2*3.14159 * i/num_boxes
                        a = max_ab * cos(angle)
                        b = max_ab * sin(angle)
                        color.set_lab(lab, l, a, b)
                        ctx.set_nonstroke(color)
                        color.set_lab(lab, darkl, a, b)
                        ctx.set_stroke(color)
                        ctx.translate(radius*cos(angle), radius*sin(angle))
                        ctx.cmd_re(-box_size/2, -box_size/2, box_size, box_size)
                        ctx.cmd_B()


    @validate_image('python_gstate', 200, 200)
    def test_gstate(self, ofilename, w, h):
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
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
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
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
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, 160, 90)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
            with g.page_draw_context() as ctx:
                pass
            with g.page_draw_context() as ctx:
                tr = capypdf.Transition()
                tr.set_S(capypdf.TransitionType.Blinds)
                tr.set_D(1.0)
                ctx.set_page_transition(tr)
                ctx.cmd_rg(0.5, 0.5, 0.5)
                ctx.cmd_re(0, 0, 160, 90)
                ctx.cmd_f()

    def build_rasterdata(self, maxval):
        ba = bytearray()
        ba.append(maxval//2)
        ba.append(0)
        ba.append(0)
        ba.append(maxval)
        ba.append(0)
        ba.append(0)

        ba.append(0)
        ba.append(maxval//2)
        ba.append(0)
        ba.append(0)
        ba.append(maxval)
        ba.append(0)

        ba.append(0)
        ba.append(0)
        ba.append(maxval//2)
        ba.append(0)
        ba.append(0)
        ba.append(maxval)

        return bytes(ba)

    @validate_image('python_rasterimage', 200, 200)
    def test_raster_image(self, ofilename, w, h):
        import zlib
        dprops = capypdf.DocumentProperties()
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as g:
            ib = capypdf.RasterImageBuilder()
            ib.set_size(2, 3)
            ba = self.build_rasterdata(255)

            ib.set_pixel_data(ba)
            image = ib.build()
            ipar = capypdf.ImagePdfProperties()
            iid = g.add_image(image, ipar)
            ib.set_size(2, 3)
            ib.set_pixel_data(zlib.compress(self.build_rasterdata(127), 9))
            ib.set_compression(capypdf.Compression.Deflate)
            comprimage = ib.build()
            ciid = g.add_image(comprimage, ipar)
            with g.page_draw_context() as ctx:
                with ctx.push_gstate():
                    ctx.translate(10, 10)
                    ctx.scale(20, 30)
                    ctx.draw_image(iid)
                with ctx.push_gstate():
                    ctx.translate(40, 40)
                    ctx.scale(20, 30)
                    ctx.draw_image(ciid)

    @validate_image('python_linestyles', 200, 200)
    def test_line_styles(self, ofilename, w, h):
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as gen:
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

    @validate_image('python_shading_rgb', 300, 200)
    def test_shading_rgb(self, ofilename, w, h):
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as gen:
            c1 = capypdf.Color()
            c1.set_rgb(0.0, 1.0, 0.0)
            c2 = capypdf.Color()
            c2.set_rgb(1.0, 0.0, 1.0)
            f2 = capypdf.Type2Function([0.0, 1.0], c1, c2, 1.0)
            f2id = gen.add_function(f2)
            sh2 = capypdf.Type2Shading(capypdf.DeviceColorspace.RGB,
                                       10.0,
                                       50.0,
                                       90.0,
                                       50.0,
                                       f2id)
            sh2id = gen.add_shading(sh2)

            f2o = capypdf.Type2Function([0.0, 1.0], c2, c1, 1.0)
            f2oid = gen.add_function(f2o)
            f3 = capypdf.Type3Function([0.0, 1.0], [f2id, f2oid], [0.7], [0.0, 1.0, 0.0, 1.0]);
            f3id = gen.add_function(f3)

            sh2f3 = capypdf.Type2Shading(capypdf.DeviceColorspace.RGB,
                                       10.0,
                                       50.0,
                                       90.0,
                                       50.0,
                                       f3id)
            sh2f3id = gen.add_shading(sh2f3)

            sh3 = capypdf.Type3Shading(capypdf.DeviceColorspace.RGB,
                                       [50, 50, 40, 40, 30, 10],
                                       f2id)
            sh3.set_extend(False, True)
            sh3id = gen.add_shading(sh3)

            sh4 = capypdf.Type4Shading(capypdf.DeviceColorspace.RGB, 0, 0, 100, 100)
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
            sh4id = gen.add_shading(sh4)

            sh6 = capypdf.Type6Shading(capypdf.DeviceColorspace.RGB, 0, 0, 100, 100)
            sh6_colors = [capypdf.Color(), capypdf.Color(), capypdf.Color(), capypdf.Color()]
            sh6_colors[0].set_rgb(1, 0, 0)
            sh6_colors[1].set_rgb(0, 1, 0)
            sh6_colors[2].set_rgb(0, 0, 1)
            sh6_colors[3].set_rgb(1, 0, 1)
            sh6.add_patch(sh6_coords, sh6_colors)
            sh6id = gen.add_shading(sh6)

            with gen.page_draw_context() as ctx:
                with ctx.push_gstate():
                    ctx.cmd_re(10, 10, 80, 80)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh2id)
                with ctx.push_gstate():
                    ctx.translate(200, 0)
                    ctx.cmd_re(10, 10, 80, 80)
                    ctx.cmd_Wstar()
                    ctx.cmd_n()
                    ctx.cmd_sh(sh2f3id)
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
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        dprops.set_colorspace(capypdf.DeviceColorspace.Gray)
        with capypdf.Generator(ofilename, dprops) as gen:
            c1 = capypdf.Color()
            c1.set_gray(0.0)
            c2 = capypdf.Color()
            c2.set_gray(1.0)
            f2 = capypdf.Type2Function([0.0, 1.0], c1, c2, 1.0)
            f2id = gen.add_function(f2)
            sh2 = capypdf.Type2Shading(capypdf.DeviceColorspace.Gray,
                                       10.0,
                                       50.0,
                                       90.0,
                                       50.0,
                                       f2id)
            sh2id = gen.add_shading(sh2)

            sh3 = capypdf.Type3Shading(capypdf.DeviceColorspace.Gray,
                                       [50, 50, 40, 40, 30, 10],
                                       f2id)
            sh3.set_extend(False, True)
            sh3id = gen.add_shading(sh3)

            sh4 = capypdf.Type4Shading(capypdf.DeviceColorspace.Gray, 0, 0, 100, 100)
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
            sh4id = gen.add_shading(sh4)

            sh6 = capypdf.Type6Shading(capypdf.DeviceColorspace.Gray, 0, 0, 100, 100)
            sh6_colors = [capypdf.Color(), capypdf.Color(), capypdf.Color(), capypdf.Color()]
            sh6_colors[0].set_gray(0)
            sh6_colors[1].set_gray(0.3)
            sh6_colors[2].set_gray(0.6)
            sh6_colors[3].set_gray(1)
            sh6.add_patch(sh6_coords, sh6_colors)
            sh6id = gen.add_shading(sh6)

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
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        dprops.set_colorspace(capypdf.DeviceColorspace.CMYK)
        dprops.set_device_profile(capypdf.DeviceColorspace.CMYK, icc_dir / 'FOGRA29L.icc')
        with capypdf.Generator(ofilename, dprops) as gen:
            c1 = capypdf.Color()
            c1.set_cmyk(0.9, 0, 0.9, 0)
            c2 = capypdf.Color()
            c2.set_cmyk(0, 0.9, 0, 0.9)
            f2 = capypdf.Type2Function([0.0, 1.0], c1, c2, 1.0)
            f2id = gen.add_function(f2)
            sh2 = capypdf.Type2Shading(capypdf.DeviceColorspace.CMYK,
                                       10.0,
                                       50.0,
                                       90.0,
                                       50.0,
                                       f2id)
            sh2id = gen.add_shading(sh2)

            sh3 = capypdf.Type3Shading(capypdf.DeviceColorspace.CMYK,
                                       [50, 50, 40, 40, 30, 10],
                                       f2id)
            sh3.set_extend(False, True)
            sh3id = gen.add_shading(sh3)

            sh4 = capypdf.Type4Shading(capypdf.DeviceColorspace.CMYK, 0, 0, 100, 100)
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
            sh4id = gen.add_shading(sh4)

            sh6 = capypdf.Type6Shading(capypdf.DeviceColorspace.CMYK, 0, 0, 100, 100)
            sh6_colors = [capypdf.Color(), capypdf.Color(), capypdf.Color(), capypdf.Color()]
            sh6_colors[0].set_cmyk(1, 0, 0, 0)
            sh6_colors[1].set_cmyk(0, 1, 0, 0)
            sh6_colors[2].set_cmyk(0, 0, 1, 0)
            sh6_colors[3].set_cmyk(0, 0, 0, 1)
            sh6.add_patch(sh6_coords, sh6_colors)
            sh6id = gen.add_shading(sh6)

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
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as gen:
            artfile = image_dir / 'comic-lines.png'
            self.assertTrue(artfile.exists())
            maskopt = capypdf.ImagePdfProperties()
            maskopt.set_mask(True)
            maskimage = gen.load_image(artfile)
            maskid = gen.add_image(maskimage, maskopt)
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
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as gen:
            # Destinations point to a page that does not exist when they
            # are created but does exist when the PDF is generated.
            d = capypdf.Destination()
            d.set_page_xyz(0, 10, 20, 1.0)
            o = capypdf.Outline()
            o.set_title('First toplevel')
            o.set_destination(d)
            t1 = gen.add_outline(o)
            o2 = capypdf.Outline()
            o2.set_title('Second toplevel')
            o2.set_destination(d)
            t2 = gen.add_outline(o2)
            o3 = capypdf.Outline()
            o3.set_title('Third toplevel')
            t3 = gen.add_outline(o3)
            o4 = capypdf.Outline()
            o4.set_title('Top1, child1')
            o4.set_parent(t1)
            t1c1 = gen.add_outline(o4)
            o5 = capypdf.Outline()
            o5.set_title('Top1, child2')
            o5.set_parent(t1)
            t1c2 = gen.add_outline(o5)
            o6 = capypdf.Outline()
            o6.set_title('Top1, child2, child1')
            o6.set_parent(t1c2)
            o6.set_F(1)
            o6.set_C(0, 0, 1)
            gen.add_outline(o6)
            o7 = capypdf.Outline()
            o7.set_title('Fourth toplevel')
            o7.set_F(2)
            gen.add_outline(o7)

            with gen.page_draw_context() as ctx:
                ctx.cmd_re(50, 50, 100, 100)
                ctx.cmd_f()
        # FIXME, validate that the outline tree is correct.

    @validate_image('python_separation', 200, 200)
    def test_separation(self, ofilename, w, h):
        gold_c = 0.0
        gold_m = 0.03
        gold_y = 0.55
        gold_k = 0.08
        function_code = f'''{{
  dup
  {gold_c} mul exch
  {gold_m} exch dup
  {gold_y} mul exch
  {gold_k} mul
}}
'''
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        dprops.set_colorspace(capypdf.DeviceColorspace.CMYK)
        dprops.set_device_profile(capypdf.DeviceColorspace.CMYK, icc_dir / 'FOGRA29L.icc')
        with capypdf.Generator(ofilename, dprops) as gen:
            red = capypdf.Color()
            gold = capypdf.Color()
            red.set_cmyk(0.2, 1, 0.8, 0)
            f4 = capypdf.Type4Function([0, 1],
                                       [0, 1, 0, 1, 0, 1, 0, 1],
                                       function_code)
            f4id  = gen.add_function(f4)
            sepid = gen.add_separation("gold", capypdf.DeviceColorspace.CMYK, f4id)
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
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        params = capypdf.ImagePdfProperties()
        with capypdf.Generator(ofilename, dprops) as gen:
            bgimage_ri = gen.load_image(image_dir / 'flame_gradient.png')
            bgimage = gen.add_image(bgimage_ri, params)
            fgimage_ri = gen.load_image(image_dir / 'object_gradient.png')
            fgimage = gen.add_image(fgimage_ri, params)
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
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as gen:
            font = gen.load_font(noto_fontdir / 'NotoSerif-Regular.ttf')
            # Repeating pattern.
            pctx = gen.create_tiling_pattern_context(0, 0, 10, 10)
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
            patternid = gen.add_tiling_pattern(pctx)
            # Text
            textctx = gen.create_tiling_pattern_context(0, 0, 3, 4)
            textctx.render_text("g", font, 3, 0, 2)
            textpatternid = gen.add_tiling_pattern(textctx)

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

    @validate_image('python_annotate', 400, 100)
    def test_annotate(self, ofilename, w, h):
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as gen:
            ta = capypdf.Annotation.new_text_annotation('This is a text ännotation.')
            ta.set_rectangle(30, 80, 40, 90)
            taid = gen.add_annotation(ta)
            fid = gen.load_font(noto_fontdir / 'NotoSans-Regular.ttf')
            embid = gen.embed_file(image_dir / '../readme.md')
            emba = capypdf.Annotation.new_file_attachment_annotation(embid)
            emba.set_rectangle(30, 50, 40, 60)
            embid = gen.add_annotation(emba)
            with gen.page_draw_context() as ctx:
                ctx.annotate(taid)
                ctx.render_text("<- This is a text annotation", fid, 11, 50, 80)
                ctx.annotate(embid)
                ctx.render_text("<- This is a file attachment annotation", fid, 11, 50, 50)

    @cleanup('python_annotate_link')
    def test_annotate_link(self, ofilename, w=200, h=200):
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)

        with capypdf.Generator(ofilename, dprops) as gen:
            forward_anno = capypdf.Annotation.new_link_annotation()
            forward_dest = capypdf.Destination()
            forward_dest.set_page_fit(1)
            forward_anno.set_destination(forward_dest)
            forward_anno.set_rectangle(10, 10, 20, 20)
            forward_id = gen.add_annotation(forward_anno)

            backward_anno = capypdf.Annotation.new_link_annotation()
            backward_dest = capypdf.Destination()
            backward_dest.set_page_fit(0)
            backward_anno.set_destination(backward_dest)
            backward_anno.set_rectangle(10, 10, 20, 20)
            backward_id = gen.add_annotation(backward_anno)

            with gen.page_draw_context() as ctx:
                ctx.cmd_rg(1, 0, 0)
                ctx.cmd_re(10, 10, 20, 20)
                ctx.cmd_f()
                ctx.annotate(forward_id)

            with gen.page_draw_context() as ctx:
                ctx.cmd_rg(0, 0, 1)
                ctx.cmd_re(10, 10, 20, 20)
                ctx.cmd_f()
                ctx.annotate(backward_id)

    @validate_image('python_tagged', 200, 200)
    def test_tagged(self, ofilename, w, h):
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        dprops.set_tagged(True)
        with capypdf.Generator(ofilename, dprops) as gen:
            fid = gen.load_font(noto_fontdir / 'NotoSerif-Regular.ttf')
            bfid = gen.load_font(noto_fontdir / 'NotoSans-Bold.ttf')
            title = 'H1 element'
            title_extra = capypdf.StructItemExtraData()
            title_extra.set_t('Main title')
            title_extra.set_lang('fi-FI')
            title_extra.set_alt('Alt text for H1')
            title_extra.set_actual_text('Actual text for H1')
            doc_id = gen.add_structure_item(capypdf.StructureType.Document, extra=title_extra)

            with gen.page_draw_context() as ctx:
                tw = gen.text_width(title,bfid, 14)
                title_id = gen.add_structure_item(capypdf.StructureType.H1, doc_id)
                with ctx.cmd_BDC_builtin(title_id):
                    ctx.render_text(title, bfid, 14, 100-tw/2, 170)
                with ctx.cmd_BMC('Artifact'):
                    ctx.cmd_w(1.5)
                    ctx.cmd_m(50, 155)
                    ctx.cmd_l(150, 155)
                    ctx.cmd_S()
                with ctx.cmd_BDC_builtin(gen.add_structure_item(capypdf.StructureType.P, doc_id)):
                    with ctx.text_new() as t:
                        t.cmd_Td(20, 130)
                        t.cmd_Tf(fid, 8)
                        t.cmd_TL(10)
                        t.render_text('Text object inside marked content')
                with ctx.text_new() as t:
                    t.cmd_Td(20, 100)
                    t.cmd_Tf(fid, 8)
                    t.cmd_TL(10)
                    with t.cmd_BDC_builtin(gen.add_structure_item(capypdf.StructureType.P, doc_id)):
                        t.render_text('Marked content inside text object.')

    @validate_image('python_customroles', 200, 200)
    def test_customroles(self, ofilename, w, h):
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        dprops.set_tagged(True)
        with capypdf.Generator(ofilename, dprops) as gen:
            fid = gen.load_font(noto_fontdir / 'NotoSerif-Regular.ttf')
            bfid = gen.load_font(noto_fontdir / 'NotoSans-Bold.ttf')
            title = 'Headline text'
            head_role = gen.add_rolemap_entry("Headline", capypdf.StructureType.H1)
            text_role = gen.add_rolemap_entry("Text body", capypdf.StructureType.P)
            doc_id = gen.add_structure_item(capypdf.StructureType.Document)

            with gen.page_draw_context() as ctx:
                tw = gen.text_width(title,bfid, 14)
                title_id = gen.add_structure_item(head_role, doc_id)
                with ctx.cmd_BDC_builtin(title_id):
                    ctx.render_text(title, bfid, 14, 100-tw/2, 170)
                with ctx.cmd_BDC_builtin(gen.add_structure_item(text_role, doc_id)):
                    with ctx.text_new() as t:
                        t.cmd_Td(20, 130)
                        t.cmd_Tf(fid, 8)
                        t.cmd_TL(10)
                        t.render_text('Text object inside custom role mark.')
                with ctx.text_new() as t:
                    t.cmd_Td(20, 100)
                    t.cmd_Tf(fid, 8)
                    t.cmd_TL(10)
                    with t.cmd_BDC_builtin(gen.add_structure_item(text_role, doc_id)):
                        t.render_text('Custom role mark inside text object.')


    @validate_image('python_printersmark', 200, 200)
    def test_printersmark(self, ofilename, w, h):
        cropmark_size = 5
        bleed_size = 2*cropmark_size
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        pprops.set_pagebox(capypdf.PageBox.Trim, bleed_size, bleed_size, w - 2*bleed_size, h - 2*bleed_size)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        with capypdf.Generator(ofilename, dprops) as gen:
            vctx = capypdf.FormXObjectDrawContext(gen, 0, 0, 1, cropmark_size)
            vctx.cmd_re(0, 0, 1, cropmark_size)
            vctx.cmd_f()
            vid = gen.add_form_xobject(vctx)
            del vctx
            hctx = capypdf.FormXObjectDrawContext(gen, 0, 0, cropmark_size, 1)
            hctx.cmd_re(0, 0, cropmark_size, 1)
            hctx.cmd_f()
            hid = gen.add_form_xobject(hctx)
            del hctx
            with gen.page_draw_context() as ctx:
                ctx.cmd_rg(0.9, 0.1, 0.1)
                ctx.cmd_re(bleed_size, bleed_size, w-2*bleed_size, h-2*bleed_size)
                ctx.cmd_f()

                # Vertical annotations
                a = capypdf.Annotation.new_printers_mark_annotation(vid)
                a.set_rectangle(bleed_size-0.5, 0, bleed_size+.5, cropmark_size)
                a.set_flags(capypdf.AnnotationFlag.Print | capypdf.AnnotationFlag.ReadOnly)
                aid = gen.add_annotation(a)
                ctx.annotate(aid)
                a = capypdf.Annotation.new_printers_mark_annotation(vid)
                a.set_rectangle(bleed_size-0.5, h-cropmark_size, bleed_size+.5, h)
                a.set_flags(capypdf.AnnotationFlag.Print | capypdf.AnnotationFlag.ReadOnly)
                aid = gen.add_annotation(a)
                ctx.annotate(aid)

                # Horizontal annotations
                a = capypdf.Annotation.new_printers_mark_annotation(hid)
                a.set_rectangle(0, bleed_size - 0.5, cropmark_size, bleed_size+.5)
                a.set_flags(capypdf.AnnotationFlag.Print | capypdf.AnnotationFlag.ReadOnly)
                aid = gen.add_annotation(a)
                ctx.annotate(aid)
                a = capypdf.Annotation.new_printers_mark_annotation(hid)
                a.set_rectangle(0, h - bleed_size - 0.5, cropmark_size, h - bleed_size+.5)
                a.set_flags(capypdf.AnnotationFlag.Print | capypdf.AnnotationFlag.ReadOnly)
                aid = gen.add_annotation(a)
                ctx.annotate(aid)

                # The other corners would go here, but I'm lazy.

    @validate_image('python_transparency_groups', 200, 200)
    def test_transparency_groups(self, ofilename, w, h):
        pprops = capypdf.PageProperties()
        tr_props = capypdf.TransparencyGroupProperties()
        tr_props.set_CS(capypdf.DeviceColorspace.RGB)
        pprops.set_transparency_group_properties(tr_props)
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)

        with capypdf.Generator(ofilename, dprops) as gen:
            # Test transparency painting within transparency group
            gstate = capypdf.GraphicsState()
            gstate.set_CA(0.5)
            gstate.set_ca(0.9)
            gsid = gen.add_graphics_state(gstate)

            # This is the actual transparency for the whole group
            pstate = capypdf.GraphicsState()
            pstate.set_ca(0.5)
            psid = gen.add_graphics_state(pstate)

            with gen.page_draw_context() as page:
                ctx = capypdf.TransparencyGroupDrawContext(gen, 0, 0, 200, 200)
                ctx.set_transparency_group_properties(tr_props)

                # Transformation tests to locality of the transparency group's drawing context
                ctx.set_group_matrix(1, 0, 0, 1, -100, -100)

                ctx.cmd_gs(gsid)
                ctx.cmd_w(5)
                ctx.cmd_rg(0, 0, 0)
                ctx.cmd_RG(1, 0, 0)
                ctx.cmd_re(15, 15, 175, 175)
                ctx.cmd_b() # paint both
                tid = gen.add_transparency_group(ctx)
                del ctx

                page.cmd_cm(1, 0, 0, 1, 100, 100)
                page.cmd_gs(psid)
                page.cmd_Do(tid)

    @validate_image('python_pdfx3', 200, 200)
    def test_pdfx3(self, ofilename, w, h):
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        pprops.set_pagebox(capypdf.PageBox.Trim, 1, 1, 198, 198)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)
        dprops.set_colorspace(capypdf.DeviceColorspace.CMYK)
        dprops.set_output_intent('Uncoated Fogra 29')
        dprops.set_pdfx(capypdf.PdfXType.X3_2003)
        dprops.set_device_profile(capypdf.DeviceColorspace.CMYK, icc_dir / 'FOGRA29L.icc')
        dprops.set_title('PDF X3 test')
        with capypdf.Generator(ofilename, dprops) as gen:
            fid = gen.load_font(noto_fontdir / 'NotoSerif-Regular.ttf')
            with gen.page_draw_context() as ctx:
                ctx.render_text('This document should validate as PDF/X3.', fid, 8, 10, 180)
                ctx.render_text('The image was converted from sRGB to DeviceCMYK on load.', fid, 6, 10, 120)
                params = capypdf.ImagePdfProperties()
                rgb_image = gen.load_image(image_dir / 'flame_gradient.png')
                self.assertEqual(rgb_image.get_colorspace(), capypdf.ImageColorspace.RGB)
                self.assertFalse(rgb_image.has_profile())
                cmyk_image = gen.convert_image(rgb_image,
                    capypdf.DeviceColorspace.CMYK,
                    capypdf.RenderingIntent.RelativeColorimetric)
                self.assertEqual(cmyk_image.get_colorspace(), capypdf.ImageColorspace.CMYK)
                image = gen.add_image(cmyk_image, params)
                with ctx.push_gstate():
                    ctx.translate(75, 50)
                    ctx.scale(50, 50)
                    ctx.draw_image(image)


    @validate_image('python_shading_transparency', 200, 200)
    def test_shading_transparency(self, ofilename, w, h):
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)

        with capypdf.Generator(ofilename, dprops) as gen:
            # Fill gradient + opacity gradient
            c1 = capypdf.Color()
            o1 = capypdf.Color()
            c2 = capypdf.Color()
            o2 = capypdf.Color()
            c1.set_rgb(0, 0, 1)
            o1.set_gray(1);
            c2.set_rgb(1, 0, 0)
            o2.set_gray(0);
            fc = capypdf.Type2Function([0, 1], c1, c2, 1)
            fo = capypdf.Type2Function([0, 1], o1, o2, 1)
            fcid = gen.add_function(fc)
            foid = gen.add_function(fo)
            shc = capypdf.Type2Shading(capypdf.DeviceColorspace.RGB,
                                       0,
                                       0,
                                       200,
                                       200,
                                       fcid)
            sho = capypdf.Type2Shading(capypdf.DeviceColorspace.Gray,
                                       0,
                                       0,
                                       200,
                                       200,
                                       foid)
            shcid = gen.add_shading(shc)
            shoid = gen.add_shading(sho)
            pc = capypdf.ShadingPattern(shcid)
            po = capypdf.ShadingPattern(shoid)
            pcid = gen.add_shading_pattern(pc)
            poid = gen.add_shading_pattern(po)

            # Paint a soft mask for the alpha channel
            ctx = capypdf.TransparencyGroupDrawContext(gen, 0, 0, 200, 200)
            fillpattern = capypdf.Color()
            fillpattern.set_pattern(poid)
            ctx.set_nonstroke(fillpattern)
            ctx.cmd_re(0, 0, 200, 200)
            ctx.cmd_f()
            tr_props = capypdf.TransparencyGroupProperties()
            tr_props.set_CS(capypdf.DeviceColorspace.Gray)
            tr_props.set_I(True);
            tr_props.set_K(False);
            ctx.set_transparency_group_properties(tr_props)
            tid = gen.add_transparency_group(ctx)

            sm = capypdf.SoftMask(capypdf.SoftMaskSubType.Luminosity, tid)
            smid = gen.add_soft_mask(sm)

            gstate = capypdf.GraphicsState()
            gstate.set_SMask(smid)
            gsid = gen.add_graphics_state(gstate)

            with gen.page_draw_context() as ctx:
                ctx.cmd_gs(gsid)
                fillpattern = capypdf.Color()
                fillpattern.set_pattern(pcid)
                ctx.set_nonstroke(fillpattern)
                ctx.cmd_re(0, 0, 200, 200)
                ctx.cmd_f()

    @validate_image('python_shading_pattern', 200, 200)
    def test_shading_pattern(self, ofilename, w, h):
        pprops = capypdf.PageProperties()
        pprops.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
        dprops = capypdf.DocumentProperties()
        dprops.set_default_page_properties(pprops)

        with capypdf.Generator(ofilename, dprops) as gen:
            # Fill gradient
            c1 = capypdf.Color()
            c2 = capypdf.Color()
            c1.set_rgb(0, 0, 1)
            c2.set_rgb(1, 0, 0)
            f1 = capypdf.Type2Function([0, 1], c1, c2, 1)
            f1id = gen.add_function(f1)
            sh1 = capypdf.Type2Shading(capypdf.DeviceColorspace.RGB,
                                       312.925537,
                                       484.552765,
                                       498.24588,
                                       661.916748,
                                       f1id)
            sh1.set_extend(True, True)
            sh1id = gen.add_shading(sh1)
            p1 = capypdf.ShadingPattern(sh1id)
            p1.set_matrix(0.752683, 0, 0, -0.752683, -207.66943, 529.072279)
            p1id = gen.add_shading_pattern(p1)

            # Stroke gradient
            c1.set_rgb(1, 0.00392157, 1)
            c2.set_rgb(0.262745, 0.996078, 0)
            f2 = capypdf.Type2Function([0, 1], c1, c2, 1)
            f2id = gen.add_function(f2)
            sh2 = capypdf.Type2Shading(capypdf.DeviceColorspace.RGB,
                                       302.362183,
                                       572.714111,
                                       509.850525,
                                       572.714111,
                                       f2id)
            sh2.set_extend(True, True)
            sh2id = gen.add_shading(sh2)
            p2 = capypdf.ShadingPattern(sh2id)
            p2.set_matrix(0.752683, 0, 0, -0.752683, -207.66943, 529.072279)
            p2id = gen.add_shading_pattern(p2)
            with gen.page_draw_context() as ctx:
                ctx.cmd_w(40)
                fillpattern = capypdf.Color()
                fillpattern.set_pattern(p1id)
                ctx.set_nonstroke(fillpattern)
                strokepattern = capypdf.Color()
                strokepattern.set_pattern(p2id)
                ctx.set_stroke(strokepattern)
                ctx.cmd_j(capypdf.LineJoinStyle.Round)
                ctx.cmd_re(20, 20, 160, 160)
                ctx.cmd_B()

if __name__ == "__main__":
    unittest.main()
