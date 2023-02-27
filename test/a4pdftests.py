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
            subprocess.check_call(['gs',
                                   '-q',
                                   '-dNOPAUSE',
                                   '-dBATCH',
                                   '-sDEVICE=png256',
                                   f'-g{w}x{h}',
                                   '-dPDFFitPage',
                                   f'-sOutputFile={pngname}',
                                   pdfname])
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

if __name__ == "__main__":
    unittest.main()
