/*
 * Copyright 2022-2023 Jussi Pakkanen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <pdfgen.hpp>
#include <pdftext.hpp>
#include <vector>
#include <string>
#include <algorithm>

namespace {

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

const std::string title{"Title McTitleface"};
const std::string author{"Author McAuthorface"};
const std::string email{"author@servermcserverface.com"};

double cm2pt(double cm) { return cm * 28.346; }
// double pt2cm(double pt) { return pt / 28.346; }

int num_spaces(const std::string_view s) { return std::count(s.begin(), s.end(), ' '); }

double
text_width(const std::string_view s, A4PDF::PdfGen &gen, A4PDF_FontId fid, double pointsize) {
    double total_w = 0;
    for(const char c : s) {
        // ASCII FTW!
        const auto w = *gen.glyph_advance(fid, pointsize, c);
        total_w += w;
    }
    return total_w;
}

const double midx = cm2pt(21.0 / 2);

} // namespace

using namespace A4PDF;

void render_column(const std::vector<std::string> &text_lines,
                   PdfGen &gen,
                   PdfDrawContext &ctx,
                   A4PDF_FontId textfont,
                   double textsize,
                   double leading,
                   double column_left,
                   double column_top) {
    const double target_width = cm2pt(8);
    auto textobj = PdfText();
    textobj.cmd_Tf(textfont, textsize);
    textobj.cmd_Td(column_left, column_top);
    textobj.cmd_TL(leading);
    for(size_t i = 0; i < text_lines.size(); ++i) {
        const auto &l = text_lines[i];
        if(i + 1 < text_lines.size() && text_lines[i + 1].empty()) {
            textobj.cmd_Tw(0);
            textobj.render_text(l);
            textobj.cmd_Tstar();
        } else {
            if(!l.empty()) {
                double total_w = text_width(l, gen, textfont, textsize);
                const double extra_w = target_width - total_w;
                const int ns = num_spaces(l);
                const double word_spacing = ns != 0 ? extra_w / ns : 0;
                textobj.cmd_Tw(word_spacing);
                textobj.render_text(l);
            }
            textobj.cmd_Tstar();
        }
    }
    ctx.render_text(textobj);
}

void draw_headings(PdfGen &gen, PdfDrawContext &ctx) {
    auto titlefont = gen.load_font("/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf").value();
    auto authorfont = gen.load_font("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf").value();
    const double titley = cm2pt(29 - 2.5);
    const double authory = cm2pt(29 - 3.5);
    const double titlesize = 28;
    const double authorsize = 18;

    ctx.render_utf8_text(title,
                         titlefont,
                         titlesize,
                         midx - text_width(title, gen, titlefont, titlesize) / 2,
                         titley);
    ctx.render_utf8_text(author,
                         authorfont,
                         authorsize,
                         midx - text_width(author, gen, authorfont, authorsize) / 2,
                         authory);
}

void draw_maintext(PdfGen &gen, PdfDrawContext &ctx) {
    const double pagenumy = cm2pt(2);
    const double column1_top = cm2pt(29 - 6);
    const double column1_left = cm2pt(2);
    const double column2_top = cm2pt(29 - 6);
    const double column2_left = cm2pt(21 - 2 - 8);
    const double leading = 15;
    const double textsize = 10;
    auto textfont = gen.load_font("/usr/share/fonts/truetype/noto/NotoSerif-Regular.ttf").value();
    render_column(column1, gen, ctx, textfont, textsize, leading, column1_left, column1_top);
    render_column(column2, gen, ctx, textfont, textsize, leading, column2_left, column2_top);
    ctx.render_utf8_text(
        "1", textfont, textsize, midx - text_width("1", gen, textfont, textsize) / 2, pagenumy);
}

void draw_email(PdfGen &gen, PdfDrawContext &ctx) {
    auto emailfont = gen.load_font("/usr/share/fonts/truetype/noto/NotoMono-Regular.ttf").value();
    // auto emailfont = gen.load_font("/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf").value();
    // auto emailfont = gen.load_font("/usr/share/fonts/truetype/freefont/FreeMono.ttf").value();
    //     auto emailfont =
    //       gen.load_font("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf").value();
    const double emailsize = 16;
    const double emaily = cm2pt(29 - 4.3);
    ctx.render_utf8_text(email,
                         emailfont,
                         emailsize,
                         midx - text_width(email, gen, emailfont, emailsize) / 2,
                         emaily);
}

int main() {
    PdfGenerationData opts;
    GenPopper genpop("loremipsum.pdf", opts);
    PdfGen &gen = *genpop.g;

    auto ctxguard = gen.guarded_page_context();
    auto &ctx = ctxguard.ctx;

    draw_headings(gen, ctx);
    draw_email(gen, ctx);
    draw_maintext(gen, ctx);
    return 0;
}
