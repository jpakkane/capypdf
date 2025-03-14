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
    do_push(ContainerType::Array);
    state.array_elems_per_line = max_element;
}

void ObjectFormatter::begin_dict() {
    do_push(ContainerType::Dictionary);
    state.array_elems_per_line = 0;
}

void ObjectFormatter::end_array() { do_pop(ContainerType::Array); }
void ObjectFormatter::end_dict() { do_pop(ContainerType::Dictionary); }

void ObjectFormatter::do_pop(ContainerType ctype) {
    if(stack.empty()) {
        fprintf(stderr, "Stack underrun\n");
        std::abort();
    }
    if(stack.top().type != ctype) {
        fprintf(stderr, "Pop type mismatch.\n");
        std::abort();
    }
    state = std::move(stack.top().params);
    stack.pop();
    if(!buf.empty() && buf.back() == '\n') {
        buf += state.indent;
    }
    buf += ctype == ContainerType::Dictionary ? ">>" : "]";
    added_item();
}

void ObjectFormatter::do_push(ContainerType ctype) {
    check_indent();
    stack.push(FormatStash{ctype, state});
    state.indent += "  ";
    state.num_entries = 0;
    buf += ctype == ContainerType::Dictionary ? "<<\n" : "[\n";
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

void ObjectFormatter::add_token(size_t number) {
    check_indent();
    std::format_to(app, "{}", number);
    added_item();
}

void ObjectFormatter::add_token_with_slash(const char *name) {
    check_indent();
    assert(name[0] != '/');
    std::format_to(app, "/{}", name);
    added_item();
}

void ObjectFormatter::add_token_with_slash(std::string_view name) {
    check_indent();
    assert(name[0] != '/');
    std::format_to(app, "/{}", name);
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
    check_indent();
    std::format_to(app, "({})", str.c_str());
    added_item();
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
