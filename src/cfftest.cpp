// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <cffsubsetter.hpp>

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "Is bad\n");
        return 1;
    }
    auto cff = capypdf::internal::parse_cff_file(argv[1]);
    return 0;
}
