// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#pragma once

#include <cstdint>
#include <stack>
#include <string>
#include <vector>

namespace capypdf::internal {

enum class ContainerType { Array, Dictionary };

struct FormatState {
    std::string indent;
    int array_elems_per_line;
    int num_entries;
};

struct FormatStash {
    ContainerType type;
    FormatState params;
};

class u8string;
class asciistring;

class ObjectFormatter {
public:
    explicit ObjectFormatter(std::string_view base_indent = {});

    void begin_array(int32_t array_elems_per_line = 10000);
    void begin_dict();
    void end_array();
    void end_dict();

    void add_token_pair(const char *t1, const char *t2);

    void add_token(const char *raw_text);
    void add_token(std::string_view raw_text);
    void add_token(int32_t number);
    void add_token(uint32_t number);
    void add_token(double number);

    void add_object_ref(int32_t onum);
    void add_pdfstring(const asciistring &str);

    std::string steal();

    const std::string &current_indent() const { return state.indent; }
    size_t depth() const { return stack.size(); }

private:
    void added_item();
    void check_indent();

    void do_pop(ContainerType ctype);
    void do_push(ContainerType ctype);

    FormatState state;
    std::stack<FormatStash, std::vector<FormatStash>> stack;
    std::string buf;
    std::back_insert_iterator<std::string> app;
};

} // namespace capypdf::internal
