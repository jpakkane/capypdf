// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#include <objectformatter.hpp>
#include <pdfcommon.hpp>

#include <format>
#include <cassert>

namespace capypdf::internal {

ObjectFormatter::ObjectFormatter(std::string_view base_indent)
    : state{std::string{base_indent}, 0, 0}, app{std::back_inserter(buf)} {}

void ObjectFormatter::begin_array(int32_t max_element) {
    check_indent();
    stack.push(FormatStash{ContainerType::Array, state});
    state.indent += "  ";
    state.num_entries = 0;
    state.array_elems_per_line = max_element;
    buf += "[\n";
}

void ObjectFormatter::end_array() {
    if(stack.empty()) {
        fprintf(stderr, "Stack underrun\n");
        std::abort();
    }
    if(stack.top().type != ContainerType::Array) {
        fprintf(stderr, "Was expecting array.\n");
    }
    state = std::move(stack.top().params);
    stack.pop();
    if(!buf.empty() && buf.back() == '\n') {
        buf += state.indent;
    }
    buf += "]";
    added_item();
}

void ObjectFormatter::begin_dict() {
    check_indent();
    stack.push(FormatStash{ContainerType::Dictionary, state});
    state.indent += "  ";
    state.num_entries = 0;
    buf += "<<\n";
}

void ObjectFormatter::end_dict() {
    if(stack.empty()) {
        fprintf(stderr, "Stack underrun\n");
        std::abort();
    }
    if(stack.top().type != ContainerType::Dictionary) {
        fprintf(stderr, "Was expecting dictionary.\n");
    }
    state = std::move(stack.top().params);
    stack.pop();
    if(!buf.empty() && buf.back() == '\n') {
        buf += state.indent;
    }
    buf += ">>";
    added_item();
}

void ObjectFormatter::add_token_pair(const char *t1, const char *t2) {
    add_token(t1);
    add_token(t2);
}

void ObjectFormatter::add_token(const char *raw_text) {
    check_indent();
    buf += raw_text;
    added_item();
}

void ObjectFormatter::add_token(std::string_view raw_text) {
    check_indent();
    buf += raw_text;
    added_item();
}

void ObjectFormatter::add_token(int32_t number) {
    check_indent();
    std::format_to(app, "{}", number);
    added_item();
}

void ObjectFormatter::add_token(uint32_t number) {
    check_indent();
    std::format_to(app, "{}", number);
    added_item();
}

void ObjectFormatter::add_token(double number) {
    check_indent();
    std::format_to(app, "{:f}", number);
    added_item();
}

void ObjectFormatter::add_object_ref(int32_t onum) {
    check_indent();
    std::format_to(app, "{} 0 R", onum);
    added_item();
}

void ObjectFormatter::check_indent() {
    if(state.num_entries == 0) {
        buf += state.indent;
    }
}

void ObjectFormatter::add_pdfstring(const asciistring &str) {
    // FIXME: add quoting for special characters.
    std::format_to(app, "({})", str.c_str());
}

void ObjectFormatter::added_item() {
    ++state.num_entries;
    if(stack.empty()) {
        return;
    }
    if(stack.top().type == ContainerType::Array) {
        if(state.num_entries >= state.array_elems_per_line) {
            buf += "\n";
            state.num_entries = 0;
        } else {
            buf += " ";
        }
    } else if(stack.top().type == ContainerType::Dictionary) {
        if(state.num_entries >= 2) {
            buf += "\n";
            state.num_entries = 0;
        } else {
            buf += " ";
        }
    } else {
        std::abort();
    }
}

std::string ObjectFormatter::steal() {
    assert(stack.empty());
    if(buf.empty() || (!buf.empty() && buf.back() != '\n')) {
        buf += '\n';
    }
    return std::move(buf);
}

} // namespace capypdf::internal
