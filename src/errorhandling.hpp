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
    MMapFail,
    InvalidSubfont,
    FontNotSpecified,
    InvalidBBox,
    TooManyGlyphsUsed,
    VariationsNotSupported,
    VariationNotFound,
    // When you add an error code here, also add the string representation in the .cpp file.
    NumErrors,
};

const char *error_text(ErrorCode ec) noexcept;

#if defined(CAPY_USE_EXCEPTIONS)
// All errors throw exceptions

template<typename T> struct rvoe {
    T v;

    rvoe(const T &in) : v{in} {}
    rvoe(T &&in) : v{std::move(in)} {}

    T &&value() { return std::move(v); }
    operator bool() const { return true; }
    T *operator->() { return &v; }
    const T *operator->() const { return &v; }
    T &operator*() { return v; }
    const T &operator*() const { return v; }

    ErrorCode error() const { return ErrorCode::NoError; }
};

#define RETERR(code) throw(ErrorCode::code)

#define RETOK                                                                                      \
    return NoReturnValue {}

#define ERC(varname, func)                                                                         \
    auto varname##_shell = func;                                                                   \
    auto &varname = varname##_shell.v;

// For void.

#define ERCV(func) func

#else
// All errors are returned as std::unexpecteds and propagated manually.

// This error exists solely so you can put a breakpoint in it.
inline std::unexpected<ErrorCode> create_error(ErrorCode code) { return std::unexpected(code); }

#define RETERR(code) return create_error(ErrorCode::code)

#define RETOK                                                                                      \
    return NoReturnValue {}

// Return value or error.
// Would be nice to tag  [[nodiscard]] but it does not seem to be possible.
template<typename T> using rvoe = std::expected<T, ErrorCode>;

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

#endif

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
