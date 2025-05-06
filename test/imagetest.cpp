// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#include <capypdf.hpp>
#include <iostream>

int main() {
    const char *fname = "capy_imagetest.pdf";
    // FILE *f;
    // unlink(fname);
    // f = fopen(fname, "rb");
    // if(f) {
    //     fprintf(stderr, "Test file already exists.\n");
    //     fclose(f);
    //     return 1;
    // }

    const int N_IMG = 4;
    std::string img_files[] = {
        "rgb_tiff.tif",
        "simple.jpg",
        "1bit_noalpha.png",
        "gray_alpha.png",
    };
    std::string img_dir = "../images/";

    capypdf::DocumentProperties dp;
    capypdf::PageProperties pp;
    pp.set_pagebox(CAPY_BOX_MEDIA, 0, 0, 200 * N_IMG, 200);
    dp.set_default_page_properties(pp);
    capypdf::Generator gen(fname, dp);
    auto ctx = gen.new_page_context();
    ctx.cmd_cm(200, 0, 0, 200, 0, 0);

    for(int i = 0; i < N_IMG; ++i) {
        auto img = gen.load_image((img_dir + img_files[i]).c_str());
        auto img_id = gen.add_image(img, capypdf::ImagePdfProperties());
        ctx.cmd_q();
        ctx.cmd_cm(1, 0, 0, 1, i, 0);
        ctx.cmd_Do(img_id);
        ctx.cmd_Q();
    }

    gen.add_page(ctx);
    gen.write();

    // f = fopen(fname, "rb");
    // if(!f) {
    //     fprintf(stderr, "Output file not created.\n");
    //     return 1;
    // }
    // fclose(f);
    // unlink(fname);
    return 0;
}
