#!/usr/bin/env python3

# SPDX-License-Identifier: Apache-2.0
# Copyright 2023-2024 Jussi Pakkanen

import pathlib, os, sys, json

os.environ['CAPYPDF_SO_OVERRIDE'] = 'src'
source_root = pathlib.Path(__file__).parent / '..'
sys.path.append(str(source_root / 'python'))

try:
    import capypdf
except Exception:
    print('You might need to edit the search paths at the top of this file to get it to find the dependencies.')
    raise

def cm2pt(pts):
    return pts*28.346;

TRANS2ENUM = {
    'split': capypdf.TransitionType.Split,
    'blinds': capypdf.TransitionType.Blinds,
    'box': capypdf.TransitionType.Box,
    'wipe': capypdf.TransitionType.Wipe,
    'dissolve': capypdf.TransitionType.Dissolve,
    'glitter': capypdf.TransitionType.Glitter,
    'r': capypdf.TransitionType.R,
    'fly': capypdf.TransitionType.Fly,
    'push': capypdf.TransitionType.Push,
    'cover': capypdf.TransitionType.Cover,
    'uncover': capypdf.TransitionType.Uncover,
    'fade': capypdf.TransitionType.Fade,
}


class Demopresentation:
    def __init__(self, inputdoc, ofilename):
        self.doc = json.load(open(inputdoc))
        self.ofilename = ofilename
        self.w, self.h = self.doc['metadata']['pagesize']
        fonts = self.doc['fonts']
        self.fontfile = fonts['regular']
        self.boldfontfile = '/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf'
        self.symbolfontfile = '/usr/share/fonts/truetype/noto/NotoSansSymbols2-Regular.ttf'
        self.codefontfile = '/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf'
        fontsizes = self.doc['fontsizes']
        self.titlesize = fontsizes['title']
        self.headingsize = fontsizes['heading']
        self.textsize = fontsizes['text']
        self.codesize = fontsizes['code']
        self.symbolsize = fontsizes['symbol']
        self.footersize = fontsizes['footer']
        opts = capypdf.Options()
        props = capypdf.PageProperties()
        opts.set_title(self.doc['metadata']['title'])
        opts.set_author(self.doc['metadata']['author'])
        opts.set_creator('PDF presentation generator')
        props.set_pagebox(capypdf.PageBox.Media, 0, 0, self.w, self.h)
        opts.set_default_page_properties(props)
        self.pdfgen = capypdf.Generator(self.ofilename, opts)
        self.basefont = self.pdfgen.load_font(self.fontfile)
        self.boldbasefont = self.pdfgen.load_font(self.boldfontfile)
        self.symbolfont = self.pdfgen.load_font(self.symbolfontfile)
        self.codefont = self.pdfgen.load_font(self.codefontfile)

    def split_to_lines(self, text, fid, ptsize, width):
        if self.pdfgen.text_width(text, fid, ptsize) <= width:
            return [text]
        words = text.strip().split(' ')
        lines = []
        space_width = self.pdfgen.text_width(' ', fid, ptsize)
        current_line = []
        current_width = 0
        for word in words:
            wwidth = self.pdfgen.text_width(word, fid, ptsize)
            if current_width + space_width + wwidth >= width:
                lines.append(' '.join(current_line))
                current_line = [word]
                current_width = wwidth
            else:
                current_line.append(word)
                current_width += space_width + wwidth
        if current_line:
            lines.append(' '.join(current_line))
        return lines

    def set_transition(self, ctx, page):
        try:
            tdict = page['pagetransition']
        except KeyError:
            return
        ttype = TRANS2ENUM[tdict['type']]
        duration = tdict['duration']
        tr = capypdf.Transition()
        tr.set_S(ttype)
        tr.set_D(duration)
        ctx.set_page_transition(tr)

    def draw_master(self, ctx):
        with ctx.push_gstate():
            ctx.cmd_rg(0.1, 0.3, 0.5)
            ctx.cmd_re(0, 0, self.w, 30)
            ctx.cmd_f()
            ctx.cmd_rg(1, 1, 1)
            ctx.render_text(self.doc['metadata']['url'], self.codefont, self.footersize, self.w-280, 10)

    def render_centered(self, ctx, text, font, pointsize, x, y):
        text_w = self.pdfgen.text_width(text, font, pointsize)
        ctx.render_text(text, font, pointsize, x -text_w/2, y)

    def render_title_page(self, ctx, p):
        self.render_centered(ctx,
                             p['title'],
                             self.boldbasefont,
                             self.titlesize,
                             self.w/2,
                             self.h - 2*self.headingsize)

        self.render_centered(ctx,
                             p['author'],
                             self.basefont,
                             self.titlesize,
                             self.w/2,
                             0.5*self.h)

        self.render_centered(ctx,
                             p['email'],
                             self.codefont,
                             self.titlesize-4,
                             self.w/2,
                             0.35*self.h)


    def render_bullet_page(self, ctx, p):
        subtr = p['subtransition']
        bullettr = capypdf.Transition()
        bullettr.set_S(TRANS2ENUM[subtr['type']])
        bullettr.set_D(subtr['duration'])
        text_w = self.pdfgen.text_width(p['heading'],
                                        self.boldbasefont,
                                        self.headingsize)
        head_y = self.h - 1.5*self.headingsize
        ctx.render_text(p['heading'],
                        self.boldbasefont,
                        self.headingsize,
                        (self.w-text_w)/2, head_y)
        current_y = head_y - 1.5*self.headingsize
        box_indent = 90
        bullet_separation = 1.5
        bullet_linesep = 1.2
        bullet_id = 1
        ocgs = []
        for entry in p['bullets']:
            ocg = self.pdfgen.add_optional_content_group(capypdf.OptionalContentGroup('bullet' + str(bullet_id)))
            ocgs.append(ocg)
            ctx.cmd_BDC(ocg)
            ctx.render_text(p['bulletchar'],
                            self.symbolfont,
                            self.symbolsize,
                            box_indent - 40,
                            current_y+1)
            for line in self.split_to_lines(entry, self.basefont, self.textsize, self.w - 2*box_indent):
                ctx.render_text(line, self.basefont, self.textsize, box_indent, current_y)
                current_y -= bullet_linesep*self.textsize
            ctx.cmd_EMC()
            current_y += (bullet_linesep - bullet_separation)*self.textsize
            bullet_id += 1
        ctx.add_simple_navigation(ocgs, bullettr)

    def render_code_page(self, ctx, p):
        self.render_centered(ctx,
                             p['heading'],
                             self.boldbasefont,
                             self.headingsize,
                             self.w/2,
                             self.h - 1.5*self.headingsize)
        text = ctx.text_new()
        text.cmd_Tf(self.codefont, self.codesize)
        text.cmd_Td(60, self.h - 3.5*self.headingsize)
        text.cmd_TL(1.5 * self.codesize)
        for line in p['code']:
            text.render_text(line)
            text.cmd_Tstar()
        ctx.render_text_obj(text)

    def create(self):
        for page in self.doc['pages']:
            with self.pdfgen.page_draw_context() as ctx:
                self.set_transition(ctx, page)
                self.draw_master(ctx)
                if page['type'] == 'title':
                    self.render_title_page(ctx, page)
                elif page['type'] == 'bullet':
                    self.render_bullet_page(ctx, page)
                elif page['type'] == 'code':
                    self.render_code_page(ctx, page)
                    pass
                else:
                    raise RuntimeError('Unknown page type.')

    def finish(self):
        self.pdfgen.write()

if __name__ == '__main__':
    p = Demopresentation(sys.argv[1], 'demo_presentation.pdf')
    p.create()
    p.finish()
