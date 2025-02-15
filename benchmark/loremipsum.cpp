// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <capypdf.hpp>
#include <vector>
#include <string>
#include <algorithm>
#ifndef _WIN32
#include <unistd.h>
#endif

#define CHCK(command)                                                                              \
    {                                                                                              \
        auto rc = command;                                                                         \
        if(!rc) {                                                                                  \
            fprintf(stderr, "%s\n", capy_error_message(rc.error()));                               \
            std::abort();                                                                          \
        }                                                                                          \
    }

namespace {

CapyPDF_StructureItemId document_root_item;

// #define YOLO

#ifdef YOLO
const std::vector<std::string> column1{
    "Lorem ipsum dolor sit amet, consectetur",
};
#else

const std::vector<std::string> column1{
    "Lorem ipsum dolor sit amet, consectetur",
    "adipiscing elit, sed do eiusmod tempor",
    "incididunt ut labore et dolore magna aliqua.",
    "Amet mauris commodo quis imperdiet. Risus",
    "viverra adipiscing at in tellus integer feugiat",
    "scelerisque varius. Urna nec tincidunt praesent",
    "semper. Lorem ipsum dolor sit amet",
    "consectetur adipiscing. Quis hendrerit dolor",
    "magna eget est. Velit euismod in pellentesque",
    "massa placerat duis ultricies lacus sed.",
    "Rhoncus aenean vel elit scelerisque mauris",
    "pellentesque pulvinar pellentesque. Dignissim",
    "convallis aenean et tortor at. Turpis massa",
    "tincidunt dui ut ornare lectus sit amet est. Velit",
    "aliquet sagittis id consectetur purus ut",
    "faucibus. Arcu dictum varius duis at",
    "consectetur lorem donec massa. Pellentesque",
    "habitant morbi tristique senectus. Praesent",
    "elementum facilisis leo vel fringilla est. Congue",
    "nisi vitae suscipit tellus mauris a diam.",
    "Faucibus pulvinar elementum integer enim",
    "neque. Pellentesque id nibh tortor id aliquet.",
    "",
    "Augue ut lectus arcu bibendum at varius vel",
    "pharetra. Amet mattis vulputate enim nulla",
    "aliquet porttitor. Purus semper eget duis at",
    "tellus. Quam lacus suspendisse faucibus",
    "interdum posuere. Massa sed elementum",
    "tempus egestas sed sed risus pretium quam.",
    "Elit ut aliquam purus sit. Euismod lacinia at",
    "quis risus. Integer malesuada nunc vel risus",
    "commodo. Non arcu risus quis varius. Quam id",
    "leo in vitae turpis massa sed. Amet consectetur",
    "adipiscing elit pellentesque habitant morbi",
    "tristique senectus et. Et leo duis ut diam. Elit",
    "pellentesque habitant morbi tristique senectus",
    "et. Nisi porta lorem mollis aliquam. Feugiat",
};
#endif

const std::vector<std::string> column2{
    "pretium nibh ipsum consequat. Morbi leo urna",
    "molestie at elementum eu. Quis vel eros donec",
    "ac odio tempor orci.",
    "",
    "Massa tempor nec feugiat nisl pretium. Elit",
    "scelerisque mauris pellentesque pulvinar",
    "pellentesque habitant morbi tristique senectus.",
    "Diam in arcu cursus euismod quis viverra.",
    "Bibendum est ultricies integer quis. Semper",
    "risus in hendrerit gravida. Urna porttitor",
    "rhoncus dolor purus non enim praesent",
    "elementum. In mollis nunc sed id. Auctor",
    "neque vitae tempus quam pellentesque nec",
    "nam aliquam sem. Ultricies mi quis hendrerit",
    "dolor magna eget est lorem ipsum. Vulputate",
    "dignissim suspendisse in est ante in nibh",
    "mauris. Nulla pharetra diam sit amet nisl",
    "suscipit adipiscing. Eu mi bibendum neque",
    "egestas. Semper feugiat nibh sed pulvinar",
    "proin gravida.",
    "",
    "Facilisi etiam dignissim diam quis. Ultrices in",
    "iaculis nunc sed augue lacus viverra vitae.",
    "Lacus sed viverra tellus in hac habitasse.",
    "Faucibus pulvinar elementum integer enim",
    "neque. Pulvinar mattis nunc sed blandit libero",
    "volutpat sed. Tellus id interdum velit laoreet id",
    "donec. Velit sed ullamcorper morbi tincidunt",
    "ornare. Venenatis tellus in metus vulputate eu",
    "scelerisque felis imperdiet proin. Tellus",
    "elementum sagittis vitae et leo. Lobortis",
    "elementum nibh tellus molestie nunc non.",
    "Aenean pharetra magna ac placerat vestibulum",
    "lectus mauris ultrices. Imperdiet dui accumsan",
    "sit amet nulla facilisi morbi. Laoree",
    "suspendisse interdum consectetur libero id.",
    "Purus in massa tempor nec feugiat nisl pretium",
};

const std::string title = "Title McTitleface";
const std::string author = "Author McAuthorface";
const std::string email = "author@servermcserverface.com";

double cm2pt(double cm) { return cm * 28.346; }
// double pt2cm(double pt) { return pt / 28.346; }

template<typename T> int num_spaces(const T &s) { return std::count(s.begin(), s.end(), ' '); }

const double midx = cm2pt(21.0 / 2);

} // namespace

void render_column(const std::vector<std::string> &text_lines,
                   capypdf::Generator &gen,
                   capypdf::DrawContext &ctx,
                   CapyPDF_FontId textfont,
                   double textsize,
                   double leading,
                   double column_left,
                   double column_top) {
    const double target_width = cm2pt(8);
    auto textobj = ctx.text_new();
    textobj.cmd_Tf(textfont, textsize);
    textobj.cmd_Td(column_left, column_top);
    textobj.cmd_TL(leading);
    textobj.cmd_BDC(gen.add_structure_item(CAPY_STRUCTURE_TYPE_P, &document_root_item, nullptr));
    for(size_t i = 0; i < text_lines.size(); ++i) {
        const auto &l = text_lines[i];
        if(i + 1 < text_lines.size() && text_lines[i + 1].empty()) {
            textobj.cmd_Tw(0);
            textobj.render_text(l);
            textobj.cmd_Tstar();
        } else {
            if(!l.empty()) {
                double total_w = gen.text_width(l, textfont, textsize);
                const double extra_w = target_width - total_w;
                const int ns = num_spaces(l);
                const double word_spacing = ns != 0 ? extra_w / ns : 0;
                textobj.cmd_Tw(word_spacing);
                textobj.render_text(l);
            } else {
                textobj.cmd_EMC();
                textobj.cmd_BDC(
                    gen.add_structure_item(CAPY_STRUCTURE_TYPE_P, &document_root_item, nullptr));
            }
            textobj.cmd_Tstar();
        }
    }
    textobj.cmd_EMC();
    ctx.render_text_obj(textobj);
}

void draw_headings(capypdf::Generator &gen, capypdf::DrawContext &ctx) {
    auto titlefont = gen.load_font("/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf");
    auto authorfont = gen.load_font("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf");
    const double titley = cm2pt(29 - 2.5);
    const double authory = cm2pt(29 - 3.5);
    const double titlesize = 28;
    const double authorsize = 18;

    auto title_item = gen.add_structure_item(CAPY_STRUCTURE_TYPE_H1, &document_root_item, {});
    ctx.cmd_BDC(title_item);
    ctx.render_text(title,
                    titlefont,
                    titlesize,
                    midx - gen.text_width(title, titlefont, titlesize) / 2,
                    titley);
    ctx.cmd_EMC();
    ctx.cmd_BDC(gen.add_structure_item(CAPY_STRUCTURE_TYPE_H2, &document_root_item, {}));
    ctx.render_text(author,
                    authorfont,
                    authorsize,
                    midx - gen.text_width(author, authorfont, authorsize) / 2,
                    authory);
    ctx.cmd_EMC();
}

void draw_maintext(capypdf::Generator &gen, capypdf::DrawContext &ctx) {
    const double pagenumy = cm2pt(2);
    const double column1_top = cm2pt(29 - 6);
    const double column1_left = cm2pt(2);
    const double column2_top = cm2pt(29 - 6);
    const double column2_left = cm2pt(21 - 2 - 8);
    const double leading = 15;
    const double textsize = 10;
    capypdf::BDCTags tags;
    tags.add_tag("Type", -1, "Pagination", -1);
    auto textfont = gen.load_font("/usr/share/fonts/truetype/noto/NotoSerif-Regular.ttf");
    render_column(column1, gen, ctx, textfont, textsize, leading, column1_left, column1_top);
    render_column(column2, gen, ctx, textfont, textsize, leading, column2_left, column2_top);
    ctx.cmd_BDC_testing("Artifact", -1, &tags);
    ctx.render_text("1",
                    1,
                    textfont,
                    textsize,
                    midx - gen.text_width("1", -1, textfont, textsize) / 2,
                    pagenumy);
    ctx.cmd_EMC();
}

void draw_email(capypdf::Generator &gen, capypdf::DrawContext &ctx) {
    auto emailfont = gen.load_font("/usr/share/fonts/truetype/noto/NotoMono-Regular.ttf");
    const double emailsize = 16;
    const double emaily = cm2pt(29 - 4.3);
    ctx.cmd_BDC(gen.add_structure_item(CAPY_STRUCTURE_TYPE_H3, &document_root_item, nullptr));
    ctx.render_text(email,
                    emailfont,
                    emailsize,
                    midx - gen.text_width(email, emailfont, emailsize) / 2,
                    emaily);
    ctx.cmd_EMC();
}

void create_doc() {
    capypdf::DocumentProperties opts;
    opts.set_tagged(true);
    opts.set_language("en-US");
    opts.set_pdfa(CAPY_PDFA_4f);
    opts.set_device_profile(CAPY_DEVICE_CS_RGB, "/usr/share/color/icc/ghostscript/srgb.icc");
    opts.set_output_intent("sRGB IEC61966-2.1");
    capypdf::Generator gen{"loremipsum.pdf", opts};

    auto ctx = gen.new_page_context();

    document_root_item = gen.add_structure_item(CAPY_STRUCTURE_TYPE_DOCUMENT, {}, {});
    draw_headings(gen, ctx);
    draw_email(gen, ctx);
    draw_maintext(gen, ctx);
    gen.add_page(ctx);
    gen.write();
}

int main(int argc, char **argv) {
    int num_rounds = 1000;
    if(argc > 1) {
        num_rounds = atoi(argv[1]);
    }
    for(int i = 0; i < num_rounds; ++i) {
        unlink("loremipsum.pdf");
        create_doc();
    }
    return 0;
}
