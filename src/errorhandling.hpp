/*
 * Copyright 2022-2023 Jussi Pakkanen
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

#include <cstdint>
#include <expected>

namespace A4PDF {

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
    BadOperationForIntent,
    OutputProfileMissing,
    MissingIntentIdentifier,
    // When you add an error code here, also add the string representation in the .cpp file.
    NumErrors,
};

const char *error_text(ErrorCode ec) noexcept;

[[noreturn]] void printandabort(ErrorCode ec) noexcept;

#define ABORTIF(ec)                                                                                \
    if(ec != ErrorCode::NoError) {                                                                 \
        printandabort(ec);                                                                         \
    }

// Return value or error.
// Would be nice to tag  [[nodiscard]] but it does not seem to be possible.
template<typename T> using rvoe = std::expected<T, ErrorCode>;

struct NoReturnValue {};

} // namespace A4PDF

#define CHECK_COLORCOMPONENT(c)                                                                    \
    if(c < 0 || c > 1) {                                                                           \
        return ErrorCode::ColorOutOfRange;                                                         \
    }

#define CHECK_INDEXNESS(ind, container)                                                            \
    if((ind < 0) || ((size_t)ind >= container.size())) {                                           \
        return ErrorCode::BadId;                                                                   \
    }

#define CHECK_INDEXNESS_V(ind, container)                                                          \
    if((ind < 0) || ((size_t)ind >= container.size())) {                                           \
        return std::unexpected(ErrorCode::BadId);                                                  \
    }

#define CHECK_ENUM(v, max_enum_val)                                                                \
    if((int)v < 0 || ((int)v > (int)max_enum_val)) {                                               \
        return ErrorCode::BadEnum;                                                                 \
    }

#define CHECK_NULL(x)                                                                              \
    if(x == nullptr) {                                                                             \
        return (A4PDF_EC)ErrorCode::ArgIsNull;                                                     \
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

#define RETERR(code) return std::unexpected(ErrorCode::code)
