#!/usr/bin/env python3

# SPDX-License-Identifier: Apache-2.0
# Copyright 2023-2024 Jussi Pakkanen


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
            if 'mesoncheckout' in str(f):
                continue
            self.assertNotIn(b'\t', f.read_bytes(), str(f.resolve()))


if __name__ == "__main__":
    unittest.main()
