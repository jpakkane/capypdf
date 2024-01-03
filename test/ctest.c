// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <capypdf.h>
#include <stdio.h>

int main() {
    CapyPDF_EC rc;
    CapyPDF_Generator *gen;
    CapyPDF_Options *opt;

    if((rc = capy_options_new(&opt)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_generator_new("dummy.pdf", opt, &gen)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_options_destroy(opt)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_generator_destroy(gen)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }
    return 0;
}
