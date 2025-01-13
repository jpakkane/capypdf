// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <capypdf.hpp>
#include <stdio.h>
#include <string>
#ifndef _WIN32
#include <unistd.h>
#endif

int main() {
    const char *fname = "capy_cpptest.pdf";
    FILE *f;
    unlink(fname);
    f = fopen(fname, "r");
    if(f) {
        fprintf(stderr, "Test file alread exists.\n");
        fclose(f);
        return 1;
    }

    capypdf::DocumentProperties docpropd;
    docpropd.set_author("Creator Person");
    std::string creator{"C++ test program"};
    docpropd.set_creator(creator);
    std::string title{"Test document"};
    docpropd.set_title(std::string_view{title});
    capypdf::Generator gen(fname, docpropd);
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
