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
#

import ctypes

libfile = ctypes.cdll.LoadLibrary('./libmypdf.so')

libfile.pdf_options_create.restype = ctypes.c_void_p

libfile.pdf_options_destroy.argtypes = [ctypes.c_void_p]

libfile.pdf_generator_create.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
libfile.pdf_generator_create.restype = ctypes.c_void_p

libfile.pdf_generator_destroy.argtypes = [ctypes.c_void_p]

libfile.pdf_generator_new_page.argtypes = [ctypes.c_void_p]
libfile.pdf_generator_new_page.restype = ctypes.c_void_p

libfile.pdf_page_destroy.argtypes = [ctypes.c_void_p]

class PdfOptions:
    def __init__(self):
        self._as_parameter_ = libfile.pdf_options_create()

    def __del__(self):
        libfile.pdf_options_destroy(self)

class PdfPage:
    def __init__(self, handle):
        self._as_parameter_ = handle

    def __del__(self):
        libfile.pdf_page_destroy(self)


class PdfGenerator:
    def __init__(self, filename, options):
        self._as_parameter_ = libfile.pdf_generator_create(filename, options)

    def __del__(self):
        libfile.pdf_generator_destroy(self)

    def new_page(self):
        return PdfPage(libfile.pdf_generator_new_page(self))

if __name__ == "__main__":
    o = PdfOptions()
    g = PdfGenerator(b"python.pdf", o)
    g.new_page()
