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

#include "pdfcommon.hpp"
#include <errorhandling.hpp>
#include <cstdint>
#include <string>
#include <optional>
#include <variant>
#include <expected>
#include <filesystem>

namespace capypdf {

rvoe<RasterImage> load_image_file(const std::filesystem::path &fname);

rvoe<jpg_image> load_jpg(const std::filesystem::path &fname);

} // namespace capypdf
