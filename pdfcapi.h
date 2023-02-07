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

#ifdef __cplusplus
extern "C" {
#endif

enum BuiltinFonts {
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

enum PdfColorSpace { PDF_DEVICE_RGB, PDF_DEVICE_GRAY, PDF_DEVICE_CMYK };

enum RenderingIntent {
    RI_RELATIVE_COLORIMETRIC,
    RI_ABSOLUTE_COLORIMETRIC,
    RI_SATURATION,
    RI_PERCEPTUAL
};

enum BlendMode {
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

typedef struct _PdfOptions PdfOptions;
typedef struct _PdfGenerator PdfGenerator;
typedef struct _PdfPage2 PdfPage2; // Kludge to avoid name clashes for now.

PdfOptions *pdf_options_create();

void pdf_options_destroy(PdfOptions *);

PdfGenerator *pdf_generator_create(const char *filename, PdfOptions *options);

void pdf_generator_destroy(PdfGenerator *);

PdfPage2 *pdf_generator_new_page(PdfGenerator *);

void pdf_page_destroy(PdfPage2 *);

#ifdef __cplusplus
}
#endif
