// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <pdfcommon.hpp>
#include <cstdint>
#include <string>

namespace capypdf::internal {

enum class XMLState : uint8_t {
    Basic,
    TagOpen,
};

class XMLFormatter {
public:
    XMLFormatter();

    void add_text_unchecked(const char *txt);

    void start_tag(const char *name);
    void add_tag_attribute(const char *name, std::string_view value);
    void finish_tag();
    void finish_standalone_tag();
    void add_end_tag(const char *fname);

    void add_content(const u8string &content);
    void add_content(const std::string_view &content);

    std::string steal();

private:
    std::string output;
    std::string indent;
    XMLState state = XMLState::Basic;
};

} // namespace capypdf::internal
