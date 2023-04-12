/*
 * Copyright 2022 Jussi Pakkanen
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

#include <errorhandling.hpp>
#include <cstdint>
#include <string>
#include <optional>
#include <variant>
#include <expected>

namespace A4PDF {

struct mono_image {
    int32_t w;
    int32_t h;
    std::string pixels;
    std::optional<std::string> alpha;
};

struct rgb_image {
    int32_t w;
    int32_t h;
    std::string pixels;
    std::optional<std::string> alpha;
};

struct gray_image {
    int32_t w;
    int32_t h;
    std::string pixels;
    std::optional<std::string> alpha;
};

struct jpg_image {
    int32_t w;
    int32_t h;
    std::string file_contents;
};

struct cmyk_image {
    int32_t w;
    int32_t h;
    std::string pixels;
    std::optional<std::string> icc;
    std::optional<std::string> alpha;
};

typedef std::variant<mono_image, gray_image, rgb_image, cmyk_image> RasterImage;

std::expected<RasterImage, ErrorCode> load_image_file(const char *fname);

std::expected<jpg_image, ErrorCode> load_jpg(const char *fname);

} // namespace A4PDF
