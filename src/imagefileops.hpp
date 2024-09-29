// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#pragma once

#include "pdfcommon.hpp"
#include <errorhandling.hpp>
#include <cstdint>
#include <string>
#include <optional>
#include <variant>
#include <expected>
#include <filesystem>

namespace capypdf::internal {

rvoe<RasterImage> load_image_file(const std::filesystem::path &fname);

rvoe<jpg_image> load_jpg(const std::filesystem::path &fname);

} // namespace capypdf::internal
