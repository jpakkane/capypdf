// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <cffsubsetter.hpp>
#include <ft_subsetter.hpp>
#include <filesystem>

int main2(int argc, char **argv) {
    auto cff_file = capypdf::internal::parse_cff_file(argv[1]);
    return 0;
}

int main1(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "%s <font file>\n", argv[0]);
        return 1;
    }
    std::filesystem::path fontfile(true ? "/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc"
                                        : argv[1]);
    auto ext = fontfile.extension();
    if(ext == ".cff") {
        auto cff = capypdf::internal::parse_cff_file(fontfile.string().c_str()).value();
        printf("Num chars: %d\n", (int)cff.char_strings.size());
        printf("All strings:\n");
        for(const auto &s : cff.string.entries) {
            std::string tmpstr((const char *)s.data(), s.size());
            printf("%s\n", tmpstr.c_str());
        }
    } else if(ext == ".ttc") {
        // /usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc
        capypdf::internal::FontProperties fprops;
        auto cff =
            capypdf::internal::load_and_parse_font_file(fontfile.string().c_str(), fprops).value();
        if(auto *plaincff = std::get_if<capypdf::internal::CFFont>(&cff)) {
            printf("Num chars: %d\n", (int)plaincff->char_strings.size());
        }
        if(auto *ttc = std::get_if<capypdf::internal::TrueTypeFontFile>(&cff)) {
            std::vector<capypdf::internal::SubsetGlyphs> glyphs;
            glyphs.emplace_back(0, 0);
            glyphs.emplace_back(1024, 1024);
            capypdf::internal::CFFWriter wr(ttc->cff.value(), glyphs);
            wr.create();
            auto sfont = wr.steal();
            capypdf::internal::parse_cff_data(sfont).value();
            FILE *f = fopen("fontout.cff", "wb");
            fwrite(sfont.data(), 1, sfont.size(), f);
            fclose(f);
        }
    } else {
        printf("Unsupported format.\n");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) { return main1(argc, argv); }
