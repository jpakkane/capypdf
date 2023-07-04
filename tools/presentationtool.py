#!/usr/bin/env python3

import pathlib, os, sys

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

class TitlePage:
    def __init__(self, title, author, email):
        self.title = title
        self.author = author
        self.email = email

class BulletPage:
    def __init__(self, heading, entries):
        self.heading = heading
        self.entries = entries

class CodePage:
    def __init__(self, heading, code):
        self.heading = heading
        self.code = code

def create_pages():
    pages = [TitlePage("CapyPDF output demonstration", "Sample Presenter", "none@example.com"),
             BulletPage('Animated bullet points', ['These appear one by one',
                                                   'Navigate with left and right arrow',
                                                   'Only Acrobat Reader in presenter mode works correctly',
                                                   'Others show all bullets immediately']),
             CodePage('Sample code', '''def advance_highlighting():
    code_color += compute_increment()
    # The highlighting used here is arbitrary.
    # Its only purpose is to demonstrate text coloring.
    return True
'''),
            ]
    return pages

class Demopresentation:
    def __init__(self, ofilename, w, h):
        self.ofilename = ofilename
        self.w = w
        self.h = h
        self.fontfile = '/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf'
        self.boldfontfile = '/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf'
        self.symbolfontfile = '/usr/share/fonts/truetype/noto/NotoSansSymbols2-Regular.ttf'
        self.codefontfile = '/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf'
        self.titlesize = 44
        self.headingsize = 44
        self.textsize = 32
        self.codesize = 20
        self.symbolsize = 28
        opts = capypdf.Options()
        opts.set_author('CapyPDF tester')
        opts.set_pagebox(capypdf.PageBox.Media, 0, 0, w, h)
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

    def render_centered(self, ctx, text, font, pointsize, x, y):
        text_w = self.pdfgen.text_width(text, font, pointsize)
        ctx.render_text(text, font, pointsize, x -text_w/2, y)

    def render_title_page(self, ctx, p):
        self.render_centered(ctx,
                             p.title,
                             self.boldbasefont,
                             self.titlesize,
                             self.w/2,
                             self.h - 2*self.headingsize)

        self.render_centered(ctx,
                             p.author,
                             self.basefont,
                             self.titlesize,
                             self.w/2,
                             0.5*self.h)

        self.render_centered(ctx,
                             p.author,
                             self.basefont,
                             self.titlesize,
                             self.w/2,
                             0.5*self.h)

        self.render_centered(ctx,
                             p.email,
                             self.codefont,
                             self.titlesize-4,
                             self.w/2,
                             0.35*self.h)


    def render_bullet_page(self, ctx, p):
        pagetr = capypdf.Transition(capypdf.TransitionType.Push, 1.0)
        ctx.set_page_transition(pagetr)
        bullettr = capypdf.Transition(capypdf.TransitionType.Dissolve, 1.0)
        text_w = self.pdfgen.text_width(p.heading, self.boldbasefont, self.headingsize)
        head_y = self.h - 1.5*self.headingsize
        ctx.render_text(p.heading, self.boldbasefont, self.headingsize, (self.w-text_w)/2, head_y)
        current_y = head_y - 1.5*self.headingsize
        box_indent = 90
        bullet_separation = 1.5
        bullet_linesep = 1.2
        bullet_id = 1
        ocgs = []
        for entry in p.entries:
            ocg = self.pdfgen.add_optional_content_group(capypdf.OptionalContentGroup('bullet' + str(bullet_id)))
            ocgs.append(ocg)
            ctx.cmd_BDC(ocg)
            ctx.render_text('🞂', self.symbolfont, self.symbolsize, box_indent - 40, current_y+1)
            for line in self.split_to_lines(entry, self.basefont, self.textsize, self.w - 2*box_indent):
                ctx.render_text(line, self.basefont, self.textsize, box_indent, current_y)
                current_y -= bullet_linesep*self.textsize
            ctx.cmd_EMC()
            current_y += (bullet_linesep - bullet_separation)*self.textsize
            bullet_id += 1
        ctx.add_simple_navigation(ocgs, bullettr)

    def render_code_page(self, ctx, p):
        tr = capypdf.Transition(capypdf.TransitionType.Uncover, 1.0)
        ctx.set_page_transition(tr)
        self.render_centered(ctx,
                             p.heading,
                             self.boldbasefont,
                             self.titlesize,
                             self.w/2,
                             self.h - 1.5*self.headingsize)
        num_words = len(p.code.split())
        text = capypdf.Text()
        text.cmd_Tf(self.codefont, self.codesize)
        text.cmd_Td(60, self.h - 3.5*self.headingsize)
        text.cmd_TL(1.5 * self.codesize)
        i = 0
        color = capypdf.Color()
        for line in p.code.split('\n'):
            cur = ''
            for word in line.split(' '):
                if not word:
                    cur += ' '
                else:
                    cur += word + ' '
                    text.render_text(cur)
                    r = 1.0*i/num_words
                    if i%2 == 0:
                        r = 1.0-r
                    color.set_rgb(r, 0, 0)
                    text.nonstroke_color(color)
                    cur = ''
                i += 1
            text.cmd_Tstar()
        ctx.render_text_obj(text)

    def add_pages(self, pages):
        for page in pages:
            with self.pdfgen.page_draw_context() as ctx:
                if isinstance(page, TitlePage):
                    self.render_title_page(ctx, page)
                elif isinstance(page, BulletPage):
                    self.render_bullet_page(ctx, page)
                elif isinstance(page, CodePage):
                    self.render_code_page(ctx, page)
                else:
                    raise RuntimeError('Unknown page type.')

    def finish(self):
        self.pdfgen.write()

if __name__ == '__main__':
    p = Demopresentation('demo_presentation.pdf', cm2pt(28), cm2pt(16))
    pages = create_pages()
    p.add_pages(pages)
    p.finish()
