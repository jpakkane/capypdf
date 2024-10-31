# SPDX-License-Identifier: Apache-2.0
# Copyright 2023-2024 Jussi Pakkanen


import ctypes
import os, sys
import math
from enum import Enum, IntFlag, auto

class LineCapStyle(Enum):
    Butt = 0
    Round = 1
    Projection_Square = 2

class LineJoinStyle(Enum):
    Miter = 0
    Round = 1
    Bevel = 2

class BlendMode(Enum):
    Normal = 0
    Multiply = 1
    Screen = 2
    Overlay = 3
    Darken = 4
    Lighten = 5
    Colordodge = 6
    Colorburn = 7
    Hardlight = 8
    Softlight = 9
    Difference = 10
    Exclusion = 11
    Hue = 12
    Saturation = 13
    Color = 14
    Luminosity = 15

class DeviceColorspace(Enum):
    RGB = 0
    Gray = 1
    CMYK = 2

class ImageColorspace(Enum):
    RGB = 0
    Gray = 1
    CMYK = 2

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

class TransitionDimension:
    H = 0,
    V = 1

class TransitionMotion:
    I = 0,
    O = 1

class PageBox(Enum):
    Media = 0
    Crop = 1
    Bleed = 2
    Trim = 3
    Art = 4

class RenderingIntent(Enum):
    RelativeColorimetric = 0
    AbsoluteColorimetric = 1
    Saturation = 2
    Perceptual = 3

class TextMode(Enum):
    Fill = 0
    Stroke = 1
    FillStroke = 2
    Invisible = 3
    FillClip = 4
    StrokeClip = 5
    FillStrokeClip = 6
    Clip = 7

class PdfXType(Enum):
    X1_2001 = 0
    X1A_2001 = 1
    X1A_2003 = 2
    X3_2002 = 3
    X3_2003 = 4
    X4 = 5
    X4P = 6
    X5G = 7
    X5PG= 8

class ImageInterpolation(Enum):
    Automatic = 0
    Pixelated = 1
    Smooth = 2

class Compression(Enum):
    Not = 0
    Deflate = 1

class AnnotationFlag(IntFlag):
    Invisible = auto()
    Hidden = auto()
    Print = auto()
    Nozoom = auto()
    NoRotate = auto()
    NoView = auto()
    ReadOnly = auto()
    Locked = auto()
    ToggleNoView = auto()
    LockedContents = auto()

class StructureType(Enum):
    Document = 0
    DocumentFragment = 1

    Part = 2,
    Sect = 3,
    Div = 4
    Aside = 5
    Nonstruct = 6

    P = 7
    H = 8
    H1 = 9
    H2 = 10
    H3 = 11
    H4 = 12
    H5 = 13
    H6 = 14
    H7 = 15
    Title = 16
    FENote = 17

    Sub = 18

    Lbl = 19
    Span = 20
    Em = 21
    Strong = 22
    Link = 23
    Annot = 24
    Form = 25

    Ruby = 26
    RB = 27
    RT = 28
    RP = 29
    Warichu = 30
    WT = 31
    WP = 32

    L = 33
    Li = 34
    LBody = 35
    Table = 36
    TR = 37
    TH = 38
    TD = 39
    Thead = 40
    TBody = 41
    TFoot = 42

    Caption = 43

    Figure = 44

    Formula = 45

    Artifact = 46

class PageLayout(Enum):
    SinglePage = 0
    OneColumn = 1
    TwoColumnLeft = 2
    TwoColumnRight = 3
    TwoPageLeft = 4
    TwoPageRight = 5

class PageMode(Enum):
    UseNone = 0
    Outlines = 1
    Thumbs = 2
    FullScreen = 3
    OC = 4
    Attachments = 5

class CapyPDFException(Exception):
    def __init__(*args, **kwargs):
        Exception.__init__(*args, **kwargs)

ec_type = ctypes.c_int32
enum_type = ctypes.c_int32

class AnnotationId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class FontId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class GraphicsStateId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class IccColorSpaceId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class LabColorSpaceId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class ImageId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class OptionalContentGroupId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class FunctionId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class ShadingId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class OutlineId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class SeparationId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class PatternId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class EmbeddedFileId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class FormXObjectId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class TransparencyGroupId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class StructureItemId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]

class RoleId(ctypes.Structure):
    _fields_ = [('id', ctypes.c_int32)]


cfunc_types = (

('capy_doc_md_new', [ctypes.c_void_p]),
('capy_doc_md_destroy', [ctypes.c_void_p]),
('capy_doc_md_set_colorspace', [ctypes.c_void_p, enum_type]),
('capy_doc_md_set_device_profile', [ctypes.c_void_p, enum_type, ctypes.c_char_p]),
('capy_doc_md_set_title', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_doc_md_set_author', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_doc_md_set_creator', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_doc_md_set_language', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_doc_md_set_output_intent', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_doc_md_set_pdfx', [ctypes.c_void_p, enum_type]),
('capy_doc_md_set_default_page_properties', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_doc_md_set_tagged', [ctypes.c_void_p, ctypes.c_int32]),

('capy_page_properties_new', [ctypes.c_void_p]),
('capy_page_properties_destroy', [ctypes.c_void_p]),
('capy_page_properties_set_pagebox',
    [ctypes.c_void_p, enum_type, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),

('capy_generator_new', [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_page', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_form_xobject', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_transparency_group', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_color_pattern', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_embed_jpg', [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_embed_file', [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p]),
('capy_generator_load_image', [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p]),
('capy_generator_convert_image', [ctypes.c_void_p, ctypes.c_void_p, enum_type, enum_type, ctypes.c_void_p]),
('capy_generator_load_icc_profile', [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p]),
('capy_generator_add_lab_colorspace', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_void_p]),
('capy_generator_load_font', [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p]),
('capy_generator_add_image', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_type2_function', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_type2_shading', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_type3_shading', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_type4_shading', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_type6_shading', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_structure_item', [ctypes.c_void_p, enum_type, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_custom_structure_item', [ctypes.c_void_p, RoleId, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_write', [ctypes.c_void_p]),
('capy_generator_add_graphics_state', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_optional_content_group', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_add_outline', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_create_separation_simple', [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_generator_text_width', [ctypes.c_void_p, ctypes.c_char_p, FontId, ctypes.c_double, ctypes.POINTER(ctypes.c_double)]),
('capy_generator_add_rolemap_entry', [ctypes.c_void_p, ctypes.c_char_p, enum_type, ctypes.c_void_p]),
('capy_generator_destroy', [ctypes.c_void_p]),

('capy_page_draw_context_new', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_dc_add_simple_navigation', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p]),
('capy_dc_cmd_b', [ctypes.c_void_p]),
('capy_dc_cmd_B', [ctypes.c_void_p]),
('capy_dc_cmd_bstar', [ctypes.c_void_p]),
('capy_dc_cmd_Bstar', [ctypes.c_void_p]),
('capy_dc_cmd_BDC_ocg', [ctypes.c_void_p, OptionalContentGroupId]),
('capy_dc_cmd_BMC', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_dc_cmd_c', [ctypes.c_void_p,
    ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_cm', [ctypes.c_void_p,
    ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_d', [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.c_int32, ctypes.c_double]),
('capy_dc_cmd_Do', [ctypes.c_void_p, TransparencyGroupId]),
('capy_dc_cmd_EMC', [ctypes.c_void_p]),
('capy_dc_cmd_f', [ctypes.c_void_p]),
('capy_dc_cmd_fstar', [ctypes.c_void_p]),
('capy_dc_cmd_G', [ctypes.c_void_p, ctypes.c_double]),
('capy_dc_cmd_g', [ctypes.c_void_p, ctypes.c_double]),
('capy_dc_cmd_gs', [ctypes.c_void_p, GraphicsStateId]),
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
('capy_dc_cmd_sh', [ctypes.c_void_p, ShadingId]),
('capy_dc_cmd_v', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_cmd_w', [ctypes.c_void_p, ctypes.c_double]),
('capy_dc_cmd_W', [ctypes.c_void_p]),
('capy_dc_cmd_Wstar', [ctypes.c_void_p]),
('capy_dc_cmd_y', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_set_custom_page_properties', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_dc_draw_image',
    [ctypes.c_void_p, ImageId]),
('capy_dc_render_text',
    [ctypes.c_void_p, ctypes.c_char_p, FontId, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_dc_render_text_obj',
    [ctypes.c_void_p, ctypes.c_void_p]),
('capy_dc_set_nonstroke', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_dc_text_new', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_dc_annotate', [ctypes.c_void_p, AnnotationId]),
('capy_dc_destroy', [ctypes.c_void_p]),

('capy_color_pattern_context_new', [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_double, ctypes.c_double]),
('capy_form_xobject_new', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_void_p]),
('capy_transparency_group_new', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_void_p]),
('capy_transparency_group_extra_new', [ctypes.c_void_p]),

('capy_text_sequence_new', [ctypes.c_void_p]),
('capy_text_sequence_append_codepoint', [ctypes.c_void_p, ctypes.c_uint32]),
('capy_text_sequence_append_kerning', [ctypes.c_void_p, ctypes.c_int32]),
('capy_text_sequence_append_actualtext_start', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_text_sequence_append_actualtext_end', [ctypes.c_void_p]),
('capy_text_sequence_append_raw_glyph', [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32]),
('capy_text_sequence_append_ligature_glyph', [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_char_p]),
('capy_text_sequence_destroy', [ctypes.c_void_p]),

('capy_text_destroy', [ctypes.c_void_p]),
('capy_text_cmd_BDC_builtin', [ctypes.c_void_p, StructureItemId]),
('capy_text_cmd_EMC', [ctypes.c_void_p]),
('capy_text_cmd_Tc', [ctypes.c_void_p, ctypes.c_double]),
('capy_text_cmd_Td', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double]),
('capy_text_cmd_Tf', [ctypes.c_void_p, FontId, ctypes.c_double]),
('capy_text_cmd_TJ', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_text_cmd_TL', [ctypes.c_void_p, ctypes.c_double]),
('capy_text_cmd_Tm', [ctypes.c_void_p,
    ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_text_cmd_Tr', [ctypes.c_void_p, enum_type]),
('capy_text_cmd_Tw', [ctypes.c_void_p, ctypes.c_double]),
('capy_text_cmd_Tstar', [ctypes.c_void_p]),
('capy_text_render_text', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_text_set_stroke', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_text_set_nonstroke', [ctypes.c_void_p, ctypes.c_void_p]),

('capy_color_new', [ctypes.c_void_p]),
('capy_color_destroy', [ctypes.c_void_p]),
('capy_color_set_rgb', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_color_set_gray', [ctypes.c_void_p, ctypes.c_double]),
('capy_color_set_cmyk', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_color_set_icc', [ctypes.c_void_p, IccColorSpaceId, ctypes.POINTER(ctypes.c_double), ctypes.c_int32]),
('capy_color_set_separation', [ctypes.c_void_p, SeparationId, ctypes.c_double]),
('capy_color_set_pattern', [ctypes.c_void_p, PatternId]),
('capy_color_set_lab', [ctypes.c_void_p, LabColorSpaceId, ctypes.c_double, ctypes.c_double, ctypes.c_double]),

('capy_transition_new', [ctypes.c_void_p]),
('capy_transition_set_S', [ctypes.c_void_p, enum_type]),
('capy_transition_set_D', [ctypes.c_void_p, ctypes.c_double]),
('capy_transition_set_Dm', [ctypes.c_void_p, enum_type]),
('capy_transition_set_M', [ctypes.c_void_p, enum_type]),
('capy_transition_set_Di', [ctypes.c_void_p, ctypes.c_uint32]),
('capy_transition_set_SS', [ctypes.c_void_p, ctypes.c_double]),
('capy_transition_set_B', [ctypes.c_void_p, ctypes.c_int32]),
('capy_transition_destroy', [ctypes.c_void_p]),

('capy_graphics_state_new', [ctypes.c_void_p]),
('capy_graphics_state_set_CA', [ctypes.c_void_p, ctypes.c_double]),
('capy_graphics_state_set_ca', [ctypes.c_void_p, ctypes.c_double]),
('capy_graphics_state_set_BM', [ctypes.c_void_p, enum_type]),
('capy_graphics_state_set_op', [ctypes.c_void_p, ctypes.c_int32]),
('capy_graphics_state_set_OP', [ctypes.c_void_p, ctypes.c_int32]),
('capy_graphics_state_set_OPM', [ctypes.c_void_p, ctypes.c_int32]),
('capy_graphics_state_set_TK', [ctypes.c_void_p, ctypes.c_int32]),
('capy_graphics_state_destroy', [ctypes.c_void_p]),

('capy_raster_image_builder_new', [ctypes.c_void_p]),
('capy_raster_image_builder_set_size', [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32]),
('capy_raster_image_builder_set_pixel_data', [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int32]),
('capy_raster_image_builder_set_compression', [ctypes.c_void_p, enum_type]),
('capy_raster_image_builder_build', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_raster_image_get_colorspace', [ctypes.c_void_p, ctypes.POINTER(enum_type)]),
('capy_raster_image_has_profile', [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int32)]),
('capy_raster_image_builder_destroy', [ctypes.c_void_p]),

('capy_raster_image_destroy', [ctypes.c_void_p]),

('capy_optional_content_group_new', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_optional_content_group_destroy', [ctypes.c_void_p]),

('capy_type2_function_new', [ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_double, ctypes.c_void_p]),
('capy_type2_function_destroy', [ctypes.c_void_p]),

('capy_type2_shading_new', [enum_type,
                            ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double,
                            FunctionId, ctypes.c_int32, ctypes.c_int32,
                            ctypes.c_void_p]),
('capy_type2_shading_destroy', [ctypes.c_void_p]),


('capy_type3_shading_new', [enum_type, ctypes.POINTER(ctypes.c_double),
                            FunctionId, ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p]),
('capy_type3_shading_destroy', [ctypes.c_void_p]),

('capy_type4_shading_new', [enum_type, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_void_p]),
('capy_type4_shading_add_triangle', [ctypes.c_void_p,
                                     ctypes.POINTER(ctypes.c_double),
                                     ctypes.c_void_p]),
('capy_type4_shading_extend', [ctypes.c_void_p,
                               ctypes.c_int32,
                               ctypes.POINTER(ctypes.c_double),
                               ctypes.c_void_p]),
('capy_type4_shading_destroy', [ctypes.c_void_p]),

('capy_type6_shading_new', [enum_type, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_void_p]),
('capy_type6_shading_add_patch', [ctypes.c_void_p,
                                  ctypes.POINTER(ctypes.c_double),
                                  ctypes.c_void_p]),
('capy_type6_shading_extend', [ctypes.c_void_p,
                               ctypes.c_int32,
                               ctypes.POINTER(ctypes.c_double),
                               ctypes.c_void_p]),
('capy_type6_shading_destroy', [ctypes.c_void_p]),

('capy_text_annotation_new', [ctypes.c_char_p, ctypes.c_void_p]),
('capy_file_attachment_annotation_new', [EmbeddedFileId, ctypes.c_void_p]),
('capy_printers_mark_annotation_new', [FormXObjectId, ctypes.c_void_p]),
('capy_annotation_set_rectangle', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_annotation_set_flags', [ctypes.c_void_p, enum_type]),
('capy_annotation_destroy', [ctypes.c_void_p]),

('capy_struct_item_extra_data_new', [ctypes.c_void_p]),
('capy_struct_item_extra_data_set_t', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_struct_item_extra_data_set_lang', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_struct_item_extra_data_set_alt', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_struct_item_extra_data_set_actual_text', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_struct_item_extra_data_destroy', [ctypes.c_void_p]),

('capy_image_pdf_properties_new', [ctypes.c_void_p]),
('capy_image_pdf_properties_set_mask', [ctypes.c_void_p, ctypes.c_int32]),
('capy_image_pdf_properties_set_interpolate', [ctypes.c_void_p, enum_type]),
('capy_image_pdf_properties_destroy', [ctypes.c_void_p]),

('capy_destination_new', [ctypes.c_void_p]),
('capy_destination_set_page_fit', [ctypes.c_void_p, ctypes.c_int32]),
('capy_destination_set_page_xyz', [ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]),
('capy_destination_destroy', [ctypes.c_void_p]),

('capy_outline_new', [ctypes.c_void_p]),
('capy_outline_set_title', [ctypes.c_void_p, ctypes.c_char_p]),
('capy_outline_set_destination', [ctypes.c_void_p, ctypes.c_void_p]),
('capy_outline_set_rgb', [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
('capy_outline_set_f', [ctypes.c_void_p, ctypes.c_uint32]),
('capy_outline_set_parent', [ctypes.c_void_p, OutlineId]),
('capy_outline_destroy', [ctypes.c_void_p]),

)

def locate_shared_lib():
    if sys.platform == 'win32':
        libfile_name = 'capypdf-0.dll'
    elif sys.platform == 'darwin':
        libfile_name = 'libcapypdf.0.dylib'
    else:
        libfile_name = 'libcapypdf.so'
    libfile = None

    if 'CAPYPDF_SO_OVERRIDE' in os.environ:
        libfile = ctypes.cdll.LoadLibrary(os.path.join(os.environ['CAPYPDF_SO_OVERRIDE'], libfile_name))

    if libfile is None:
        try:
            libfile = ctypes.cdll.LoadLibrary(libfile_name)
        except (FileNotFoundError, OSError):
            pass

    if libfile is None:
        from glob import glob
        if 'site-packages' in __file__:
            # Most likely a wheel installation with embedded libs.
            sdir = os.path.split(__file__)[0]
            # Match libcapypdf.so.0.5.0 and similar names too
            globber = os.path.join(sdir, '.*capypdf*', libfile_name + "*")
            matches = glob(globber)
            if len(matches) == 1:
                libfile = ctypes.cdll.LoadLibrary(matches[0])
        else:
            # FIXME, system installation without the -dev package
            # so that the file name is `libcapypdf.so.0.22.0
            # or similar
            pass
    return libfile

libfile = locate_shared_lib()

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

def to_array(ctype, array):
    if not isinstance(array, (list, tuple)):
        raise CapyPDFException('Array value argument must be an list or tuple.')
    return (ctype * len(array))(*array), len(array)

class DocumentMetadata:
    def __init__(self):
        opt = ctypes.c_void_p()
        check_error(libfile.capy_doc_md_new(ctypes.pointer(opt)))
        self._as_parameter_ = opt

    def __del__(self):
        check_error(libfile.capy_doc_md_destroy(self))

    def set_colorspace(self, cs):
        if not isinstance(cs, DeviceColorspace):
            raise CapyPDFException('Argument not a device colorspace object.')
        check_error(libfile.capy_doc_md_set_colorspace(self, cs.value))

    def set_title(self, title):
        if not isinstance(title, str):
            raise CapyPDFException('Title must be an Unicode string.')
        check_error(libfile.capy_doc_md_set_title(self, title.encode('UTF-8')))

    def set_author(self, author):
        if not isinstance(author, str):
            raise CapyPDFException('Author must be an Unicode string.')
        check_error(libfile.capy_doc_md_set_author(self, author.encode('UTF-8')))

    def set_creator(self, creator):
        if not isinstance(creator, str):
            raise CapyPDFException('Creator must be an Unicode string.')
        check_error(libfile.capy_doc_md_set_creator(self, creator.encode('UTF-8')))

    def set_language(self, lang):
        if not isinstance(lang, str):
            raise CapyPDFException('Creator must be an Unicode string.')
        check_error(libfile.capy_doc_md_set_language(self, lang.encode('ASCII')))

    def set_device_profile(self, colorspace, path):
        check_error(libfile.capy_doc_md_set_device_profile(self, colorspace.value, to_bytepath(path)))

    def set_output_intent(self, identifier):
        check_error(libfile.capy_doc_md_set_output_intent(self, identifier.encode('utf-8')))

    def set_pdfx(self, xtype):
        if not isinstance(xtype, PdfXType):
            raise CapyPDFException('Argument must be an PDF/X type.')
        check_error(libfile.capy_doc_md_set_pdfx(self, xtype.value))

    def set_default_page_properties(self, props):
        if not isinstance(props, PageProperties):
            raise CapyPDFException('Argument is not a PageProperties object.')
        check_error(libfile.capy_doc_md_set_default_page_properties(self, props))

    def set_tagged(self, is_tagged):
        tagint = 1 if is_tagged else 0
        check_error(libfile.capy_doc_md_set_tagged(self, tagint))


class PageProperties:
    def __init__(self):
        opt = ctypes.c_void_p()
        check_error(libfile.capy_page_properties_new(ctypes.pointer(opt)))
        self._as_parameter_ = opt

    def __del__(self):
        check_error(libfile.capy_page_properties_destroy(self))

    def set_pagebox(self, boxtype, x1, y1, x2, y2):
        check_error(libfile.capy_page_properties_set_pagebox(self, boxtype.value, x1, y1, x2, y2))


class DrawContextBase:

    def __init__(self, generator):
        self.generator = generator

    def __del__(self):
        check_error(libfile.capy_dc_destroy(self))

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
        # FIXME, return a context manager.

    def cmd_BDC_builtin(self, structid):
        if not isinstance(structid, StructureItemId):
            raise CapyPDFException('Argument must be a structure item ID.')
        check_error(libfile.capy_dc_cmd_BDC_builtin(self, structid))
        return MarkedContextManager(self)

    def cmd_BMC(self, tag):
        check_error(libfile.capy_dc_cmd_BMC(self, tag.encode('UTF-8')))
        return MarkedContextManager(self)

    def cmd_c(self, x1, y1, x2, y2, x3, y3):
        check_error(libfile.capy_dc_cmd_c(self, x1, y1, x2, y2, x3, y3))

    def cmd_cm(self, m1, m2, m3, m4, m5, m6):
        check_error(libfile.capy_dc_cmd_cm(self, m1, m2, m3, m4, m5, m6))

    def cmd_d(self, array, phase):
        check_error(libfile.capy_dc_cmd_d(self, *to_array(ctypes.c_double, array), phase))

    def cmd_Do(self, tgid):
        if not isinstance(tgid, TransparencyGroupId):
            raise CapyPDFException('Argument must be a transparency group id.')
        check_error(libfile.capy_dc_cmd_Do(self, tgid))

    def cmd_EMC(self):
        check_error(libfile.capy_dc_cmd_EMC(self))

    def cmd_f(self):
        check_error(libfile.capy_dc_cmd_f(self))

    def cmd_fstar(self):
        check_error(libfile.capy_dc_cmd_fstar(self))

    def cmd_G(self, gray):
        check_error(libfile.capy_dc_cmd_G(self, gray))

    def cmd_g(self, gray):
        check_error(libfile.capy_dc_cmd_g(self, gray))

    def cmd_gs(self, gsid):
        if not isinstance(gsid, GraphicsStateId):
            raise CapyPDFException('Argument must be a graphics state id.')
        check_error(libfile.capy_dc_cmd_gs(self, gsid))

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

    def cmd_sh(self, shid):
        check_error(libfile.capy_dc_cmd_sh(self, shid))

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
        if isinstance(color, PatternId):
            pattern_color = Color()
            pattern_color.set_pattern(color)
            color = pattern_color
        if not isinstance(color, Color):
            raise CapyPDFException('Argument must be a Color object.')
        check_error(libfile.capy_dc_set_stroke(self, color))

    def set_nonstroke(self, color):
        if isinstance(color, PatternId):
            pattern_color = Color()
            pattern_color.set_pattern(color)
            color = pattern_color
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

    def text_new(self):
        return Text(self)

    def translate(self, xtran, ytran):
        self.cmd_cm(1.0, 0, 0, 1.0, xtran, ytran)

    def scale(self, xscale, yscale):
        self.cmd_cm(xscale, 0, 0, yscale, 0, 0)

    def rotate(self, angle):
        self.cmd_cm(math.cos(angle), math.sin(angle), -math.sin(angle), math.cos(angle), 0.0, 0.0)

class DrawContext(DrawContextBase):

    def __init__(self, generator):
        super().__init__(generator)
        dcptr = ctypes.c_void_p()
        check_error(libfile.capy_page_draw_context_new(generator, ctypes.pointer(dcptr)))
        self._as_parameter_ = dcptr

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        try:
            self.generator.add_page(self)
        finally:
            self.generator = None # Not very elegant.

    def push_gstate(self):
        return StateContextManager(self)

    def add_simple_navigation(self, ocgs, transition=None):
        arraytype = len(ocgs)*OptionalContentGroupId
        arr = arraytype(*tuple(ocgs))
        if transition is not None and not isinstance(transition, Transition):
            raise CapyPDFException('Transition argument must be a transition object.')
        check_error(libfile.capy_dc_add_simple_navigation(self,
                                                          ctypes.pointer(arr),
                                                          len(ocgs),
                                                          transition))

    def set_custom_page_properties(self, props):
        if not isinstance(props, PageProperties):
            raise CapyPDFException('Argument is not a PageProperties object.')
        check_error(libfile.capy_dc_set_custom_page_properties(self, props))

    def annotate(self, annotation_id):
        check_error(libfile.capy_dc_annotate(self, annotation_id))

class ColorPatternDrawContext(DrawContextBase):

    def __init__(self, generator, w, h):
        super().__init__(generator)
        dcptr = ctypes.c_void_p()
        check_error(libfile.capy_color_pattern_context_new(generator, ctypes.pointer(dcptr), w, h))
        self._as_parameter_ = dcptr

class FormXObjectDrawContext(DrawContextBase):

    def __init__(self, generator, w, h):
        super().__init__(generator)
        dcptr = ctypes.c_void_p()
        check_error(libfile.capy_form_xobject_new(generator, w, h, ctypes.pointer(dcptr)))
        self._as_parameter_ = dcptr

class TransparencyGroupDrawContext(DrawContextBase):

    def __init__(self, generator, left, bottom, right, top):
        super().__init__(generator)
        dcptr = ctypes.c_void_p()
        check_error(libfile.capy_transparency_group_new(generator, left, bottom, right, top, ctypes.pointer(dcptr)))
        self._as_parameter_ = dcptr
        # Extra options include directly
        self._optptr = ctypes.c_void_p()
        check_error(libfile.capy_transparency_group_extra_new(ctypes.pointer(self._optptr)))


class StateContextManager:
    def __init__(self, ctx):
        self.ctx = ctx
        ctx.cmd_q()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        self.ctx.cmd_Q()

class MarkedContextManager:

    def __init__(self, dc):
        self.dc = dc

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        try:
            self.dc.cmd_EMC()
        finally:
            self.dc = None # Not very elegant.

class Generator:
    def __init__(self, filename, options=None):
        file_name_bytes = to_bytepath(filename)
        if options is None:
            options = DocumentMetadata()
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

    def create_color_pattern_context(self, w, h):
        return ColorPatternDrawContext(self, w, h)

    def add_page(self, page_ctx):
        check_error(libfile.capy_generator_add_page(self, page_ctx))

    def add_form_xobject(self, fxo_ctx):
        fxid = FormXObjectId()
        check_error(libfile.capy_generator_add_form_xobject(self, fxo_ctx, ctypes.pointer(fxid)))
        return fxid

    def add_transparency_group(self, tg_ctx):
        tgid = TransparencyGroupId()
        check_error(libfile.capy_generator_add_transparency_group(self, tg_ctx, tg_ctx._optptr, ctypes.pointer(tgid)))
        return tgid

    def add_color_pattern(self, pattern_ctx):
        pid = PatternId()
        check_error(libfile.capy_generator_add_color_pattern(self, pattern_ctx, ctypes.pointer(pid)))
        return pid

    def embed_jpg(self, fname, props):
        if not isinstance(props, ImagePdfProperties):
            raise CapyPDFException('Argument must be an image property object.')
        iid = ImageId()
        check_error(libfile.capy_generator_embed_jpg(self, to_bytepath(fname), props, ctypes.pointer(iid)))
        return iid

    def embed_file(self, fname):
        fid = EmbeddedFileId()
        check_error(libfile.capy_generator_embed_file(self, to_bytepath(fname), ctypes.pointer(fid)))
        return fid

    def load_font(self, fname):
        fid = FontId()
        check_error(libfile.capy_generator_load_font(self, to_bytepath(fname), ctypes.pointer(fid)))
        return fid

    def load_icc_profile(self, fname):
        iid = IccColorSpaceId()
        check_error(libfile.capy_generator_load_icc_profile(self, to_bytepath(fname), ctypes.pointer(iid)))
        return iid

    def add_lab_colorspace(self, xw, yw, zw, amin, amax, bmin, bmax):
        lid = LabColorSpaceId()
        check_error(libfile.capy_generator_add_lab_colorspace(self, xw, yw, zw, amin, amax, bmin, bmax, ctypes.pointer(lid)))
        return lid

    def load_image(self, fname):
        optr = ctypes.c_void_p()
        check_error(libfile.capy_generator_load_image(self, to_bytepath(fname), ctypes.pointer(optr)))
        return RasterImage(optr)

    def convert_image(self, in_image, output_cs, ri):
        if not isinstance(in_image, RasterImage):
            raise CapyPDFException('First argument must be a RasterImage object.')
        optr = ctypes.c_void_p()
        check_error(libfile.capy_generator_convert_image(self, in_image, output_cs.value, ri.value, ctypes.pointer(optr)))
        return RasterImage(optr)

    def add_image(self, ri, params):
        if not isinstance(ri, RasterImage):
            raise CapyPDFException('First argument must be a raster image.')
        if not isinstance(params, ImagePdfProperties):
            raise CapyPDFException('Second argument must be an PDF property object.')
        iid = ImageId()
        check_error(libfile.capy_generator_add_image(self, ri, params, ctypes.pointer(iid)))
        return iid

    def add_type2_function(self, type2func):
        if not isinstance(type2func, Type2Function):
            raise CapyPDFException('Argument must be a function.')
        fid = FunctionId()
        check_error(libfile.capy_generator_add_type2_function(self, type2func, ctypes.pointer(fid)))
        return fid

    def add_type2_shading(self, type2shade):
        if not isinstance(type2shade, Type2Shading):
            raise CapyPDFException('Argument must be a type 2 shading object.')
        shid = ShadingId()
        check_error(libfile.capy_generator_add_type2_shading(self, type2shade, ctypes.pointer(shid)))
        return shid

    def add_type3_shading(self, type3shade):
        if not isinstance(type3shade, Type3Shading):
            raise CapyPDFException('Argument must be a type 3 shading object.')
        shid = ShadingId()
        check_error(libfile.capy_generator_add_type3_shading(self, type3shade, ctypes.pointer(shid)))
        return shid

    def add_type4_shading(self, type4shade):
        if not isinstance(type4shade, Type4Shading):
            raise CapyPDFException('Argument must be a type 4 shading object.')
        shid = ShadingId()
        check_error(libfile.capy_generator_add_type4_shading(self, type4shade, ctypes.pointer(shid)))
        return shid

    def add_type6_shading(self, type6shade):
        if not isinstance(type6shade, Type6Shading):
            raise CapyPDFException('Argument must be a type 4 shading object.')
        shid = ShadingId()
        check_error(libfile.capy_generator_add_type6_shading(self, type6shade, ctypes.pointer(shid)))
        return shid

    def add_structure_item(self, struct_type, parent=None, extra=None):
        if parent is None:
            parentptr = None
        else:
            if not isinstance(parent, StructureItemId):
                raise CapyPDFException('Parent argument must be a StructureItemID or None.')
            parentptr = ctypes.pointer(parent)
        if extra is None:
            extraptr = None
        else:
            if not isinstance(extra, StructItemExtraData):
                raise CapyPDFException('Extra argument must be a StructItemExtraData.')
            extraptr = extra._as_parameter_
        stid = StructureItemId()
        if isinstance(struct_type, StructureType):
            check_error(libfile.capy_generator_add_structure_item(self, struct_type.value, parentptr, extraptr, ctypes.pointer(stid)))
        elif isinstance(struct_type, RoleId):
            check_error(libfile.capy_generator_add_custom_structure_item(self, struct_type, parentptr, extraptr, ctypes.pointer(stid)))
        else:
            raise CapyPDFException('First argument must be a structure item or role id.')
        return stid


    def create_separation_simple(self, name, color):
        if not isinstance(color, Color):
            raise CapyPDFException('Color argument must be a color object.')
        sepid = SeparationId()
        text_bytes = name.encode('UTF-8')
        check_error(libfile.capy_generator_create_separation_simple(self, text_bytes, color, ctypes.pointer(sepid)))
        return sepid

    def write(self):
        check_error(libfile.capy_generator_write(self))

    def text_width(self, text, font, pointsize):
        if not isinstance(text, str):
            raise CapyPDFException('Text must be a Unicode string.')
        if not isinstance(font, FontId):
            raise CapyPDFException('Font argument is not a font id.')
        w = ctypes.c_double()
        bytes = text.encode('UTF-8')
        check_error(libfile.capy_generator_text_width(self, bytes, font, pointsize, ctypes.pointer(w)))
        return w.value

    def add_graphics_state(self, gs):
        if not isinstance(gs, GraphicsState):
            raise CapyPDFException('Argument must be a graphics state object.')
        gsid = GraphicsStateId()
        check_error(libfile.capy_generator_add_graphics_state(self, gs, ctypes.pointer(gsid)))
        return gsid

    def add_outline(self, outline):
        if not isinstance(outline, Outline):
            raise CapyPDFException('Argument must be an outline object.')
        oid = OutlineId()
        check_error(libfile.capy_generator_add_outline(self, outline, ctypes.pointer(oid)))
        return oid

    def add_optional_content_group(self, ocg):
        ocgid = OptionalContentGroupId()
        check_error(libfile.capy_generator_add_optional_content_group(self, ocg, ctypes.pointer(ocgid)))
        return ocgid

    def create_annotation(self, annotation):
        aid = AnnotationId()
        check_error(libfile.capy_generator_create_annotation(self, annotation, ctypes.pointer(aid)))
        return aid

    def add_rolemap_entry(self, name, builtin_type):
        if not isinstance(builtin_type, StructureType):
            raise CapyPDFException('Builtin type must be a StructureType.')
        roid = RoleId()
        name_bytes = name.encode('ASCII')
        check_error(libfile.capy_generator_add_rolemap_entry(self, name_bytes, builtin_type.value, ctypes.pointer(roid)))
        return roid


class TextSequence:
    def __init__(self):
        opt = ctypes.c_void_p()
        check_error(libfile.capy_text_sequence_new(ctypes.pointer(opt)))
        self._as_parameter_ = opt

    def __del__(self):
        check_error(libfile.capy_text_sequence_destroy(self))

    def append_codepoint(self, codepoint):
        if not isinstance(codepoint, int):
            codepoint = ord(codepoint)
        check_error(libfile.capy_text_sequence_append_codepoint(self, codepoint))

    def append_kerning(self, kern):
        check_error(libfile.capy_text_sequence_append_kerning(self, kern))

    def append_actualtext_start(self, txt):
        check_error(libfile.capy_text_sequence_append_actualtext_start(self, txt.encode('UTF-8')))

    def append_actualtext_end(self):
        check_error(libfile.capy_text_sequence_append_actualtext_end(self))

    def append_raw_glyph(self, glyph_id, codepoint):
        if not isinstance(codepoint, int):
            codepoint = ord(codepoint)
        check_error(libfile.capy_text_sequence_append_raw_glyph(self, glyph_id, codepoint))

    def append_ligature_glyph(self, glyph_id, txt):
        u8txt = txt.encode('UTF-8')
        check_error(libfile.capy_text_sequence_append_ligature_glyph(self, glyph_id, u8txt))


class Text:
    def __init__(self, dc):
        if not isinstance(dc, DrawContext):
            raise CapyPDFException('Argument must be a DrawingContext (preferably use its .text_new() method instead).')
        self._as_parameter_ = None
        opt = ctypes.c_void_p()
        check_error(libfile.capy_dc_text_new(dc, ctypes.pointer(opt)))
        self._as_parameter_ = opt
        self.dc = dc

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        try:
            self.dc.render_text_obj(self)
        finally:
            self.dc = None # Not very elegant.

    def __del__(self):
        if self._as_parameter_ is not None:
            check_error(libfile.capy_text_destroy(self))

    def render_text(self, text):
        if not isinstance(text, str):
            raise CapyPDFException('Text must be a Unicode string.')
        bytes = text.encode('UTF-8')
        check_error(libfile.capy_text_render_text(self, bytes))

    def set_nonstroke(self, color):
        check_error(libfile.capy_text_set_nonstroke(self, color))

    def set_stroke(self, color):
        check_error(libfile.capy_text_set_stroke(self, color))

    def cmd_BDC_builtin(self, struct_id):
        check_error(libfile.capy_text_cmd_BDC_builtin(self, struct_id))
        return MarkedContextManager(self)

    def cmd_EMC(self):
        check_error(libfile.capy_text_cmd_EMC(self))

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

    def cmd_TJ(self, seq):
        if not isinstance(seq, TextSequence):
            raise CapyPDFException('Argument must be a kerning sequence.')
        check_error(libfile.capy_text_cmd_TJ(self, seq))

    def cmd_Tm(self, a, b, c, d, e, f):
        check_error(libfile.capy_text_cmd_Tm(self, a, b, c, d, e, f))

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

    def get_underlying(self):
        return self._as_parameter_

    def set_rgb(self, r, g, b):
        check_error(libfile.capy_color_set_rgb(self, r, g, b))

    def set_gray(self, g):
        check_error(libfile.capy_color_set_gray(self, g))

    def set_cmyk(self, c, m, y, k):
        check_error(libfile.capy_color_set_cmyk(self, c, m, y, k))

    def set_icc(self, icc_id, values):
        check_error(libfile.capy_color_set_icc(self, icc_id, *to_array(ctypes.c_double, values)))

    def set_separation(self, sepid, value):
        check_error(libfile.capy_color_set_separation(self, sepid, value))

    def set_pattern(self, pattern_id):
        check_error(libfile.capy_color_set_pattern(self, pattern_id))

    def set_lab(self, lab_id, l, a, b):
        check_error(libfile.capy_color_set_lab(self, lab_id, l, a, b))

class Transition:
    def __init__(self):
        self._as_parameter_ = None
        opt = ctypes.c_void_p()
        check_error(libfile.capy_transition_new(ctypes.pointer(opt)))
        self._as_parameter_ = opt

    def set_S(self, S):
        if not isinstance(S, TransitionType):
            raise CapyPDFException('Argument is not a transition type.')
        check_error(libfile.capy_transition_set_S(self, S.value))

    def set_D(self, d):
        check_error(libfile.capy_transition_set_D(self, d))

    def set_Dn(self, dm):
        check_error(libfile.capy_transition_set_Dm(self, Dm.value))

    def set_M(self, m):
        check_error(libfile.capy_transition_set_M(self, M.value))

    def set_Di(self, Di):
        check_error(libfile.capy_transition_set_Di(self, Di))

    def set_SS(self, ss):
        check_error(libfile.capy_transition_set_SS(self, ss))

    def set_B(self, B):
        check_error(libfile.capy_transition_set_S(self, int(B)))


    def __del__(self):
        check_error(libfile.capy_transition_destroy(self))

class RasterImage:
    def __init__(self, cptr = None):
        if cptr is None:
            self._as_parameter_ = None
            opt = ctypes.c_void_p()
            check_error(libfile.capy_raster_image_new(ctypes.pointer(opt)))
            self._as_parameter_ = opt
        else:
            self._as_parameter_ = cptr

    def __del__(self):
        check_error(libfile.capy_raster_image_destroy(self))

    def get_colorspace(self):
        val = enum_type(99)
        check_error(libfile.capy_raster_image_get_colorspace(self, ctypes.pointer(val)))
        return ImageColorspace(val.value)

    def has_profile(self):
        val = ctypes.c_int32(99)
        check_error(libfile.capy_raster_image_has_profile(self, ctypes.pointer(val)))
        return True if val.value != 0 else False

class RasterImageBuilder:
    def __init__(self, cptr = None):
        if cptr is None:
            self._as_parameter_ = None
            opt = ctypes.c_void_p()
            check_error(libfile.capy_raster_image_builder_new(ctypes.pointer(opt)))
            self._as_parameter_ = opt
        else:
            self._as_parameter_ = cptr

    def __del__(self):
        check_error(libfile.capy_raster_image_builder_destroy(self))

    def set_size(self, w, h):
        check_error(libfile.capy_raster_image_builder_set_size(self, w, h))

    def set_pixel_data(self, pixels):
        if not isinstance(pixels, bytes):
            raise CapyPDFException('Pixel data must be in bytes.')
        check_error(libfile.capy_raster_image_builder_set_pixel_data(self, pixels, len(pixels)))

    def set_compression(self, compression):
        if not isinstance(compression, Compression):
            raise CapyPDFException('Compression argument must be enum value.')
        check_error(libfile.capy_raster_image_builder_set_compression(self, compression.value))


    def build(self):
        opt = ctypes.c_void_p()
        check_error(libfile.capy_raster_image_builder_build(self, ctypes.pointer(opt)))
        return RasterImage(opt)


class GraphicsState:
    def __init__(self):
        self._as_parameter_ = None
        opt = ctypes.c_void_p()
        check_error(libfile.capy_graphics_state_new(ctypes.pointer(opt)))
        self._as_parameter_ = opt

    def __del__(self):
        check_error(libfile.capy_graphics_state_destroy(self))

    def set_CA(self, value):
        check_error(libfile.capy_graphics_state_set_CA(self, value))

    def set_ca(self, value):
        check_error(libfile.capy_graphics_state_set_ca(self, value))

    def set_BM(self, blendmode):
        check_error(libfile.capy_graphics_state_set_BM(self, blendmode.value))

    def set_op(self, value):
        value = 1 if value else 0
        check_error(libfile.capy_graphics_state_set_op(self, value))

    def set_OP(self, value):
        value = 1 if value else 0
        check_error(libfile.capy_graphics_state_set_OP(self, value))

    def set_OPM(self, value):
        check_error(libfile.capy_graphics_state_set_OPM(self, value))

    def set_TK(self, value):
        value = 1 if value else 0
        check_error(libfile.capy_graphics_state_set_TK(self, value))


class OptionalContentGroup:
    def __init__(self, name):
        self._as_parameter_ = None
        in_bytes = name.encode('ASCII')
        opt = ctypes.c_void_p()
        check_error(libfile.capy_optional_content_group_new(ctypes.pointer(opt), in_bytes))
        self._as_parameter_ = opt

    def __del__(self):
        check_error(libfile.capy_optional_content_group_destroy(self))

class Type2Function:
    def __init__(self, domain, c1, c2, n):
        self._as_parameter_ = None
        t2f = ctypes.c_void_p()
        check_error(libfile.capy_type2_function_new(*to_array(ctypes.c_double, domain), c1, c2, n, ctypes.pointer(t2f)))
        self._as_parameter_ = t2f

    def __del__(self):
        check_error(libfile.capy_type2_function_destroy(self))

class Type2Shading:
    def __init__(self, cs, x0, y0, x1, y1, funcid, extend1, extend2):
        e1 = 1 if extend1 else 0
        e2 = 1 if extend2 else 0
        self._as_parameter_ = None
        t2s = ctypes.c_void_p()
        check_error(libfile.capy_type2_shading_new(cs.value, x0, y0, x1, y1, funcid, e1, e2, ctypes.pointer(t2s)))
        self._as_parameter_ = t2s

    def __del__(self):
        check_error(libfile.capy_type2_shading_destroy(self))

class Type3Shading:
    def __init__(self, cs, coords, funcid, extend1, extend2):
        e1 = 1 if extend1 else 0
        e2 = 1 if extend2 else 0
        if len(coords) != 6:
            raise CapyPDFException('Coords array must hold exactly 6 doubles.')
        self._as_parameter_ = None
        t3s = ctypes.c_void_p()
        check_error(libfile.capy_type3_shading_new(cs.value, to_array(ctypes.c_double, coords)[0], funcid, e1, e2, ctypes.pointer(t3s)))
        self._as_parameter_ = t3s

    def __del__(self):
        check_error(libfile.capy_type3_shading_destroy(self))


class Type4Shading:
    def __init__(self, cs, minx, miny, maxx, maxy):
        t4s = ctypes.c_void_p()
        check_error(libfile.capy_type4_shading_new(cs.value,
                    minx, miny, maxx, maxy, ctypes.pointer(t4s)))
        self._as_parameter_ = t4s

    def __del__(self):
        check_error(libfile.capy_type4_shading_destroy(self))

    def add_triangle(self, coords, colors):
        if len(coords) != 6:
            raise CapyPDFException('Must have exactly 6 floats.')
        if len(colors) != 3:
            raise CapyPDFException('Must have exactly 3 colors.')
        colorptrs = [x.get_underlying() for x in colors]
        check_error(libfile.capy_type4_shading_add_triangle(self,
                    to_array(ctypes.c_double, coords)[0],
                    to_array(ctypes.c_void_p, colorptrs)[0]))

    def extend(self, flag, coords, color):
        if flag == 1 or flag == 2:
            if not isinstance(color, Color):
                raise CapyPDFException('Color argument not a color object.')
            if len(coords) != 2:
                raise CapyPDFException('Must have exactly 2 floats.')
            check_error(libfile.capy_type4_shading_extend(self,
                        flag,
                        to_array(ctypes.c_double, coords)[0],
                        color))
        else:
            raise CapyPDFException(f'Bad flag value {flag}')


class Type6Shading:
    def __init__(self, cs, minx, miny, maxx, maxy):
        t6s = ctypes.c_void_p()
        check_error(libfile.capy_type6_shading_new(cs.value,
                    minx, miny, maxx, maxy, ctypes.pointer(t6s)))
        self._as_parameter_ = t6s

    def __del__(self):
        check_error(libfile.capy_type6_shading_destroy(self))

    def add_patch(self, coords, colors):
        if len(coords) != 24:
            raise CapyPDFException('Must have exactly 24 floats.')
        if len(colors) != 4:
            raise CapyPDFException('Must have exactly 4 colors.')
        colorptrs = [x.get_underlying() for x in colors]
        check_error(libfile.capy_type6_shading_add_patch(self,
                    to_array(ctypes.c_double, coords)[0],
                    to_array(ctypes.c_void_p, colorptrs)[0]))

    def extend(self, flag, coords, colors):
        if flag == 1 or flag == 2 or flag == 3:
            if len(coords) != 16:
                raise CapyPDFException('Must have exactly 16 floats.')
            if len(colors) != 2:
                raise CapyPDFException('Must have exactly 2 colors.')
            colorptrs = [x.get_underlying() for x in colors]
            check_error(libfile.capy_type6_shading_extend(self,
                        flag,
                        to_array(ctypes.c_double, coords)[0],
                        to_array(ctypes.c_void_p, colorptrs)[0]))
        else:
            raise CapyPDFException(f'Bad flag value {flag}')

class Annotation:
    def __init__(self, handle):
        self._as_parameter_ = handle

    def __del__(self):
        check_error(libfile.capy_annotation_destroy(self))

    def set_rectangle(self, x1, y1, x2, y2):
        check_error(libfile.capy_annotation_set_rectangle(self, x1, y1, x2, y2))

    def set_flags(self, flags):
        if not isinstance(flags, AnnotationFlag):
            raise CapyPDFException('Flag argument is not an AnnotationFlag.')
        check_error(libfile.capy_annotation_set_flags(self, flags.value ))

    @classmethod
    def new_text_annotation(cls, text):
        ta = ctypes.c_void_p()
        check_error(libfile.capy_text_annotation_new(text.encode('utf-8'), ctypes.pointer(ta)))
        return Annotation(ta)

    @classmethod
    def new_file_attachment_annotation(cls, fid):
        ta = ctypes.c_void_p()
        check_error(libfile.capy_file_attachment_annotation_new(fid, ctypes.pointer(ta)))
        return Annotation(ta)

    @classmethod
    def new_printers_mark_annotation(cls, fid):
        ta = ctypes.c_void_p()
        check_error(libfile.capy_printers_mark_annotation_new(fid, ctypes.pointer(ta)))
        return Annotation(ta)

class StructItemExtraData:
    def __init__(self):
        ed = ctypes.c_void_p()
        check_error(libfile.capy_struct_item_extra_data_new(ctypes.pointer(ed)))
        self._as_parameter_ = ed

    def __del__(self):
        check_error(libfile.capy_struct_item_extra_data_destroy(self))

    def set_t(self, T):
        chars = T.encode('UTF-8')
        check_error(libfile.capy_struct_item_extra_data_set_t(self, chars))

    def set_lang(self, lang):
        chars = lang.encode('UTF-8')
        check_error(libfile.capy_struct_item_extra_data_set_lang(self, chars))

    def set_alt(self, alt):
        chars = alt.encode('UTF-8')
        check_error(libfile.capy_struct_item_extra_data_set_alt(self, chars))

    def set_actual_text(self, actual):
        chars = actual.encode('UTF-8')
        check_error(libfile.capy_struct_item_extra_data_set_actual_text(self, chars))

class ImagePdfProperties:
    def __init__(self):
        ed = ctypes.c_void_p()
        check_error(libfile.capy_image_pdf_properties_new(ctypes.pointer(ed)))
        self._as_parameter_ = ed

    def __del__(self):
        check_error(libfile.capy_image_pdf_properties_destroy(self))

    def set_mask(self, boolval):
        intval = 1 if boolval else 0
        check_error(libfile.capy_image_pdf_properties_set_mask(self, intval))

    def set_interpolate(self, ival):
        if not isinstance(ival, ImageInterpolation):
            raise CapyPDFException('Argument must be image interpolation enum.')
        check_error(libfile.capy_image_lpdf_properties_set_interpolate(self, ival.value))

class Destination:
    def __init__(self):
        d = ctypes.c_void_p()
        check_error(libfile.capy_destination_new(ctypes.pointer(d)))
        self._as_parameter_ = d

    def __del__(self):
        check_error(libfile.capy_destination_destroy(self))

    def set_page_fit(self, page_num):
        check_error(libfile.capy_destination_set_page_fit(self, page_num))

    def set_page_xyz(self, page_number, x=None, y=None, z=None):
        xptr = self.double_to_cptr(x)
        yptr = self.double_to_cptr(y)
        zptr = self.double_to_cptr(z)
        check_error(libfile.capy_destination_set_page_xyz(self, page_number, xptr, yptr, zptr))

    def double_to_cptr(self, value):
        if value is None:
            return None
        d = ctypes.c_double(value)
        cptr = ctypes.pointer(d)
        return cptr

class Outline:
    def __init__(self):
        o = ctypes.c_void_p()
        check_error(libfile.capy_outline_new(ctypes.pointer(o)))
        self._as_parameter_ = o

    def __del__(self):
        check_error(libfile.capy_outline_destroy(self))

    def set_title(self, title):
        ctitle = title.encode('UTF-8')
        check_error(libfile.capy_outline_set_title(self, ctitle))

    def set_destination(self, dest):
        if not isinstance(dest, Destination):
            raise CapyPDFException('Argument must be a destination object.')
        check_error(libfile.capy_outline_set_destination(self, dest))

    def set_rgb(self, r, g, b):
        check_error(libfile.capy_outline_set_rgb(self, r, g, b))

    def set_f(self, f):
        check_error(libfile.capy_outline_set_f(self, f))

    def set_parent(self, parent):
        if not isinstance(parent, OutlineId):
            raise CapyPDFException('Argument must be a parent id.')
        check_error(libfile.capy_outline_set_parent(self, parent))
