// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <capypdf.h>
#include <stdio.h>
#ifndef _WIN32
#include <unistd.h>
#endif

int main() {
    CapyPDF_EC rc;
    CapyPDF_Generator *gen;
    CapyPDF_DocumentMetadata *opt;
    CapyPDF_DrawContext *dc;
    const char *fname = "capy_ctest.pdf";
    FILE *f;
    unlink(fname);
    f = fopen(fname, "r");
    if(f) {
        fprintf(stderr, "Test file exists.\n");
        return 1;
    }

    if((rc = capy_doc_md_new(&opt)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_generator_new(fname, opt, &gen)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_doc_md_destroy(opt)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_page_draw_context_new(gen, &dc)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_dc_cmd_rg(dc, 1.0, 0.1, 0.5)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_dc_cmd_re(dc, 100, 100, 200, 200)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_dc_cmd_f(dc)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_generator_add_page(gen, dc)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_dc_destroy(dc)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_generator_write(gen)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    if((rc = capy_generator_destroy(gen)) != 0) {
        fprintf(stderr, "%s\n", capy_error_message(rc));
        return 1;
    }

    f = fopen(fname, "r");
    if(!f) {
        fprintf(stderr, "Output file not created.\n");
        return 1;
    }
    fclose(f);
    unlink(fname);
    return 0;
}
