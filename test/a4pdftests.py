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
testdata_dir = source_root / 'testdata'
sys.path.append(str(source_root / 'python'))

sys.argv = sys.argv[0:1] + sys.argv[2:]

import a4pdf

def validate_image(basename, w, h):
    import functools
    def decorator_validate(func):
        @functools.wraps(func)
        def wrapper_validate(*args, **kwargs):
            assert(len(args) == 1)
            utobj = args[0]
            pngname = pathlib.Path(basename + '.png')
            pdfname = pathlib.Path(basename + '.pdf')
            args = (args[0], pdfname)
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
                                              '-sDEVICE=png256',
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
    def test_simple(self, ofilename):
        ofile = pathlib.Path(ofilename)
        with a4pdf.Generator(ofile) as g:
            with g.page_draw_context() as ctx:
                ctx.set_rgb_nonstroke(1.0, 0.0, 0.0)
                ctx.cmd_re(10, 10, 100, 100)
                ctx.cmd_f()

    @validate_image('python_text', 400, 400)
    def test_text(self, ofilename):
        opts = a4pdf.Options()
        opts.set_mediabox(0, 0, 400, 400)
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

if __name__ == "__main__":
    unittest.main()
