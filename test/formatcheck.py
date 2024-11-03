#!/usr/bin/env python3

# SPDX-License-Identifier: Apache-2.0
# Copyright 2024 Jussi Pakkanen

import os, sys, subprocess, pathlib, shutil

class FormatChecker:
    def __init__(self, source_root):
        self.source_root = source_root
        self.unformatted = []
        self.clangformat = shutil.which('clang-format')
        if not self.clangformat:
            sys.exit('Clang-format binary not found.')

    def check(self):
        self.check_files(self.source_root.glob('*/*.cpp'))
        self.check_files(self.source_root.glob('*/*.c'))
        self.check_files(self.source_root.glob('*/*.h.in'))
        self.check_files(self.source_root.glob('*/*.h'))
        self.check_files(self.source_root.glob('*/*.hpp'))
        if self.unformatted:
            print('Unformatted files:\n')
            [print(f) for f in self.unformatted]
        sys.exit(len(self.unformatted))

    def check_files(self, filelist):
        for f in filelist:
            if not f.is_file():
                continue
            formatted = subprocess.check_output([self.clangformat,
                                                 '--style=file',
                                                 f])
            original = f.read_bytes()
            if formatted != original:
                self.unformatted.append(f)

if __name__ == '__main__':
    src_root = pathlib.Path(__file__).parent.parent
    c = FormatChecker(src_root)
    c.check()

