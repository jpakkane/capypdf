// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#pragma once

import std;

#include "pdfcommon.hpp"
#include <errorhandling.hpp>
#include <cstdint>

namespace capypdf::internal {

rvoe<RasterImage> load_image_file(const char *fname);
rvoe<RasterImage> load_image_from_memory(const char *buf, int64_t bufsize);

} // namespace capypdf::internal
