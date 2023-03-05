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

os.environ['LD_LIBRARY_PATH'] = 'src'
source_root = pathlib.Path(sys.argv[1])
testdata_dir = source_root / 'testoutput'
image_dir = source_root / 'images'
sys.path.append(str(source_root / 'python'))

sys.argv = sys.argv[0:1] + sys.argv[2:]

import a4pdf

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

class TestPDFCreation(unittest.TestCase):

    @validate_image('python_simple', 480, 640)
    def test_simple(self, ofilename, w, h):
        ofile = pathlib.Path(ofilename)
        with a4pdf.Generator(ofile) as g:
            with g.page_draw_context() as ctx:
                ctx.cmd_rg(1.0, 0.0, 0.0)
                ctx.cmd_re(10, 10, 100, 100)
                ctx.cmd_f()

    @validate_image('python_text', 400, 400)
    def test_text(self, ofilename, w, h):
        opts = a4pdf.Options()
        opts.set_mediabox(0, 0, w, h)
        with a4pdf.Generator(ofilename, opts) as g:
            fid = g.load_font('/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf')
            with g.page_draw_context() as ctx:
                ctx.render_text('Av, Tv, kerning yo.', fid, 12, 50, 150)

    def test_error(self):
        ofile = pathlib.Path('delme.pdf')
        if ofile.exists():
            ofile.unlink()
        with self.assertRaises(a4pdf.A4PDFException) as cm_outer:
            with a4pdf.Generator(ofile) as g:
                ctx = g.page_draw_context()
                with self.assertRaises(a4pdf.A4PDFException) as cm:
                    ctx.cmd_w(-0.1)
                self.assertEqual(str(cm.exception), 'Negative line width.')
                ctx = None # Destroy without adding page, so there should be no output.
        self.assertEqual(str(cm_outer.exception), 'No pages defined.')
        self.assertFalse(ofile.exists())

    def test_line_drawing(self):
        ofile = pathlib.Path('nope.pdf')
        with a4pdf.Generator(ofile) as g:
            with g.page_draw_context() as ctx:
                ctx.cmd_J(a4pdf.LineCapStyle.Round)
                ctx.cmd_j(a4pdf.LineJoinStyle.Bevel)
        ofile.unlink()

    @validate_image('python_image', 200, 200)
    def test_images(self, ofilename, w, h):
        opts = a4pdf.Options()
        opts.set_mediabox(0, 0, w, h)
        with a4pdf.Generator(ofilename, opts) as g:
            bg_img = g.embed_jpg(image_dir / 'simple.jpg')
            mono_img = g.load_image(image_dir / '1bit_noalpha.png')
            gray_img = g.load_image(image_dir / 'gray_alpha.png')
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

    @validate_image('python_path', 200, 200)
    def test_path(self, ofilename, w, h):
        opts = a4pdf.Options()
        opts.set_mediabox(0, 0, w, h)
        with a4pdf.Generator(ofilename, opts) as g:
            with g.page_draw_context() as ctx:
                with ctx.push_gstate():
                    ctx.cmd_w(5)
                    ctx.cmd_J(a4pdf.LineCapStyle.Round)
                    ctx.cmd_m(10, 10);
                    ctx.cmd_c(80, 10, 20, 90, 90, 90);
                    ctx.cmd_S();
                with ctx.push_gstate():
                    ctx.cmd_w(10)
                    ctx.translate(100, 0)
                    ctx.cmd_RG(1.0, 0.0, 0.0)
                    ctx.cmd_rg(0.9, 0.9, 0.0)
                    ctx.cmd_j(a4pdf.LineJoinStyle.Bevel)
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
                    ctx.cmd_j(a4pdf.LineJoinStyle.Round)
                    ctx.cmd_B()
                with ctx.push_gstate():
                    ctx.translate(100, 100)
                    ctx.cmd_w(2)
                    ctx.cmd_rg(0, 1, 0);
                    ctx.cmd_RG(0.5, 0.1, 0.5)
                    draw_intersect_shape(ctx)
                    ctx.cmd_Bstar()

if __name__ == "__main__":
    unittest.main()
