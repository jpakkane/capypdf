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


import ctypes

cfunc_types = (('a4pdf_options_create', None, ctypes.c_void_p),
               ('a4pdf_options_destroy', [ctypes.c_void_p], None),
               ('a4pdf_options_set_title', [ctypes.c_void_p, ctypes.c_char_p], ctypes.c_int32),

               ('a4pdf_generator_create', [ctypes.c_char_p, ctypes.c_void_p], ctypes.c_void_p),
               ('a4pdf_generator_add_page', [ctypes.c_void_p, ctypes.c_void_p], None),
               ('a4pdf_generator_destroy', [ctypes.c_void_p], None),

               ('a4pdf_page_draw_context_new', [ctypes.c_void_p], ctypes.c_void_p),
               ('a4pdf_draw_context_destroy', [ctypes.c_void_p], None),

               ('a4pdf_error_message', [ctypes.c_int32], ctypes.c_char_p),
               )

libfile = ctypes.cdll.LoadLibrary('src/liba4pdf.so') # FIXME

# Options

for funcname, argtypes, restype in cfunc_types:
    funcobj = getattr(libfile, funcname)
    if argtypes is not None:
        funcobj.argtypes = argtypes
    if restype is not None:
        funcobj.restype = restype

def get_error_message(errorcode):
    return libfile.a4pdf_error_message(errorcode).decode('UTF-8', errors='ignore')

def raise_with_error(errorcode):
    raise RuntimeError(get_error_message(errorcode))

def check_error(errorcode):
    if errorcode != 0:
        raise_with_error(errorcode)

class Options:
    def __init__(self):
        self._as_parameter_ = libfile.a4pdf_options_create()

    def __del__(self):
        libfile.a4pdf_options_destroy(self)

    def set_title(self, title):
        if not isinstance(title, str):
            raise RuntimeError('Title must be an Unicode string.')
        bytes = title.encode('UTF-8')
        rc = libfile.a4pdf_options_set_title(self, bytes)
        check_error(rc)

class DrawContext:
    def __init__(self, generator):
        self._as_parameter_ = libfile.a4pdf_page_draw_context_new(generator)

    def __del__(self):
        libfile.a4pdf_page_draw_context_destroy(self)

class Generator:
    def __init__(self, filename, options):
        if isinstance(filename, bytes):
            file_name_bytes = filename
        elif isinstance(filename, str):
            file_name_bytes = filename.encode('UTF-8')
        else:
            file_name_bytes = str(filename).encode('UTF-8')
        self._as_parameter_ = libfile.a4pdf_generator_create(file_name_bytes, options)

    def __del__(self):
        libfile.a4pdf_generator_destroy(self)

    def page_draw_context(self):
        return DrawContext(self)

    def add_page(self, page_ctx):
        libfile.a4pdf_generator_add_page(self, page_ctx)
