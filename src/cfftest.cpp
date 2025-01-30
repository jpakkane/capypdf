// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <cffsubsetter.hpp>
#include <ft_subsetter.hpp>

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "Is bad\n");
        return 1;
    }
    std::filesystem::path fontfile(true ? "/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc"
                                        : argv[1]);
    auto ext = fontfile.extension();
    if(ext == ".cff") {
        auto cff = capypdf::internal::parse_cff_file(fontfile).value();
        printf("Num chars: %d\n", (int)cff.char_strings.size());
        printf("All strings:\n");
        for(const auto &s : cff.string) {
            std::string tmpstr((const char *)s.data(), s.size());
            printf("%s\n", tmpstr.c_str());
        }
    } else if(ext == ".ttc") {
        // /usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc
        auto cff = capypdf::internal::load_and_parse_font_file(fontfile).value();
        if(auto *plaincff = std::get_if<capypdf::internal::CFFont>(&cff)) {
            printf("Num chars: %d\n", (int)plaincff->char_strings.size());
        }
    } else {
        printf("Unsupported format.\n");
        return 1;
    }
    return 0;
}
