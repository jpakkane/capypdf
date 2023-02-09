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
    FONT_TIMES_ROMAN,
    FONT_HELVETICA,
    FONT_COURIER,
    FONT_TIMES_ROMAN_BOLD,
    FONT_HELVETICA_BOLD,
    FONT_COURIER_BOLD,
    FONT_TIMES_ROMAN_ITALIC,
    FONT_HELVETICA_OBLIQUE,
    FONT_COURIER_OBLIQUE,
};

enum A4PDF_Colorspace { PDF_DEVICE_RGB, PDF_DEVICE_GRAY, PDF_DEVICE_CMYK };

enum RenderingIntent {
    RI_RELATIVE_COLORIMETRIC,
    RI_ABSOLUTE_COLORIMETRIC,
    RI_SATURATION,
    RI_PERCEPTUAL
};

enum A4PDF_Blend_Mode {
    BM_NORMAL,
    BM_MULTIPLY,
    BM_SCREEN,
    BM_OVERLAY,
    BM_DARKEN,
    BM_LIGHTEN,
    BM_COLORDODGE,
    BM_COLORBURN,
    BM_HARDLIGHT,
    BM_SOFTLIGHT,
    BM_DIFFERENCE,
    BM_EXCLUSION,
    BM_HUE,
    BM_SATURATION,
    BM_COLOR,
    BM_LUMINOSITY,
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
