// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#pragma once

#include <cstdint>
#include <expected>

namespace capypdf::internal {

enum class ErrorCode : int32_t {
    NoError,
    DynamicError,
    InvalidIndex,
    NegativeLineWidth,
    NoPages,
    ColorOutOfRange,
    BadId,
    BadEnum,
    NegativeDash,
    InvalidFlatness,

    ZeroLengthArray,
    CouldNotOpenFile,
    FileWriteError,
    ArgIsNull,
    IndexIsNegative,
    IndexOutOfBounds,
    BadUtf8,
    IncorrectColorChannelCount,
    InvalidDrawContextType,
    FileReadError,

    InvalidICCProfile,
    CompressionFailure,
    FreeTypeError,
    Unreachable,
    PatternNotAccepted,
    IconvError,
    BuiltinFontNotSupported,
    NoCmykProfile,
    UnsupportedFormat,
    NonBWColormap,

    MalformedFontFile,
    EmcOnEmpty,
    UnclosedMarkedContent,
    AnnotationReuse,
    StructureReuse,
    BadOperationForIntent,
    OutputProfileMissing,
    InputProfileUnknown,
    MissingIntentIdentifier,
    DrawStateEndMismatch,

    UnusedOcg,
    UnsupportedTIFF,
    WrongDrawContext,
    MissingMediabox,
    MaskAndAlpha,
    MissingGlyph,
    InvalidImageSize,
    MissingPixels,
    ColorspaceMismatch,
    BadBoolean,

    BadStripStart,
    IncorrectDocumentForObject,
    NotASCII,
    FileDoesNotExist,
    AnnotationMissingRect,
    SlashStart,
    NestedBMC,
    RoleAlreadyDefined,
    WritingTwice,
    ProfileProblem,

    ImageFormatNotPermitted,
    InvalidPageNumber,
    NonSequentialPageNumber,
    EmptyTitle,
    WrongDCForTransp,
    WrongDCForMatrix,
    EmptyFunctionList,
    IncorrectShadingType,
    IncorrectFunctionType,
    IncorrectAnnotationType,

    DuplicateName,
    EmbeddedNullInString,
    InvalidBufsize,
    // When you add an error code here, also add the string representation in the .cpp file.
    NumErrors,
};

const char *error_text(ErrorCode ec) noexcept;

[[noreturn]] void printandabort(ErrorCode ec) noexcept;

#define RETERR(code) return std::unexpected(ErrorCode::code)

#define RETOK                                                                                      \
    return NoReturnValue {}

// Return value or error.
// Would be nice to tag  [[nodiscard]] but it does not seem to be possible.
template<typename T> using rvoe = std::expected<T, ErrorCode>;

struct NoReturnValue {};

} // namespace capypdf::internal

#define CHECK_INDEXNESS(ind, container)                                                            \
    if((ind < 0) || ((size_t)ind >= container.size())) {                                           \
        RETERR(BadId);                                                                             \
    }

#define CHECK_INDEXNESS_V(ind, container)                                                          \
    if((ind < 0) || ((size_t)ind >= container.size())) {                                           \
        RETERR(BadId);                                                                             \
    }

#define CHECK_ENUM(v, max_enum_val)                                                                \
    if((int)v < 0 || ((int)v > (int)max_enum_val)) {                                               \
        RETERR(BadEnum);                                                                           \
    }

#define ERC(varname, func)                                                                         \
    auto varname##_variant = func;                                                                 \
    if(!(varname##_variant)) {                                                                     \
        return std::unexpected(varname##_variant.error());                                         \
    }                                                                                              \
    auto &varname = varname##_variant.value();

// For void.

#define ERCV(func)                                                                                 \
    {                                                                                              \
        auto placeholder_name_variant = func;                                                      \
        if(!(placeholder_name_variant)) {                                                          \
            return std::unexpected(placeholder_name_variant.error());                              \
        }                                                                                          \
    }
