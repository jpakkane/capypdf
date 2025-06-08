// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <commandstreamformatter.hpp>
#include <format>
#include <cassert>

namespace capypdf::internal {

CommandStreamFormatter::CommandStreamFormatter() : appender{std::back_inserter(buf)} {}

CommandStreamFormatter::CommandStreamFormatter(std::string_view start_indent)
    : lead{start_indent}, appender{std::back_inserter(buf)} {}

void CommandStreamFormatter::append(std::string_view line_of_text) {
    if(!line_of_text.empty()) {
        buf += lead;
        buf += line_of_text;
        if(buf.back() != '\n') {
            buf += '\n';
        }
    }
}

void CommandStreamFormatter::append_command(std::string_view arg, const char *command) {
    std::format_to(appender, "{}{} {}\n", lead, arg, command);
}

void CommandStreamFormatter::append_command(double arg, const char *command) {
    std::format_to(appender, "{}{:f} {}\n", lead, arg, command);
}

void CommandStreamFormatter::append_command(double arg1, double arg2, const char *command) {
    std::format_to(appender, "{}{:f} {:f} {}\n", lead, arg1, arg2, command);
}

void CommandStreamFormatter::append_command(int32_t arg, const char *command) {
    std::format_to(appender, "{}{} {}\n", lead, arg, command);
}

void CommandStreamFormatter::append_dict_entry(std::string_view key, std::string_view value) {
    assert(key.front() == '/');
    std::format_to(appender, "{}{} {}\n", lead, key, value);
}

void CommandStreamFormatter::append_dict_entry(std::string_view key, int32_t value) {
    assert(key.front() == '/');
    std::format_to(appender, "{}{} {}\n", lead, key, value);
}

void CommandStreamFormatter::append_dict_entry_string(const char *key, const char *value) {
    assert(key[0] == '/');
    std::format_to(appender, "{}{} ({})\n", lead, key, value);
}

rvoe<NoReturnValue> CommandStreamFormatter::BT() {
    append("BT");
    ERCV(indent(DrawStateType::Text));
    RETOK;
}

rvoe<NoReturnValue> CommandStreamFormatter::ET() {
    ERCV(dedent(DrawStateType::Text));
    append("ET");
    RETOK;
}

rvoe<NoReturnValue> CommandStreamFormatter::CommandStreamFormatter::q() {
    append("q");
    ERCV(indent(DrawStateType::SaveState));
    RETOK;
}

rvoe<NoReturnValue> CommandStreamFormatter::Q() {
    ERCV(dedent(DrawStateType::SaveState));
    append("Q");
    RETOK;
}

rvoe<NoReturnValue> CommandStreamFormatter::BMC() {
    append("BMC");
    ERCV(indent(DrawStateType::MarkedContent));
    RETOK;
}

rvoe<NoReturnValue> CommandStreamFormatter::EMC() {
    ERCV(dedent(DrawStateType::MarkedContent));
    append("EMC");
    RETOK;
}

void CommandStreamFormatter::clear() {
    lead.clear();
    stack.clear();
    buf.clear();
}

rvoe<NoReturnValue> CommandStreamFormatter::indent(DrawStateType stype) {
    if(stype == DrawStateType::Text && has_state(stype)) {
        RETERR(DrawStateEndMismatch); // FIXME
    }
    if(stype == DrawStateType::MarkedContent && has_state(stype)) {
        RETERR(NestedBMC);
    }
    stack.push_back(stype);
    lead += "  ";
    RETOK;
}

rvoe<NoReturnValue> CommandStreamFormatter::dedent(DrawStateType stype) {
    if(stack.empty() || stack.back() != stype) {
        RETERR(DrawStateEndMismatch);
    }
    stack.pop_back();
    lead.pop_back();
    lead.pop_back();
    RETOK;
}

bool CommandStreamFormatter::has_state(DrawStateType stype) {
    for(const auto e : stack) {
        if(e == stype)
            return true;
    }
    return false;
}

rvoe<std::string> CommandStreamFormatter::steal() {
    if(!stack.empty()) {
        RETERR(DrawStateEndMismatch);
    }
    return std::move(buf);
}

size_t CommandStreamFormatter::marked_content_depth() const {
    size_t depth = 0;
    for(const auto &e : stack) {
        if(e == DrawStateType::MarkedContent) {
            ++depth;
        }
    }
    return depth;
}

} // namespace capypdf::internal
