#!/usr/bin/env python
# vim: sts=4 sw=4 et

import os,sys,re

import lxml.etree as ET

tree = ET.parse(sys.stdin)
root = tree.getroot()

auto_href = 'r[0-9]+(_[0-9]+)*'
auto_href_re = re.compile(auto_href)
auto_href_hash = re.compile('#' + auto_href)

def _splitwidth(l, width=72):
    if len(l) <= width:
        return [l]
    ll = l.split(' ')
    r = []
    l = ''
    for w in ll:
        if len(l) + len(w) + 1 < width:
            l += ' ' + w
            continue
        r.append(l)
        l = w
    if l:
        r.append(l)
    return r

def splitwidth(txt, width=72):
    r = []
    for l in txt.splitlines():
        r.extend(_splitwidth(l))
    return r

def splitshift(l, shift=0):
    return ('\n' + ' ' * shift).join(splitwidth(l, 72))

def load_toc(tree):
    d = {}
    r = tree.xpath('/lyx/toc')
    if not r:
        return d
    for e in r[0].xpath('tocentry'):
        if auto_href_hash.match(e.attrib['href']):
            continue
        num, txt = e.text.split(' ', 1)
        if num.endswith('.'):
            num = num[:-1]
        d['%s %s' % (num,txt)] = e.attrib['href']
    return d

toc = load_toc(tree)

sections = {'Chapter':1, 'Section':2, 'Subsection':3, 'Subsubsection':4, 'Subparagraph':5}

def inline_label(e):
    l = e.attrib['id']
    if auto_href_re.match(l):
        return ''
    if ',' in l: l = '"' + l + '"'
    return '[[' + l + ']]'

def inline_index(e):
    return '(((' + e.attrib['term'] + ')))'

def inline_ref(e):
    target = e.attrib['target']
    if ',' in target:
        return ' <<"' + target + '">>'
    else:
        return ' <<' + target + '>>'

def inline_formula(e):
    return 'latexmath:[' + (e.text or '') + ']'

def inline_htmlurl(e):
    return e.attrib['url'] + '[' + (e.text or '') + ']'

def inline_font(e):
    a = dict(e.items())
    if a.get('family') == 'typewriter':
        c = '`'
    elif a.get('emph') == 'on':
        c = "'"
    elif a.get('series') == 'bold':
        c = '*'
    else:
        c = ''
    inline = process_inline(e)
    if not inline:
        return ''
    return c + inline + c

def inline_footnote(e):
    return 'footnote:[' + process_block(e) + ']'

def inline_float(e):
    return "\n" + process_block(e) + "\n"

def inline_tabular(e):
    txt = ['[width="90%", options="header"]']
    txt += ['|' + '=' * 40]
    for r in e.xpath('row'):
        row = []
        for c in r.xpath('cell'):
            row.append(process_block(c).strip())
        txt.append('|' + ' | '.join(row))
    txt += ['|' + '=' * 40]
    return '\n'.join(txt)

def inline_graphics(e):
    if e.getparent().getparent().tag != 'float':
        extra = ''
    else:
        extra = ':'
    return 'image:' + extra + e.attrib['filename'] + '[]'

def inline_deeper(e):
    ll = process_block(e).splitlines()
    for i,l in enumerate(ll):
        if not l.strip():
            ll[i] = '+' + l
    return '\n' + '\n'.join(ll)

def process_inline(e, text=True):
    if text:
        txt = e.text or ''
    else:
        txt = ''
    for c in e.getchildren():
        if c.tag == 'label':
            txt += inline_label(c)
        elif c.tag == 'index':
            txt += inline_index(c)
        elif c.tag == 'ref':
            txt += inline_ref(c)
        elif c.tag == 'font':
            txt += inline_font(c)
        elif c.tag == 'footnote':
            txt += inline_footnote(c)
        elif c.tag == 'float':
            txt += inline_float(c)
        elif c.tag == 'tabular':
            txt += inline_tabular(c)
        elif c.tag == 'graphics':
            txt += inline_graphics(c)
        elif c.tag in ['color', 'size', 'shape']:
            txt += process_inline(c)
        elif c.tag in ['newline']:
            txt += ' +\n'
        elif c.tag in ['deeper']:
            txt += inline_deeper(c)
        elif c.tag in ['formula']:
            txt += inline_formula(c)
        elif c.tag in ['htmlurl']:
            txt += inline_htmlurl(c)
        elif c.tag in [ 'align', 'noindent', 'inset', 'pagebreak_bottom'
                      , 'ert', 'labelwidthstring', 'added_space_bottom'
                      , 'hfill', 'added_space_top', 'bar']:
            pass
        else:
            sys.stderr.write('>>> Unknown inline tag: %s\n' % c.tag)
        txt += c.tail or ''
    return splitshift(txt, 0)

def layout_section(e):
    level = sections[e.attrib['class']]
    #label = toc.get(e.text)
    txt = "=" * level
    txt += ' '
    #if label:
    #    txt += '[[' + label + ']] '
    inline = process_inline(e, text=False)
    if level != 5:
        txt += e.text.split(' ', 1)[1]
    else:
        txt += e.text
    txt += (inline and ' ') + inline
    return ' '.join(txt.splitlines())

def layout_standard(e):
    return process_inline(e)

def layout_caption(e):
    return '.' + process_inline(e)

def layout_code(e):
    txt = e.text or ''
    if '\n' in txt:
        return '\n'.join(['', '-' * 40, txt, '-' * 40, ''])
    n = e.getnext()
    if n is None or n.tag != 'layout' or n.attrib['class'] != 'LyX-Code':
        extra = '\n'
    else:
        extra = ''
    return '    ' + (e.text or '') + extra

def layout_quote(e):
    return '    ' + splitshift(process_inline(e), 4)

def layout_quotation(e):
    n = e.getnext()
    close = open = False
    if n is None or n.tag != 'layout' or n.attrib['class'] != 'Quotation':
        close = True
    n = e.getprevious()
    if n is None or n.tag != 'layout' or n.attrib['class'] != 'Quotation':
        open = True
    txt = []
    if open:
        txt += ['[quote]', "_" * 40]
    txt += [process_inline(e)]
    if close:
        txt += ["_" * 40]
    return '\n'.join(txt) + "\n"

def block_layout(e):
    klass = e.attrib['class']
    if klass  in sections:
        return layout_section(e) + "\n"
    elif klass == 'Standard':
        return layout_standard(e).strip() + "\n"
    elif klass == 'Caption':
        return layout_caption(e) + "\n"
    elif klass == 'LyX-Code':
        return layout_code(e)
    elif klass == 'Quote':
        return layout_quote(e) + "\n"
    elif klass == 'Quotation':
        return layout_quotation(e) + "\n"
    else:
        sys.stderr.write('>>> Unknown layout class: %s\n' % klass)
    return ''

def parents(elt):
    e = elt.getparent()
    while e is not None:
        yield e
        e = e.getparent()

def block_descr(elt):
    nest = filter(lambda e: e.tag == 'descr', parents(elt))
    if not nest:
        sep = '::'
    elif len(nest) == 1:
        sep = ';;'
    else:
        sep = ':::'
    txt = []
    for e in elt.getchildren():
        if e.tag == 'term':
            txt.append(process_inline(e) + sep)
        elif e.tag == 'desc':
            txt.append('    ' + splitshift(process_inline(e), 4) + '\n')
    return '\n'.join(txt)

def block_enumerate(elt):
    txt = []
    for e in elt.xpath('item'):
        txt.append(' . ' + splitshift(process_inline(e), 3))
    return '\n'.join(txt) + "\n"

def block_itemize(elt):
    txt = []
    for e in elt.xpath('item'):
        txt.append(' - ' + splitshift(process_inline(e), 3))
    return '\n'.join(txt) + "\n"

def process_block(elt):
    txt = []
    for e in elt.getchildren():
        if e.tag == 'layout':
            txt.append(block_layout(e))
        elif e.tag == 'descr':
            txt.append(block_descr(e))
        elif e.tag == 'enumerate':
            txt.append(block_enumerate(e))
        elif e.tag == 'itemize':
            txt.append(block_itemize(e))
        else:
            pass
    return '\n'.join(txt)

txt = process_block(tree.getroot())
print txt.encode('utf-8')
