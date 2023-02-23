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
import os, sys, pathlib

os.environ['LD_LIBRARY_PATH'] = 'src'
sys.path.append(sys.argv[1])

sys.argv = sys.argv[0:1] + sys.argv[2:]

import a4pdf

class TestPDFCreation(unittest.TestCase):

    def test_simple(self):
        ofile = pathlib.Path('python_test.pdf')
        try:
            ofile.unlink()
        except FileNotFoundError:
            pass
        self.assertFalse(ofile.exists())
        o = a4pdf.Options()
        g = a4pdf.Generator(ofile, o)
        o = None
        with g.page_draw_context() as ctx:
            ctx.set_rgb_nonstroke(1.0, 0.0, 0.0)
            ctx.cmd_re(10, 10, 100, 100)
            ctx.cmd_f()
        g = None
        self.assertTrue(ofile.exists())
        ofile.unlink()

if __name__ == "__main__":
    unittest.main()
