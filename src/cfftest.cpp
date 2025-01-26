// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <cffsubsetter.hpp>
#include <ft_subsetter.hpp>

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "Is bad\n");
        return 1;
    }
    std::filesystem::path fontfile(argv[1]);
    auto ext = fontfile.extension();
    if(ext == ".cff") {
        auto cff = capypdf::internal::parse_cff_file(fontfile);
    } else if(ext == ".ttc") {
        // /usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc
        auto cff = capypdf::internal::load_and_parse_font_file(fontfile).value();
    } else {
        printf("Unsupported format.\n");
        return 1;
    }
    return 0;
}
