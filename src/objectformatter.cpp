// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#include <objectformatter.hpp>
#include <print>
#include <format>
#include <cassert>

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

void ObjectFormatter::add_token(const char *raw_text) {
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

int main() {
    ObjectFormatter f("-> ");
    f.begin_dict();
    f.add_token("/Key");
    f.add_token("(value)");
    f.add_token("/Key2");
    f.begin_array(2);
    f.add_token("one");
    f.add_token("two");
    f.add_token("three");
    f.begin_dict();
    f.add_token("/Subkey");
    f.add_object_ref(42);
    f.end_dict();
    f.add_token("five");
    f.add_token("six");
    f.end_array();
    f.add_token("/Key3");
    f.add_token(444);
    f.end_dict();
    auto s = f.steal();
    std::println("Got this:\n{}", s);
    return 0;
}
