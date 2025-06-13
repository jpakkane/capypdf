// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <commandstreamformatter.hpp>
#include <assert.h>

namespace capypdf::internal {

namespace {

pystd2025::CStringView sv2csv(pystd2025::CStringView v) {
    return pystd2025::CStringView(v.data(), v.size());
}

} // namespace

CommandStreamFormatter::CommandStreamFormatter() {}

// CommandStreamFormatter::CommandStreamFormatter(pystd2025::CStringView start_indent)
//     : lead{start_indent.data(), start_indent.size()} {}

void CommandStreamFormatter::append(pystd2025::CStringView line_of_text_) {
    auto line_of_text = sv2csv(line_of_text_);
    if(!line_of_text.is_empty()) {
        buf += lead;
        buf += line_of_text;
        if(buf.back() != '\n') {
            buf += '\n';
        }
    }
}

void CommandStreamFormatter::append_command(pystd2025::CStringView arg, const char *command) {
    buf += lead;
    buf += sv2csv(arg);
    buf += ' ';
    buf += command;
    buf += '\n';
}

void CommandStreamFormatter::append_command(double arg, const char *command) {
    pystd2025::format_append(buf, "%s%f %s\n", lead.c_str(), arg, command);
}

void CommandStreamFormatter::append_command(double arg1, double arg2, const char *command) {
    pystd2025::format_append(buf, "%s%f %f %s\n", lead.c_str(), arg1, arg2, command);
}

void CommandStreamFormatter::append_command(double arg1,
                                            double arg2,
                                            double arg3,
                                            const char *command) {
    pystd2025::format_append(buf, "%s%f %f %f %s\n", lead.c_str(), arg1, arg2, arg3, command);
}

void CommandStreamFormatter::append_command(
    double arg1, double arg2, double arg3, double arg4, const char *command) {
    pystd2025::format_append(
        buf, "%s%f %f %f %f %s\n", lead.c_str(), arg1, arg2, arg3, arg4, command);
}

void CommandStreamFormatter::append_command(int32_t arg, const char *command) {
    pystd2025::format_append(buf, "%s%d %s\n", lead.c_str(), arg, command);
}

void CommandStreamFormatter::append_dict_entry(const char *key, pystd2025::CStringView value) {
    assert(key[0] == '/');
    buf += lead;
    buf += key;
    buf += ' ';
    buf += sv2csv(value);
    buf += '\n';
}

void CommandStreamFormatter::append_dict_entry(const char *key, int32_t value) {
    pystd2025::format_append(buf, "%s%s %d\n", lead.c_str(), key, value);
}

void CommandStreamFormatter::append_dict_entry_string(const char *key, const char *value) {
    if(key[0] == '/') {
        pystd2025::format_append(buf, "%s%s (%s)\n", lead.c_str(), key, value);
    } else {
        pystd2025::format_append(buf, "%s/%s (%s)\n", lead.c_str(), key, value);
    }
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
    if(stack.is_empty() || stack.back() != stype) {
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

rvoe<pystd2025::CString> CommandStreamFormatter::steal() {
    if(!stack.is_empty()) {
        RETERR(DrawStateEndMismatch);
    }
    auto tmpres = pystd2025::CString(buf.c_str());
    buf.clear();
    return tmpres;
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
