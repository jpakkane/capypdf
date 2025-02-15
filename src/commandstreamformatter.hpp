// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <errorhandling.hpp>

#include <string>
#include <stack>
#include <vector>
#include <stdint.h>

namespace capypdf::internal {

enum class StateCommand : uint8_t {
    q,
    BT,
};

class CommandStreamFormatter {

public:
    CommandStreamFormatter(std::string_view start_indent);

    void append(std::string_view raw_text);

    void BT();
    rvoe<NoReturnValue> ET();

    void q();
    rvoe<NoReturnValue> Q();

    const std::string &contents() const { return buf; }
    const std::string &current_indent() const { return lead; }

private:
    void indent();
    void dedent();

    std::string lead;
    std::stack<StateCommand, std::vector<StateCommand>> stack;
    std::string buf;
    std::back_insert_iterator<std::string> app;
};

} // namespace capypdf::internal
