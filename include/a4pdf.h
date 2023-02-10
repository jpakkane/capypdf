/*
 * Copyright 2023 Jussi Pakkanen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum A4PDF_Builtin_Fonts {
    A4PDF_FONT_TIMES_ROMAN,
    A4PDF_FONT_HELVETICA,
    A4PDF_FONT_COURIER,
    A4PDF_FONT_TIMES_ROMAN_BOLD,
    A4PDF_FONT_HELVETICA_BOLD,
    A4PDF_FONT_COURIER_BOLD,
    A4PDF_FONT_TIMES_ROMAN_ITALIC,
    A4PDF_FONT_HELVETICA_OBLIQUE,
    A4PDF_FONT_COURIER_OBLIQUE,
};

enum A4PDF_Colorspace { A4PDF_DEVICE_RGB, A4PDF_DEVICE_GRAY, A4PDF_DEVICE_CMYK };

enum A4PDF_Rendering_Intent {
    A4PDF_RI_RELATIVE_COLORIMETRIC,
    A4PDF_RI_ABSOLUTE_COLORIMETRIC,
    A4PDF_RI_SATURATION,
    A4PDF_RI_PERCEPTUAL
};

enum A4PDF_Text_Rendering_Mode {
    A4PDF_Text_Fill,
    A4PDF_Text_Stroke,
    A4PDF_Text_Fill_Stroke,
    A4PDF_Text_Invisible,
    A4PDF_Text_Fill_Clip,
    A4PDF_Text_Stroke_Clip,
    A4PDF_Text_Fill_Stroke_Clip,
    A4PDF_Text_Clip,
};

enum A4PDF_Blend_Mode {
    A4PDF_BM_NORMAL,
    A4PDF_BM_MULTIPLY,
    A4PDF_BM_SCREEN,
    A4PDF_BM_OVERLAY,
    A4PDF_BM_DARKEN,
    A4PDF_BM_LIGHTEN,
    A4PDF_BM_COLORDODGE,
    A4PDF_BM_COLORBURN,
    A4PDF_BM_HARDLIGHT,
    A4PDF_BM_SOFTLIGHT,
    A4PDF_BM_DIFFERENCE,
    A4PDF_BM_EXCLUSION,
    A4PDF_BM_HUE,
    A4PDF_BM_SATURATION,
    A4PDF_BM_COLOR,
    A4PDF_BM_LUMINOSITY,
};

enum A4PDF_Line_Cap { A4PDF_Butt_Cap, A4PDF_Round_Cap, A4PDF_Projection_Square_Cap };

enum A4PDF_Line_Join {
    A4PDF_Miter_Join,
    A4PDF_Round_Join,
    A4PDF_Bevel_Join,
};

typedef struct _A4PDF_Options A4PDF_Options;
typedef struct _A4PDF_Generator A4PDF_Generator;

A4PDF_Options *a4pdf_options_create();

void a4pdf_options_destroy(A4PDF_Options *);

int32_t a4pdf_options_set_title(A4PDF_Options *opt, const char *utf8_title);

A4PDF_Generator *a4pdf_generator_create(const char *filename, const A4PDF_Options *options);

void a4pdf_generator_destroy(A4PDF_Generator *);

void a4pdf_generator_new_page(A4PDF_Generator *);

const char *a4pdf_error_message(int32_t error_code);

#ifdef __cplusplus
}
#endif
