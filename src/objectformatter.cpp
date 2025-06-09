// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#include <objectformatter.hpp>
#include <pdfcommon.hpp>

#include <cassert>

namespace capypdf::internal {

ObjectFormatter::ObjectFormatter(std::string_view base_indent)
    : state{pystd2025::CString{base_indent.data(), base_indent.size()}, 0, 0} {}

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
    if(stack.is_empty()) {
        fprintf(stderr, "Stack underrun\n");
        std::abort();
    }
    if(stack.top().type != ctype) {
        fprintf(stderr, "Pop type mismatch.\n");
        std::abort();
    }
    state = std::move(stack.top().params);
    stack.pop();
    if(!buf.is_empty() && buf.back() == '\n') {
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
    add_token(pystd2025::CStringView(raw_text.data(), raw_text.size()));
}

void ObjectFormatter::add_token(pystd2025::CStringView raw_text) {
    check_indent();
    buf += raw_text;
    added_item();
}

void ObjectFormatter::add_token(int32_t number) {
    check_indent();
    pystd2025::format_append(buf, "%d", number);
    added_item();
}

void ObjectFormatter::add_token(uint32_t number) {
    check_indent();
    pystd2025::format_append(buf, "%u", number);
    added_item();
}

void ObjectFormatter::add_token(double number) {
    check_indent();
    pystd2025::format_append(buf, "%f", number);
    added_item();
}

void ObjectFormatter::add_token(size_t number) {
    check_indent();
    pystd2025::format_append(buf, "%zd", number);
    added_item();
}

void ObjectFormatter::add_token_with_slash(const char *name) {
    check_indent();
    assert(name[0] != '/');
    pystd2025::format_append(buf, "/%s", name);
    added_item();
}

void ObjectFormatter::add_token_with_slash(std::string_view name) {
    check_indent();
    assert(name[0] != '/');
    pystd2025::CStringView tmp(name.data(), name.size());
    buf.append('/');
    buf += tmp;
    added_item();
}

void ObjectFormatter::add_object_ref(int32_t onum) {
    check_indent();
    pystd2025::format_append(buf, "%i 0 R", onum);
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
    pystd2025::format_append(buf, "(%s)", str.c_str());
    added_item();
}

void ObjectFormatter::added_item() {
    ++state.num_entries;
    if(stack.is_empty()) {
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
    assert(stack.is_empty());
    if(buf.is_empty() || (!buf.is_empty() && buf.back() != '\n')) {
        buf.append('\n');
    }
    std::string res(buf.c_str());
    buf.clear();
    return res;
}

} // namespace capypdf::internal
