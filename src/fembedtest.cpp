/*
 * Copyright 2023 Jussi Pakkanen
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
#include <cmath>

using namespace capypdf;

void file_embed() {
    PdfGenerationData opts;

    opts.default_page_properties.mediabox->x2 = opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("File embedding test").value();
    opts.author = u8string::from_cstr("Test Person").value();
    {
        GenPopper genpop("fembed_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        auto efid = gen.embed_file("embed.txt").value();
        auto fileannoid = gen.create_annotation(Annotation{FileAttachmentAnnotation{efid},
                                                           PdfRectangle{35, 95, 45, 105}})
                              .value();
        {
            auto ctxguard = gen.guarded_page_context();
            auto &ctx = ctxguard.ctx;

            ctx.render_pdfdoc_text_builtin(
                "<- an embedded file.", CAPY_FONT_HELVETICA, 12, 50, 100);
            ctx.annotate(fileannoid);
            auto textannoid =
                gen.create_annotation(Annotation{TextAnnotation{"This is a text annotation"},
                                                 PdfRectangle{150, 60, 180, 90}})
                    .value();
            ctx.annotate(textannoid);
            ctx.cmd_rg(0, 0, 1);
            ctx.render_pdfdoc_text_builtin("Link", CAPY_FONT_HELVETICA, 12, 10, 10);
            auto linkannoid = gen.create_annotation(Annotation{UriAnnotation{"https://github.com/"
                                                                             "mesonbuild/meson"},
                                                               PdfRectangle{10, 10, 32, 20}})
                                  .value();
            ctx.annotate(linkannoid);
        }
    }
}

void video_player() {
    PdfGenerationData opts;

    opts.default_page_properties.mediabox->x2 = opts.default_page_properties.mediabox->y2 = 200;
    opts.title = u8string::from_cstr("Video player test").value();
    opts.author = u8string::from_cstr("Test Person").value();
#if 0
    const char *mediafile = "samplemedia.jpg";
    const char *mimetype = "image/jpeg";
    std::optional<ClipTimes> subplay;
#else
    const char *mediafile = "samplevideo.mp4";
    const char *mimetype = "video/mp4";
    std::optional<ClipTimes> subplay = ClipTimes{14 * 60 + 26, 14 * 60 + 32};
#endif
    {
        GenPopper genpop("mediaplayer_test.pdf", opts);
        PdfGen &gen = *genpop.g;
        auto efid = gen.embed_file(mediafile).value();
        {
            auto ctxguard = gen.guarded_page_context();
            auto &ctx = ctxguard.ctx;
            ctx.render_pdfdoc_text_builtin("Video below", CAPY_FONT_HELVETICA, 12, 70, 170);
            auto media_anno_id =
                gen.create_annotation(Annotation{ScreenAnnotation{efid, mimetype, subplay},
                                                 PdfRectangle{20, 20, 180, 160}})
                    .value();
            ctx.annotate(media_anno_id);
        }
    }
}

int main(int argc, char **argv) {
    file_embed();
    video_player();
    return 0;
}
