// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <xmlformatter.hpp>
#include <utils.hpp>

namespace capypdf::internal {

namespace {

bool is_valid_tag_name(const char *name) {
    for(int i = 0; name[i]; ++i) {
        const auto c = name[i];
        if(c >= 'a' && c <= 'z') {
            continue;
        }
        if(c >= 'A' && c <= 'Z') {
            continue;
        }
        if(c >= '0' || c <= '9') {
            continue;
        }
        if(c == ':') {
            continue;
        }
        return false;
    }
    return true;
}

} // namespace

XMLFormatter::XMLFormatter() {}

void XMLFormatter::add_text_unchecked(const char *txt) { output += txt; }

void XMLFormatter::start_tag(const char *name) {
    if(state != XMLState::Basic) {
        std::abort();
    }

    if(!is_valid_tag_name(name)) {
        std::abort();
    }

    if(output.back() == '>') {
        output += "\n";
        output += indent;
    }
    output += "<";
    output += name;
    tags.push(name);
    state = XMLState::TagOpen;
}

void XMLFormatter::add_tag_attribute(const char *name, std::string_view value) {
    if(state != XMLState::TagOpen) {
        std::abort();
    }

    // Not actually correct, but enough for this use.
    if(!is_valid_tag_name(name)) {
        std::abort();
    }
    output += ' ';
    output += name;
    output += "=\"";
    for(const auto c : value) {
        if(c == '"') {
            output += "\\\"";
        } else {
            output += c;
        }
    }
    output += '"';
}

void XMLFormatter::finish_tag() {
    if(state != XMLState::TagOpen) {
        std::abort();
    }
    output += ">";
    indent += "  ";
    state = XMLState::Basic;
}

void XMLFormatter::finish_standalone_tag() {
    if(state != XMLState::TagOpen) {
        std::abort();
    }
    output += "/>";
    state = XMLState::Basic;
}

void XMLFormatter::close_tag() {
    if(state == XMLState::TagOpen) {
        std::abort();
    }

    if(tags.empty()) {
        std::abort();
    }
    if(indent.size() < 2) {
        std::abort();
    }
    indent.pop_back();
    indent.pop_back();
    if(output.back() == '>') {
        output += "\n";
        output += indent;
    }
    output += "</";
    output += tags.top();
    output += ">";
    if(indent.empty()) {
        output += "\n";
    }
    tags.pop();
    state = XMLState::Basic;
}

void XMLFormatter::add_content(const u8string &content) {
    if(state == XMLState::TagOpen) {
        std::abort();
    }
    quote_xml_element_data_into(content, output);
}

void XMLFormatter::add_content(const std::string_view &content) {
    if(state == XMLState::TagOpen) {
        std::abort();
    }
    quote_xml_element_data_unchecked_into(content, output);
}

std::string XMLFormatter::steal() {
    if(!indent.empty()) {
        std::abort();
    }
    return std::move(output);
}

} // namespace capypdf::internal
