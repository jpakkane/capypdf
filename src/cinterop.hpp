// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <capypdf.h>
#include <memory> // std::hash is in utility only since c++26
#include <cstdint>

#define DEF_BASIC_OPERATORS(TNAME)                                                                 \
    inline bool operator==(const TNAME &object_number_1, const TNAME &object_number_2) noexcept {  \
        return object_number_1.id == object_number_2.id;                                           \
    }                                                                                              \
                                                                                                   \
    inline std::strong_ordering operator<=>(const TNAME &object_number_1,                          \
                                            const TNAME &object_number_2) noexcept {               \
        return object_number_1.id <=> object_number_2.id;                                          \
    }                                                                                              \
                                                                                                   \
    template<> struct std::hash<TNAME> {                                                           \
        std::size_t operator()(const TNAME &tobj) const noexcept {                                 \
            return std::hash<decltype(tobj.id)>{}(tobj.id);                                        \
        }                                                                                          \
    }

DEF_BASIC_OPERATORS(CapyPDF_ImageId);

DEF_BASIC_OPERATORS(CapyPDF_FontId);

DEF_BASIC_OPERATORS(CapyPDF_IccColorSpaceId);

DEF_BASIC_OPERATORS(CapyPDF_FormXObjectId);

DEF_BASIC_OPERATORS(CapyPDF_FormWidgetId);

DEF_BASIC_OPERATORS(CapyPDF_AnnotationId);

DEF_BASIC_OPERATORS(CapyPDF_StructureItemId);

DEF_BASIC_OPERATORS(CapyPDF_OptionalContentGroupId);

DEF_BASIC_OPERATORS(CapyPDF_TransparencyGroupId);

// Helpers so we can use static_cast instead of reinterpret_cast.

struct _capyPDF_DocumentProperties {};

struct _capyPDF_PageProperties {};

struct _capyPDF_TransparencyGroupProperties {};

struct _capyPDF_ImagePdfProperties {};

struct _capyPDF_FontProperties {};

struct _capyPDF_DrawContext {};

struct _capyPDF_Generator {};

struct _capyPDF_GraphicsState {};
