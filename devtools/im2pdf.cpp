// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Jussi Pakkanen

#include <capypdf.hpp>

int main(int argc, char **argv) {
    if(argc != 3) {
        printf("%s <image input> <pdf output>\n", argv[0]);
    }
    capypdf::DocumentProperties dp;
    capypdf::PageProperties pp;
    pp.set_pagebox(CAPY_BOX_MEDIA, 0, 0, 200, 200);
    dp.set_default_page_properties(pp);
    dp.set_title("Image testing tool");
    capypdf::Generator gen(argv[2], dp);
    auto image = gen.load_image(argv[1]);
    auto imageid = gen.add_image(image, capypdf::ImagePdfProperties{});
    auto ctx = gen.new_page_context();
    ctx.cmd_cm(200, 0, 0, 200, 0, 0);
    ctx.cmd_Do(imageid);
    gen.add_page(ctx);
    gen.write();

    return 0;
}
