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

libfile = ctypes.cdll.LoadLibrary('src/liba4pdf.so') # FIXME

libfile.pdf_options_create.restype = ctypes.c_void_p

libfile.pdf_options_destroy.argtypes = [ctypes.c_void_p]

libfile.pdf_options_set_title.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
libfile.pdf_options_set_title.restype = ctypes.c_int32

libfile.pdf_generator_create.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
libfile.pdf_generator_create.restype = ctypes.c_void_p

libfile.pdf_generator_destroy.argtypes = [ctypes.c_void_p]

libfile.pdf_generator_new_page.argtypes = [ctypes.c_void_p]

libfile.pdf_error_message.argtypes = [ctypes.c_int32]
libfile.pdf_error_message.restype = ctypes.c_char_p

def get_error_message(errorcode):
    return libfile.pdf_error_message(errorcode).decode('UTF-8')

class PdfOptions:
    def __init__(self):
        self._as_parameter_ = libfile.pdf_options_create()

    def __del__(self):
        libfile.pdf_options_destroy(self)

    def set_title(self, title):
        if not isinstance(title, str):
            raise RuntimeError('Title must be an Unicode string.')
        bytes = title.encode('UTF-8')
        rc = libfile.pdf_options_set_title(self, bytes)
        if rc != 0:
            # fixme, get error here.
            raise RuntimeError('Setting title failed.')

class PdfGenerator:
    def __init__(self, filename, options):
        self._as_parameter_ = libfile.pdf_generator_create(filename, options)

    def __del__(self):
        libfile.pdf_generator_destroy(self)

    def new_page(self):
        libfile.pdf_generator_new_page(self)

if __name__ == "__main__":
    o = PdfOptions()
    o.set_title('Python PDF')
    g = PdfGenerator(b'python.pdf', o)
