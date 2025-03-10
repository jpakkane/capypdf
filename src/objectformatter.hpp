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

    ObjectFormatter(const ObjectFormatter &o) = delete;
    ObjectFormatter(ObjectFormatter &&o)
        : state{std::move(o.state)}, stack{std::move(o.stack)}, buf{std::move(o.buf)},
          app{std::back_inserter(buf)} {}

    ObjectFormatter &operator=(ObjectFormatter &&o) {
        state = std::move(o.state);
        stack = std::move(o.stack);
        buf = std::move(o.buf);
        app = std::back_inserter(buf);
        return *this;
    }
    ObjectFormatter &operator=(const ObjectFormatter &o) = delete;

    void begin_array(int32_t array_elems_per_line = 10000);
    void begin_dict();
    void end_array();
    void end_dict();

    template<typename T> void add_array(const T &arr) {
        begin_array();
        for(const auto &i : arr) {
            add_token(i);
        }
        end_array();
    }

    void add_token_pair(const char *t1, const char *t2);
    template<typename T> void add_token_pair(std::string_view key, T &&vtype) {
        add_token(key);
        add_token(vtype);
    }

    void add_token(const char *raw_text);
    void add_token(std::string_view raw_text);
    void add_token(int32_t number);
    void add_token(uint32_t number);
    void add_token(size_t number);
    void add_token(double number);

    void add_token_with_slash(const char *name);
    void add_token_with_slash(std::string_view name);
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
