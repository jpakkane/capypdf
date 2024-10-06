// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <capypdf.hpp>
#include <stdio.h>
#ifndef _WIN32
#include <unistd.h>
#endif

int main() {
    const char *fname = "capy_cpptest.pdf";
    FILE *f;
    unlink(fname);
    f = fopen(fname, "r");
    if(f) {
        fprintf(stderr, "Test file exists.\n");
        return 1;
    }

    capypdf::DocumentMetadata md;
    capypdf::Generator gen(fname, md);
    capypdf::DrawContext dc = gen.new_page_context();
    dc.cmd_rg(1.0, 0.1, 0.5);
    dc.cmd_re(100, 100, 200, 200);
    dc.cmd_f();
    gen.add_page(dc);
    gen.write();
    f = fopen(fname, "r");
    if(!f) {
        fprintf(stderr, "Output file not created.\n");
        return 1;
    }
    fclose(f);
    unlink(fname);
    return 0;
}
