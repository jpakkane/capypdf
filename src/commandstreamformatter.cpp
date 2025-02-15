// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <commandstreamformatter.hpp>

namespace capypdf::internal {

CommandStreamFormatter::CommandStreamFormatter(std::string_view start_indent)
    : lead{start_indent}, app{std::back_inserter(buf)} {}

void CommandStreamFormatter::append(std::string_view raw_text) {
    if(!raw_text.empty()) {
        buf += lead;
        buf += raw_text;
        if(buf.back() != '\n') {
            buf += '\n';
        }
    }
}

void CommandStreamFormatter::BT() {
    append("BT");
    stack.push(StateCommand::BT);
    indent();
}

rvoe<NoReturnValue> CommandStreamFormatter::ET() {
    if(stack.empty() || stack.top() != StateCommand::BT) {
        RETERR(DrawStateEndMismatch);
    }
    stack.pop();
    dedent();
    append("ET");
    RETOK;
}

void CommandStreamFormatter::CommandStreamFormatter::q() {
    append("q");
    stack.push(StateCommand::BT);
    indent();
}

rvoe<NoReturnValue> CommandStreamFormatter::Q() {
    if(stack.empty() || stack.top() != StateCommand::q) {
        RETERR(DrawStateEndMismatch);
    }
    stack.pop();
    dedent();
    append("Q");
    RETOK;
}

void CommandStreamFormatter::indent() { lead += "  "; }

void CommandStreamFormatter::dedent() {
    lead.pop_back();
    lead.pop_back();
}

} // namespace capypdf::internal
