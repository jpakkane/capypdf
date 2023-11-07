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

class TestSyntax(unittest.TestCase):

    def test_sizet(self):
        headerfile = pathlib.Path('include/capypdf.h').read_text()
        self.assertTrue('size_t' not in headerfile, 'The public heaader must not use size_t, only (u)int64_t et al.')

    def test_bool(self):
        headerfile = pathlib.Path('include/capypdf.h').read_text()
        self.assertTrue('bool' not in headerfile, 'The public heaader must not use booleans, use enums instead.')

    def test_tab(self):
        source_root = pathlib.Path(__file__).parent.parent
        for f in source_root.glob('**/meson.build'):
            self.assertNotIn(b'\t', f.read_bytes())


if __name__ == "__main__":
    unittest.main()
