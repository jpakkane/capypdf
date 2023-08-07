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
import os
import math
from enum import Enum

class LineCapStyle(Enum):
    Butt = 0
    Round = 1
    Projection_Square = 2

class LineJoinStyle(Enum):
    Miter = 0
    Round = 1
    Bevel = 2

class Colorspace(Enum):
    DeviceRGB = 0
    DeviceGray = 1
    DeviceCMYK = 2

class TransitionType(Enum):
    Split = 0
    Blinds = 1
    Box = 2
    Wipe = 3
    Dissolve = 4
    Glitter = 5
    R = 6
    Fly = 7
    Push = 8
    Cover = 9
    Uncover = 10
    Fade = 11

class PageBox(Enum):
    Media = 0
    Crop = 1
    Bleed = 2
    Trim = 3
    Art = 4

class RenderingIntent(Enum):
    Relative_Colorimetric = 0
    ABSOLUTE_Colorimetric = 1
    Saturation = 2
    Perceptual = 3

class TextMode(Enum):
    Fill = 0,
    Stroke = 1
    FillStroke = 2
    Invisible = 3
    FillClip = 4
    StrokeClip = 5
    FillStrokeClip = 6
    Clip = 7

class IntentSubtype(Enum):
    PDFX = 0
    PDFA = 1
    # PDFE = 2

class CapyPDFException(Exception):
    def __init__(*args, **kwargs):
        Exception.__init__(*args, **kwargs)

ec_type = ctypes.c_int32
enum_type = ctypes.c_int32

class FontId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class ImageId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class IccColorSpaceId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class OptionalContentGroupId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]


cfunc_types = (

('capy_options_new', [ctypes.c_void_p]),
('capy_options_destroy', [ctypes.c_void_p]),
('capy_options_set_colorspace', [ctypes.c_void_p, enum_type]),
('capy_options_set_device_profile', [ctypes.c_void_p, enum_type, ctypes.c_char_p]),
('capy_options_set_title', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_options_set_author', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_options_set_pagebox',
    [ctypes.c_void_p, enum_type, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_options_set_output_intent', [ctypes.c_void_p, enum_type, ctypes.c_char_p]),

('capy_generator_new', [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_page', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_embed_jpg', [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p]),
('capy_generator_load_icc_profile', [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p]),
('capy_generator_load_font', [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p]),
('capy_generator_write', [ctypes.c_void_p]),
('capy_generator_add_optional_content_group', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_destroy', [ctypes.c_void_p]),
('capy_generator_text_width', [ctypes.c_void_p, ctypes.c_char_p, FontId, ctypes.c_double, ctypes.POINTER(ctypes.c_double)]),

('capy_page_draw_context_new', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_dc_add_simple_navigation', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p]),
('capy_dc_cmd_b', [ctypes.c_void_p]),
('capy_dc_cmd_B', [ctypes.c_void_p]),
('capy_dc_cmd_bstar', [ctypes.c_void_p]),
('capy_dc_cmd_Bstar', [ctypes.c_void_p]),
('capy_dc_cmd_BDC_ocg', [ctypes.c_void_p, OptionalContentGroupId]),
('capy_dc_cmd_c', [ctypes.c_void_p,
    ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_cm', [ctypes.c_void_p,
    ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_EMC', [ctypes.c_void_p]),
('capy_dc_cmd_f', [ctypes.c_void_p]),
('capy_dc_cmd_fstar', [ctypes.c_void_p]),
('capy_dc_cmd_G', [ctypes.c_void_p, ctypes.c_double]),
('capy_dc_cmd_g', [ctypes.c_void_p, ctypes.c_double]),
('capy_dc_cmd_h', [ctypes.c_void_p]),
('capy_dc_cmd_i', [ctypes.c_void_p, ctypes.c_double]),
('capy_dc_cmd_J', [ctypes.c_void_p, enum_type]),
('capy_dc_cmd_j', [ctypes.c_void_p, enum_type]),
('capy_dc_cmd_k', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_K', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_l', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_m', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_M', [ctypes.c_void_p, ctypes.c_double]),
('capy_dc_cmd_n', [ctypes.c_void_p]),
('capy_dc_cmd_q', [ctypes.c_void_p]),
('capy_dc_cmd_Q', [ctypes.c_void_p]),
('capy_dc_cmd_RG', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_rg', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_re', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_ri', [ctypes.c_void_p, enum_type]),
('capy_dc_cmd_s', [ctypes.c_void_p]),
('capy_dc_cmd_S', [ctypes.c_void_p]),
('capy_dc_cmd_v', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_w', [ctypes.c_void_p, ctypes.c_double]),
('capy_dc_cmd_W', [ctypes.c_void_p]),
('capy_dc_cmd_Wstar', [ctypes.c_void_p]),
('capy_dc_cmd_y', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),

('capy_dc_draw_image',
    [ctypes.c_void_p, ImageId]),
('capy_dc_render_text',
    [ctypes.c_void_p, ctypes.c_char_p, FontId, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_render_text_obj',
    [ctypes.c_void_p, ctypes.c_void_p]),
('capy_dc_set_nonstroke', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_dc_destroy', [ctypes.c_void_p]),

('capy_text_new', [ctypes.c_void_p]),
('capy_text_destroy', [ctypes.c_void_p]),
('capy_text_new', [ctypes.c_void_p]),
('capy_text_cmd_Tc', [ctypes.c_void_p, ctypes.c_double]),
('capy_text_cmd_Td', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double]),
('capy_text_cmd_Tf', [ctypes.c_void_p, FontId, ctypes.c_double]),
('capy_text_cmd_TL', [ctypes.c_void_p, ctypes.c_double]),
('capy_text_cmd_Tr', [ctypes.c_void_p, enum_type]),
('capy_text_cmd_Tw', [ctypes.c_void_p, ctypes.c_double]),
('capy_text_cmd_Tstar', [ctypes.c_void_p]),
('capy_text_render_text', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_text_stroke_color', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_text_nonstroke_color', [ctypes.c_void_p, ctypes.c_void_p]),

('capy_color_new', [ctypes.c_void_p]),
('capy_color_destroy', [ctypes.c_void_p]),
('capy_color_set_rgb', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_color_set_gray', [ctypes.c_void_p, ctypes.c_double]),
('capy_color_set_cmyk', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_color_set_icc', [ctypes.c_void_p, IccColorSpaceId, ctypes.POINTER(ctypes.c_double), ctypes.c_int32]),

('capy_transition_new', [ctypes.c_void_p, enum_type, ctypes.c_double]),
('capy_transition_destroy', [ctypes.c_void_p]),

('capy_optional_content_group_new', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_optional_content_group_destroy', [ctypes.c_void_p]),

)

if os.name == 'nt':
    libfile_name = 'capypdf-0.dll'
else:
    libfile_name = 'libcapypdf.so'
libfile = None

if 'CAPYPDF_SO_OVERRIDE' in os.environ:
    libfile = ctypes.cdll.LoadLibrary(os.path.join(os.environ['CAPYPDF_SO_OVERRIDE'], libfile_name))

if libfile is None:
    try:
        ctypes.cdll.LoadLibrary(libfile_name)
    except FileNotFoundError:
        pass

if libfile is None:
    # Most likely a wheel installation with embedded libs.
    from glob import glob
    if 'site-packages' in __file__:
        sdir = os.path.split(__file__)[0]
        matches = glob(os.path.join(sdir, '.*capypdf*', libfile_name))
        if len(matches) == 1:
            libfile = ctypes.cdll.LoadLibrary(matches[0])

if libfile is None:
    raise CapyPDFException('Could not locate shared library.')

for funcname, argtypes in cfunc_types:
    funcobj = getattr(libfile, funcname)
    if argtypes is not None:
        funcobj.argtypes = argtypes
    funcobj.restype = ec_type

# This is the only function in the public API not to return an errorcode.
libfile.capy_error_message.argtypes = [ctypes.c_int32]
libfile.capy_error_message.restype = ctypes.c_char_p

def get_error_message(errorcode):
    return libfile.capy_error_message(errorcode).decode('UTF-8', errors='ignore')

def raise_with_error(errorcode):
    raise CapyPDFException(get_error_message(errorcode))

def check_error(errorcode):
    if errorcode != 0:
        raise_with_error(errorcode)

def to_bytepath(filename):
    if isinstance(filename, bytes):
        return filename
    elif isinstance(filename, str):
        return filename.encode('UTF-8')
    else:
        return str(filename).encode('UTF-8')

class Options:
    def __init__(self):
        opt = ctypes.c_void_p()
        check_error(libfile.capy_options_new(ctypes.pointer(opt)))
        self._as_parameter_ = opt

    def __del__(self):
        check_error(libfile.capy_options_destroy(self))

    def set_colorspace(self, cs):
        if not isinstance(cs, Colorspace):
            raise CapyPDFException('Argument not a colorspace object.')
        check_error(libfile.capy_options_set_colorspace(self, cs.value))

    def set_title(self, title):
        if not isinstance(title, str):
            raise CapyPDFException('Title must be an Unicode string.')
        bytes = title.encode('UTF-8')
        check_error(libfile.capy_options_set_title(self, bytes))

    def set_author(self, title):
        if not isinstance(title, str):
            raise CapyPDFException('Author must be an Unicode string.')
        bytes = title.encode('UTF-8')
        check_error(libfile.capy_options_set_author(self, bytes))

    def set_pagebox(self, boxtype, x1, y1, x2, y2):
        check_error(libfile.capy_options_set_pagebox(self, boxtype.value, x1, y1, x2, y2))

    def set_device_profile(self, colorspace, path):
        check_error(libfile.capy_options_set_device_profile(self, colorspace.value, to_bytepath(path)))

    def set_output_intent(self, stype, identifier):
        if not isinstance(stype, IntentSubtype):
            raise CapyPDFException('Argument must be an intent subtype.')
        check_error(libfile.capy_options_set_output_intent(self, stype.value, identifier.encode('utf-8')))


class DrawContext:
    def __init__(self, generator):
        dcptr = ctypes.c_void_p()
        check_error(libfile.capy_page_draw_context_new(generator, ctypes.pointer(dcptr)))
        self._as_parameter_ = dcptr
        self.generator = generator

    def __del__(self):
        check_error(libfile.capy_dc_destroy(self))

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        try:
            self.generator.add_page(self)
        finally:
            self.generator = None # Not very elegant.

    def push_gstate(self):
        return StateContextManager(self)

    def cmd_b(self):
        check_error(libfile.capy_dc_cmd_b(self))

    def cmd_B(self):
        check_error(libfile.capy_dc_cmd_B(self))

    def cmd_bstar(self):
        check_error(libfile.capy_dc_cmd_bstar(self))

    def cmd_Bstar(self):
        check_error(libfile.capy_dc_cmd_Bstar(self))

    def cmd_BDC(self, ocg):
        if not isinstance(ocg, OptionalContentGroupId):
            raise CapyPDFException('Argument must be an optional content group ID.')
        check_error(libfile.capy_dc_cmd_BDC_ocg(self, ocg))

    def cmd_c(self, x1, y1, x2, y2, x3, y3):
        check_error(libfile.capy_dc_cmd_c(self, x1, y1, x2, y2, x3, y3))

    def cmd_cm(self, m1, m2, m3, m4, m5, m6):
        check_error(libfile.capy_dc_cmd_cm(self, m1, m2, m3, m4, m5, m6))

    def cmd_EMC(self):
        check_error(libfile.capy_dc_cmd_EMC(self))

    def cmd_f(self):
        check_error(libfile.capy_dc_cmd_f(self))

    def cmd_fstar(self):
        check_error(libfile.capy_dc_cmd_fstar(self))

    def cmd_G(self, gray):
        check_error(libfile.capy_dc_cmd_G(self, gray))

    def cmd_g(self):
        check_error(libfile.capy_dc_cmd_g(self, gray))

    def cmd_h(self):
        check_error(libfile.capy_dc_cmd_h(self))

    def cmd_i(self, flatness):
        check_error(libfile.capy_dc_cmd_i(self, flatness))

    def cmd_J(self, cap_style):
        check_error(libfile.capy_dc_cmd_J(self, cap_style.value))

    def cmd_j(self, join_style):
        check_error(libfile.capy_dc_cmd_j(self, join_style.value))

    def cmd_k(self, c, m, y, k):
        check_error(libfile.capy_dc_cmd_k(self, c, m, y, k))

    def cmd_K(self, c, m, y, k):
        check_error(libfile.capy_dc_cmd_K(self, c, m, y, k))

    def cmd_l(self, x, y):
        check_error(libfile.capy_dc_cmd_l(self, x, y))

    def cmd_m(self, x, y):
        check_error(libfile.capy_dc_cmd_m(self, x, y))

    def cmd_M(self, miterlimit):
        check_error(libfile.capy_dc_cmd_M(self, miterlimit))

    def cmd_n(self):
        check_error(libfile.capy_dc_cmd_n(self))

    def cmd_q(self):
        check_error(libfile.capy_dc_cmd_q(self))

    def cmd_Q(self):
        check_error(libfile.capy_dc_cmd_Q(self))

    def cmd_re(self, x, y, w, h):
        check_error(libfile.capy_dc_cmd_re(self, x, y, w, h))

    def cmd_RG(self, r, g, b):
        check_error(libfile.capy_dc_cmd_RG(self, r, g, b))

    def cmd_rg(self, r, g, b):
        check_error(libfile.capy_dc_cmd_rg(self, r, g, b))

    def cmd_ri(self, ri):
        if not isinstance(ri, RenderingIntent):
            raise CapyPDFException('Argument must be a RenderingIntent.')
        check_error(libfile.capy_dc_cmd_ri(self, ri.value))

    def cmd_s(self):
        check_error(libfile.capy_dc_cmd_s(self))

    def cmd_S(self):
        check_error(libfile.capy_dc_cmd_S(self))

    def cmd_v(self, x2, y2, x3, y3):
        check_error(libfile.capy_dc_cmd_v(self, x2, y2, x3, y3))

    def cmd_w(self, line_width):
        check_error(libfile.capy_dc_cmd_w(self, line_width))

    def cmd_W(self):
        check_error(libfile.capy_dc_cmd_W(self))

    def cmd_Wstar(self):
        check_error(libfile.capy_dc_cmd_Wstar(self))

    def cmd_y(self, x1, y1, x3, y3):
        check_error(libfile.capy_dc_cmd_y(self, x1, y1, x3, y3))

    def set_stroke(self, color):
        if not isinstance(color, Color):
            raise CapyPDFException('Argument must be a Color object.')
        check_error(libfile.capy_dc_set_stroke(self, color))

    def set_nonstroke(self, color):
        if not isinstance(color, Color):
            raise CapyPDFException('Argument must be a Color object.')
        check_error(libfile.capy_dc_set_nonstroke(self, color))

    def render_text(self, text, fid, point_size, x, y):
        if not isinstance(text, str):
            raise CapyPDFException('Text to render is not a string.')
        if not isinstance(fid, FontId):
            raise CapyPDFException('Font id argument is not a font id object.')
        text_bytes = text.encode('UTF-8')
        check_error(libfile.capy_dc_render_text(self, text_bytes, fid, point_size, x, y))

    def render_text_obj(self, tobj):
        check_error(libfile.capy_dc_render_text_obj(self, tobj))

    def draw_image(self, iid):
        if not isinstance(iid, ImageId):
            raise CapyPDFException('Image id argument is not an image id object.')
        check_error(libfile.capy_dc_draw_image(self, iid))

    def set_page_transition(self, tr):
        if not isinstance(tr, Transition):
            raise CapyPDFException('Argument is not a transition object.')
        check_error(libfile.capy_dc_set_page_transition(self, tr))

    def translate(self, xtran, ytran):
        self.cmd_cm(1.0, 0, 0, 1.0, xtran, ytran)

    def scale(self, xscale, yscale):
        self.cmd_cm(xscale, 0, 0, yscale, 0, 0)

    def rotate(self, angle):
        self.cmd_cm(math.cos(angle), math.sin(angle), -math.sin(angle), math.cos(angle), 0.0, 0.0)

    def add_simple_navigation(self, ocgs, transition=None):
        arraytype = len(ocgs)*OptionalContentGroupId
        arr = arraytype(*tuple(ocgs))
        if transition is not None and not isinstance(transition, Transition):
            raise CapyPDFException('Transition argument must be a transition object.')
        check_error(libfile.capy_dc_add_simple_navigation(self,
                                                          ctypes.pointer(arr),
                                                          len(ocgs),
                                                          transition))

class StateContextManager:
    def __init__(self, ctx):
        self.ctx = ctx
        ctx.cmd_q()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        self.ctx.cmd_Q()


class Generator:
    def __init__(self, filename, options=None):
        file_name_bytes = to_bytepath(filename)
        if options is None:
            options = Options()
        gptr = ctypes.c_void_p()
        check_error(libfile.capy_generator_new(file_name_bytes, options, ctypes.pointer(gptr)))
        self._as_parameter_ = gptr

    def __del__(self):
        if self._as_parameter_ is not None:
            check_error(libfile.capy_generator_destroy(self))

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        if exc_type is None:
            self.write()
        else:
            return False

    def page_draw_context(self):
        return DrawContext(self)

    def add_page(self, page_ctx):
        check_error(libfile.capy_generator_add_page(self, page_ctx))

    def embed_jpg(self, fname):
        iid = ImageId()
        check_error(libfile.capy_generator_embed_jpg(self, to_bytepath(fname), ctypes.pointer(iid)))
        return iid

    def load_font(self, fname):
        fid = FontId()
        check_error(libfile.capy_generator_load_font(self, to_bytepath(fname), ctypes.pointer(fid)))
        return fid

    def load_icc_profile(self, fname):
        iid = IccColorSpaceId()
        check_error(libfile.capy_generator_load_icc_profile(self, to_bytepath(fname), ctypes.pointer(iid)))
        return iid

    def load_image(self, fname):
        iid = ImageId()
        check_error(libfile.capy_generator_load_image(self, to_bytepath(fname), ctypes.pointer(iid)))
        return iid

    def write(self):
        check_error(libfile.capy_generator_write(self))

    def text_width(self, text, font, pointsize):
        if not isinstance(text, str):
            raise CapyPDFException('Text must be a Unicode string.')
        if not isinstance(font, FontId):
            raise CapyPDFException('Argument not a colorspace object.')
        w = ctypes.c_double()
        bytes = text.encode('UTF-8')
        check_error(libfile.capy_generator_text_width(self, bytes, font, pointsize, ctypes.pointer(w)))
        return w.value

    def add_optional_content_group(self, ocg):
        ocgid = OptionalContentGroupId()
        check_error(libfile.capy_generator_add_optional_content_group(self, ocg, ctypes.pointer(ocgid)))
        return ocgid

class Text:
    def __init__(self):
        self._as_parameter_ = None
        opt = ctypes.c_void_p()
        check_error(libfile.capy_text_new(ctypes.pointer(opt)))
        self._as_parameter_ = opt

    def __del__(self):
        if self._as_parameter_ is not None:
            check_error(libfile.capy_text_destroy(self))

    def render_text(self, text):
        if not isinstance(text, str):
            raise CapyPDFException('Text must be a Unicode string.')
        bytes = text.encode('UTF-8')
        check_error(libfile.capy_text_render_text(self, bytes))

    def nonstroke_color(self, color):
        check_error(libfile.capy_text_nonstroke_color(self, color))

    def stroke_color(self, color):
        check_error(libfile.capy_text_stroke_color(self, color))

    def cmd_Tc(self, spacing):
        check_error(libfile.capy_text_cmd_Tc(self, spacing))

    def cmd_Td(self, x, y):
        check_error(libfile.capy_text_cmd_Td(self, x, y))

    def cmd_Tf(self, fontid, ptsize):
        if not isinstance(fontid, FontId):
            raise CapyPDFException('Font id is not a font object.')
        check_error(libfile.capy_text_cmd_Tf(self, fontid, ptsize))

    def cmd_TL(self, leading):
        check_error(libfile.capy_text_cmd_TL(self, leading))

    def cmd_Tr(self, rendtype):
        if not isinstance(rendtype, TextMode):
            raise CapyPDFException('Argument must be a text mode.')
        check_error(libfile.capy_text_cmd_Tr(self, rendtype.value))

    def cmd_Tw(self, spacing):
        check_error(libfile.capy_text_cmd_Tw(self, spacing))

    def cmd_Tstar(self):
        check_error(libfile.capy_text_cmd_Tstar(self))

class Color:
    def __init__(self):
        self._as_parameter_ = None
        opt = ctypes.c_void_p()
        check_error(libfile.capy_color_new(ctypes.pointer(opt)))
        self._as_parameter_ = opt

    def __del__(self):
        if self._as_parameter_ is not None:
            check_error(libfile.capy_color_destroy(self))

    def set_rgb(self, r, g, b):
        check_error(libfile.capy_color_set_rgb(self, r, g, b))

    def set_gray(self, r, g, b):
        check_error(libfile.capy_color_set_gray(self, r, g, b))

    def set_cmyk(self, c, m, y, k):
        check_error(libfile.capy_color_set_cmyk(self, c, m, y, k))

    def set_icc(self, icc_id, values):
        if not isinstance(values, list):
            raise CapyPDFException('Icc color value argument must be an array.')
        num_entries = len(values)
        doublearray = ctypes.c_double * num_entries
        doublevalues = doublearray(*tuple(values))
        check_error(libfile.capy_color_set_icc(self, icc_id, doublevalues, num_entries))

class Transition:
    def __init__(self, ttype, duration):
        self._as_parameter_ = None
        opt = ctypes.c_void_p()
        if not isinstance(ttype, TransitionType):
            raise CapyPDFException('Argument is not a transition type.')
        check_error(libfile.capy_transition_new(ctypes.pointer(opt), ttype.value, duration))
        self._as_parameter_ = opt

    def __del__(self):
        check_error(libfile.capy_transition_destroy(self))


class OptionalContentGroup:
    def __init__(self, name):
        self._as_parameter_ = None
        in_bytes = name.encode('ASCII')
        opt = ctypes.c_void_p()
        check_error(libfile.capy_optional_content_group_new(ctypes.pointer(opt), in_bytes))
        self._as_parameter_ = opt

    def __del__(self):
        check_error(libfile.capy_optional_content_group_destroy(self))
