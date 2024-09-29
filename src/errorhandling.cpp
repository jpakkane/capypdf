// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <errorhandling.hpp>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace capypdf::internal {

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
"Input image color profile could not be determined.",
"Output intent identifier missing.",
"Draw state end mismatch.",
"OCG not used on this page.",
"Unsupported TIFF image.",
"Used object with a drawing context that was not used to create it.",
"MediaBox is missing.",
"Image used as a mask has an alpha channel.",
"Font does not have the requested glyph.",
"Invalid image size.",
"Missing pixel data.",
"Color spaces are not of the same type.",
"Boolean integer value must be 0 or 1.",
"Gradient must start with a full patch.",
"Object created for one document used with a different document",
"Argument must be ASCII.",
"File does not exist.",
"Annotation is missing location rectangle.",
"Argument must not start with a slash or be empty.",
"Nesting marked content is forbidden.",
"Rolemap entry is already defined.",
"There can be only one call to the write function.",
"Unspecified color profile error.",
"Image is not in format required by output settings.",
"Page reference points to a non-existing page.",
"Title is empty.",
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

} // namespace capypdf::internal
