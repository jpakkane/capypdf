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

#include <errorhandling.hpp>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace capypdf {

// clang-format off

const std::array<const char *, (std::size_t)ErrorCode::NumErrors> error_texts{
"No error.",
"Unexpected error, the real error message should be in stdout or stderr.",
"Invalid index.",
"Negative line width.",
"No pages defined.",
"Color component out of range.",
"Bad ID number.",
"Enum out of range.",
"Negative dash array element.",
"Flatness value out of bounds.",
"Array has zero length.",
"Could not open file.",
"Writing to file failed.",
"Required argument is NULL.",
"Index is negative.",
"Index out of bounds.",
"Invalid UTF-8 string.",
"Incorrect amoung of color channels for this colorspace.",
"Invalid draw context type for this operation.",
"Failed to load data from file.",
"Invalid ICC profile data.",
"Compression failure.",
"FreeType error.",
"Unreachable code.",
"Pattern can not be used in this operation.",
"Iconv error.",
"Builtin fonts can not be used in this operation.",
"Output CMYK profile not defined.",
"Unsupported file format.",
"Only monochrome colormap images supported.",
"Malformed font file.",
"EMC called even though no marked content block is active.",
"Marked content not closed.",
"Annotations (including widgets) can only be used once.",
"Structures can only be used once.",
"Operation prohibited by current output intent.",
"Output color profile not defined.",
"Output intent identifier missing.",
"Draw state end mismatch.",
"URI must be ASCII.",
"OCG not used on this page.",
};

// clang-format on

const char *error_text(ErrorCode ec) noexcept {
    const int index = (int32_t)ec;
    if(index < 0 || (std::size_t)index >= error_texts.size()) {
        return "Invalid error code.";
    }
    return error_texts[index];
}

[[noreturn]] void abortif(ErrorCode ec) noexcept {
    fprintf(stderr, "%s\n", error_text(ec));
    std::abort();
}

} // namespace capypdf
