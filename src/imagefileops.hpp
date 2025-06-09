// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#pragma once

#include "pdfcommon.hpp"
#include <errorhandling.hpp>
#include <stdint.h>
#include <pystd2025.hpp>

namespace capypdf::internal {

rvoe<RasterImage> load_image_file(const pystd2025::Path &fname);
rvoe<RasterImage> load_image_from_memory(const char *buf, int64_t bufsize);

} // namespace capypdf::internal
