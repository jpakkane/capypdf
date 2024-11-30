#!/usr/bin/env python3

# SPDX-License-Identifier: Apache-2.0
# Copyright 2023-2024 Jussi Pakkanen

import pathlib, os, sys, json, re

if 'CAPYPDF_SO_OVERRIDE' not in os.environ:
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
        self.footerh = 30
        fonts = self.doc['fonts']
        self.fontfile = fonts['regular']
        self.boldfontfile = fonts['bold']
        self.symbolfontfile = fonts['symbol']
        self.codefontfile = fonts['code']
        fontsizes = self.doc['fontsizes']
        self.titlesize = fontsizes['title']
        self.headingsize = fontsizes['heading']
        self.textsize = fontsizes['text']
        self.codesize = fontsizes['code']
        self.symbolsize = fontsizes['symbol']
        self.hlineheight = 1.2*self.headingsize
        self.codelineheight = 1.5*self.codesize
        self.textlineheight = 1.5*self.textsize
        self.footersize = fontsizes['footer']
        opts = capypdf.DocumentMetadata()
        props = capypdf.PageProperties()
        opts.set_title(self.doc['metadata']['title'])
        opts.set_author(self.doc['metadata']['author'])
        opts.set_creator('PDF presentation generator')
        props.set_pagebox(capypdf.PageBox.Media, 0, 0, self.w, self.h)
        opts.set_default_page_properties(props)
        self.pdfgen = capypdf.Generator(self.ofilename, opts)
        self.basefont = self.pdfgen.load_font(self.fontfile)
        self.boldbasefont = self.pdfgen.load_font(self.boldfontfile)
        #self.symbolfont = self.pdfgen.load_font(self.symbolfontfile)
        self.codefont = self.pdfgen.load_font(self.codefontfile)
        self.setup_colors()
        self.setup_parsevars()

    def setup_colors(self):
        self.normalcolor = capypdf.Color()
        self.normalcolor.set_rgb(0, 0, 0)
        self.stringcolor = capypdf.Color()
        self.stringcolor.set_rgb(0.7, 0.7, 0.7)
        self.keywordcolor = capypdf.Color()
        self.keywordcolor.set_rgb(0.7, 0, 0.8)
        self.numbercolor = capypdf.Color()
        self.numbercolor.set_rgb(0, 1.0, 0)
        self.commentcolor = capypdf.Color()
        self.commentcolor.set_rgb(0.2, 0.6, 0.2)

    def setup_parsevars(self):
        self.wordre = re.compile(r'([a-zA-Z_]+)')
        self.numre = re.compile(r'([0-9]+)')
        self.keywords = {'def', 'class', 'print', 'if', 'for', 'ifelse', 'else', 'return'}

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
            ctx.cmd_re(0, 0, self.w, self.footerh)
            ctx.cmd_f()
            ctx.cmd_rg(1, 1, 1)
            url = self.doc['metadata']['url']
            w = self.pdfgen.text_width(url, self.codefont, self.footersize)
            ctx.render_text(url, self.codefont, self.footersize, self.w-50-w, 10)

    def render_centered(self, ctx, text, font, pointsize, x, y):
        text_w = self.pdfgen.text_width(text, font, pointsize)
        ctx.render_text(text, font, pointsize, x -text_w/2, y)

    def render_heading(self, ctx, text, y):
        titlelines = self.split_to_lines(text, self.boldbasefont, self.titlesize, self.w*0.8)
        y_off = y
        for i in range(len(titlelines)):
            self.render_centered(ctx,
                                 titlelines[i],
                                 self.boldbasefont,
                                 self.titlesize,
                                 self.w/2,
                                 y_off)
            y_off -= self.hlineheight
        return y - y_off

    def render_title_page(self, ctx, p):
        y_off = self.h - 2*self.headingsize
        taken_height = self.render_heading(ctx, p['title'], y_off)

        y_off -= taken_height
        y_off -= self.headingsize
        self.render_centered(ctx,
                             p['author'],
                             self.basefont,
                             self.titlesize-4,
                             self.w/2,
                             y_off)
        y_off -= self.hlineheight

        self.render_centered(ctx,
                             p['email'],
                             self.codefont,
                             self.titlesize-8,
                             self.w/2,
                             y_off)

    def render_bullet_page(self, ctx, p):
        subtr = p['subtransition']
        bullettr = capypdf.Transition()
        bullettr.set_S(TRANS2ENUM[subtr['type']])
        bullettr.set_D(subtr['duration'])
        text_w = self.pdfgen.text_width(p['heading'],
                                        self.boldbasefont,
                                        self.headingsize)
        head_y = self.h - self.hlineheight
        ctx.render_text(p['heading'],
                        self.boldbasefont,
                        self.headingsize,
                        (self.w-text_w)/2, head_y)
        current_y = head_y - self.hlineheight
        box_indent = 90
        bullet_separation = 1.5
        bullet_linesep = 1.2
        bullet_id = 1
        ocgs = []
        for entry in p['bullets']:
            ocg = self.pdfgen.add_optional_content_group(capypdf.OptionalContentGroup('bullet' + str(bullet_id)))
            ocgs.append(ocg)
            ctx.cmd_BDC(ocg)
            ctx.render_text('—', #p['bulletchar'],
                            self.basefont, #self.symbolfont,
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
                             self.h - self.hlineheight)
        text = ctx.text_new()
        text.cmd_Tf(self.codefont, self.codesize)
        text.cmd_Td(60, self.h - 3.5*self.headingsize)
        text.cmd_TL(self.codelineheight)
        if p.get('language', 'none'):
            self.colorize_pycode(text, p['code'])
        else:
            for line in p['code']:
                text.render_text(line)
                text.cmd_Tstar()
        ctx.render_text_obj(text)

    def render_image_page(self, ctx, p):
        image = self.pdfgen.load_image(p['file'])
        improp = capypdf.ImagePdfProperties()
        imw, imh = image.get_size()
        imid = self.pdfgen.add_image(image, improp)
        with ctx.push_gstate():
            usable_h = self.h - self.footerh
            head_y = self.h - self.hlineheight
            if 'heading' in p:
                taken_y = self.render_heading(ctx, p['heading'], head_y)
                usable_h -= taken_y
            w = p.get('width', None)
            if w is None:
                w = 0.8*usable_h / imh * imw
            h = w / imw * imh
            ctx.translate((self.w-w)/2, (usable_h-h)/2 + self.footerh)
            ctx.scale(w, h)
            ctx.draw_image(imid)

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
                elif page['type'] == 'image':
                    self.render_image_page(ctx, page)
                    pass
                else:
                    raise RuntimeError('Unknown page type.')

    def colorize_pycode(self, t, lines):
        for line in lines:
            ostr = ''
            while line:
                if line.startswith("'"):
                    qstr, line = line.split("'", 2)[1:]
                    t.set_nonstroke(self.stringcolor)
                    t.render_text(f"'{qstr}'")
                    t.set_nonstroke(self.normalcolor)
                    continue
                if line.startswith(' '):
                    t.render_text(' ')
                    line = line[1:]
                    continue
                if line.startswith('#'):
                    t.set_nonstroke(self.commentcolor)
                    t.render_text(line)
                    t.set_nonstroke(self.normalcolor)
                    line = ''
                    continue
                m = re.match(self.numre, line)
                if m:
                    numbah = m.group(0)
                    t.set_nonstroke(self.numbercolor)
                    t.render_text(numbah)
                    t.set_nonstroke(self.normalcolor)
                    line = line[len(numbah):]
                    continue
                m = re.match(self.wordre, line)
                if m:
                    word = m.group(0)
                    if word in self.keywords:
                        t.set_nonstroke(self.keywordcolor)
                        t.render_text(word)
                        t.set_nonstroke(self.normalcolor)
                    else:
                        t.render_text(word)
                    line = line[len(word):]
                    continue
                t.render_text(line[0:1])
                line = line[1:]
            t.cmd_Tstar()

    def finish(self):
        self.pdfgen.write()

if __name__ == '__main__':
    if True:
        p = Demopresentation(sys.argv[1], 'demo_presentation.pdf')
        p.create()
        p.finish()
    else:
        p = ColorTest()
        p.doit()
