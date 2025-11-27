// SPDX-License-Identifier: Apache-2.0
// Copyright 2023-2024 Jussi Pakkanen

#include <generator.hpp>
#include <cmath>

using namespace capypdf::internal;

void file_embed() {
    DocumentProperties opts;

    opts.default_page_properties.mediabox->x2 = opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("File embedding test").value();
    opts.author = u8string::from_cstr("Test Person").value();
    {
        GenPopper genpop("fembed_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        EmbeddedFile ef;
        ef.path = "embed.txt";
        ef.pdfname = u8string::from_cstr("embed.txt").value();
        auto efid = gen.embed_file(ef).value();
        auto fileannoid =
            gen.add_annotation(
                   Annotation{{}, FileAttachmentAnnotation{efid}, PdfRectangle{35, 95, 45, 105}})
                .value();
        {
            auto ctxguard = gen.guarded_page_context();
            auto &ctx = ctxguard.ctx;

            ctx.render_pdfdoc_text_builtin(
                "<- an embedded file.", CAPY_FONT_HELVETICA, 12, 50, 100);
            ctx.annotate(fileannoid);
            auto textannoid =
                gen
                    .add_annotation(Annotation{
                        {},
                        TextAnnotation{u8string::from_cstr("This is a text ännotation").value()},
                        PdfRectangle{150, 60, 180, 90}})
                    .value();
            ctx.annotate(textannoid);
            ctx.cmd_rg(0, 0, 1);
            ctx.render_pdfdoc_text_builtin("Link", CAPY_FONT_HELVETICA, 12, 10, 10);
            auto linkannoid =
                gen.add_annotation(
                       Annotation{{},
                                  LinkAnnotation{asciistring::from_cstr("https://github.com/"
                                                                        "mesonbuild/meson")
                                                     .value(),
                                                 {}},
                                  PdfRectangle{10, 10, 32, 20}})
                    .value();
            ctx.annotate(linkannoid);
        }
    }
}

void video_player() {
    DocumentProperties opts;

    opts.default_page_properties.mediabox->x2 = opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("Video player test").value();
    opts.author = u8string::from_cstr("Test Person").value();
#if 0
    const char *mediafile = "samplemedia.jpg";
    const char *mimetype = "image/jpeg";
    std::optional<ClipTimes> subplay;
#else
    const char *mediafile = "samplevideo.mp4";
    asciistring mimetype = asciistring::from_cstr("video/mp4").value();
    std::optional<ClipTimes> subplay = ClipTimes{14 * 60 + 26, 14 * 60 + 32};
#endif
    {
        GenPopper genpop("mediaplayer_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        EmbeddedFile ef;
        ef.path = mediafile;
        ef.pdfname = u8string::from_cstr(mediafile).value();
        auto efid = gen.embed_file(ef).value();
        {
            auto ctxguard = gen.guarded_page_context();
            auto &ctx = ctxguard.ctx;
            ctx.render_pdfdoc_text_builtin("Video below", CAPY_FONT_HELVETICA, 12, 70, 170);
            auto media_anno_id =
                gen.add_annotation(Annotation{{},
                                              ScreenAnnotation{efid, mimetype, subplay},
                                              PdfRectangle{20, 20, 180, 160}})
                    .value();
            ctx.annotate(media_anno_id);
        }
    }
}

int main(int /*argc*/, char ** /*argv*/) {
    file_embed();
    video_player();
    return 0;
}
