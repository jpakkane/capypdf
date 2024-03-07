// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#pragma once

#include <pdfdocument.hpp>

namespace capypdf {

class PdfWriter {
public:
    explicit PdfWriter(PdfDocument &doc);
    rvoe<NoReturnValue> write_to_file(const std::filesystem::path &ofilename);

private:
    PdfDocument &doc;
};

} // namespace capypdf
