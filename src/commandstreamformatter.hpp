// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <errorhandling.hpp>

#include <string>
#include <stdint.h>

#include <pystd2025.hpp>

namespace capypdf::internal {

enum class DrawStateType : uint8_t {
    MarkedContent,
    SaveState,
    Text,
    Dictionary,
};

class CommandStreamFormatter {

public:
    CommandStreamFormatter();
    // explicit CommandStreamFormatter(std::string_view start_indent);

    void append(std::string_view line_of_text);
    void append(const pystd2025::CString &line_of_text) { append(line_of_text.view()); }
    void append(pystd2025::CStringView line) { append(std::string_view(line.data(), line.size())); }
    void append_raw(std::string_view raw) {
        pystd2025::CStringView r(raw.data(), raw.size());
        append_raw(r);
    }
    void append_raw(pystd2025::CStringView raw) { buf += raw; }
    void append_raw(const pystd2025::CString &raw) { append_raw(raw.view()); }
    void append_raw(const char *raw) { buf += raw; }
    void append_command(std::string_view arg, const char *command);
    void append_command(double arg, const char *command);
    void append_command(double arg1, double arg2, const char *command);
    void append_command(double arg1, double arg2, double arg3, const char *command);
    void append_command(double arg1, double arg2, double arg3, double arg4, const char *command);
    void append_command(int32_t arg, const char *command);
    void append_indent() { buf += lead; }

    void append_dict_entry(const char *key, std::string_view value);
    void append_dict_entry(const char *key, int32_t value);
    void append_dict_entry_string(const char *key, const char *value);

    rvoe<NoReturnValue> BT();
    rvoe<NoReturnValue> ET();

    rvoe<NoReturnValue> q();
    rvoe<NoReturnValue> Q();

    rvoe<NoReturnValue> BMC();
    rvoe<NoReturnValue> EMC();

    const pystd2025::CString &contents() const { return buf; }

    void clear();

    rvoe<std::string> steal();

    rvoe<NoReturnValue> indent(DrawStateType stype);
    rvoe<NoReturnValue> dedent(DrawStateType stype);

    size_t size() const { return buf.size(); }

    size_t marked_content_depth() const;

    bool has_unclosed_state() const { return !stack.is_empty(); }

private:
    bool has_state(DrawStateType stype);

    pystd2025::CString lead;
    pystd2025::Vector<DrawStateType> stack;
    pystd2025::CString buf;
};

} // namespace capypdf::internal
