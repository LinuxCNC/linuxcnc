#!/usr/bin/python
#    This is l2h, a converter from lyx to html
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import tempfile
import sha
import sys
import getopt
import os
import re
import subprocess
import xml.dom.minidom
import mimetypes
import shutil

from xcommon import *

from lyxparser import LyxParser

def getoutput(args):
    return subprocess.Popen(args, stdout=subprocess.PIPE).communicate()[0]

formula_replacements = (
    (r'\Omega ', u'\N{GREEK CAPITAL LETTER OMEGA}'),
    (r'\Omega', u'\N{GREEK CAPITAL LETTER OMEGA}'),
    (r'\omega ', u'\N{GREEK SMALL LETTER OMEGA}'),
    (r'\omega', u'\N{GREEK SMALL LETTER OMEGA}'),
    (r'\mu ', u'\N{GREEK SMALL LETTER MU}'),
    (r'\mu', u'\N{GREEK SMALL LETTER MU}'),
    (r'^{\textrm{TM}}', u'\N{TRADE MARK SIGN}'),
    (r'\textrm', u''),
)

class LyxTreeMaker:
    def __init__(self, indir):
	self.d = Document(None, 'lyx', None)
	self.e = self.d.documentElement
	self.indir = indir
	self.dummy_inset = []

    def add(self, tagname, cnf={}, **kw):
	#if tagname == 'layout': raise RuntimeError
	assert isinstance(tagname, basestring)
	n = self.d.createElement(tagname)
	for k, v in cnf.items():
	    if k.endswith('_'): k = k[:-1]
	    n.setAttribute(k, v)
	for k, v in kw.items():
	    if k.endswith('_'): k = k[:-1]
	    n.setAttribute(k, v)
	self.e.appendChild(n)
	return n

    def begin_inset(self, tokens):
	dummy_inset = 1
	k = tokens[0]
	if len(tokens) > 1: v = tokens[1]
	if k == 'LatexCommand':
	    if v.startswith("\\label{"):
		self.add('label', id=v[7:-1])
	    elif v.startswith("\\index{"):
		self.add('index', id=v[7:-1])
	    elif v.startswith("\\ref{"):
		self.add('ref', target=v[5:-1])
	    elif v.startswith("\\url{"):
		url=v[5:-1]
		self.push('htmlurl', url=url)
		self.text(url)
		self.pop()
	    elif v.startswith("\\htmlurl{"):
		url=v[9:-1]
		self.push('htmlurl', url=url)
		self.text(url)
		self.pop()
	    elif v.startswith("\\url["):
		text, url = re.match(r"\\url\[(.*)\]{(.*)}", v).group(1,2)
		self.push('htmlurl', url=url)
		self.text(text)
		self.pop()
	    elif v.startswith("\\htmlurl["):
		text, url = re.match(r"\\htmlurl\[(.*)\]{(.*)}", v).group(1,2)
		self.push('htmlurl', url=url)
		self.text(text)
		self.pop()
	    else:
		self.add('latex', data=tokens[1])
	elif k == 'Quotes':
	    if tokens[1] == 'eld':
		self.text(u"\N{left double quotation mark}")
	    elif tokens[1] == 'erd':
		self.text(u"\N{right double quotation mark}")
	    else:
		self.add('quote', class_=tokens[1])
	elif k == 'Formula':
	    dummy_inset = "formula"
	    self.push('formula')
	    self.parser.start_verbatim("\\end_inset")
	    if len(tokens) > 1: self.text(tokens[1])
	elif k == 'Text':
	    dummy_inset = 'text'
	    self.push('text')
	elif k == 'Foot':
	    dummy_inset = 'footnote'
	    self.push('footnote')
	elif k == 'Tabular':
	    dummy_inset = 'tabular'
	    self.push('tabular')
	elif k == 'Float':
	    dummy_inset = 'float'
	    self.push('float', class_=v)
	elif k == 'Wrap':
	    dummy_inset = 'float'
	    self.push('float', class_=v)
	elif k == 'Graphics':
	    dummy_inset = 'graphics'
	    self.push('graphics')
	elif k == "Include":
	    if v.startswith("\\verbatiminput"):
		fn = v[15:-1]
		fn = os.path.join(self.indir, fn)
		try:
		    f = open(fn).read()
		except IOError, detail:
		    f = str(detail)
		    print >>sys.stderr, \
			"# Error including %r:\n#    %s" % (fn,detail)
		self.text(f)
		dummy_inset = 0
		self.push('inset')
	else:
	    dummy_inset = 0
            if len(tokens) == 0:
                self.push('inset')
            elif len(tokens) == 1 and not tokens[0].startswith('['):
                self.push('inset', data=tokens[0])
            else:
                self.push('inset', data=str(tokens))
	self.dummy_inset.append(dummy_inset)

    def end_inset(self, text):
	dummy_inset = self.dummy_inset.pop()
	if isinstance(dummy_inset, basestring):
	    while self.e.tagName != dummy_inset: self.pop()
	    self.pop()
	elif not dummy_inset:
	    while self.e.tagName != 'inset': self.pop()
	    self.pop()

    def text(self, text):
	assert isinstance(text, basestring)
	if self.e.lastChild and self.e.lastChild.nodeType == TEXT_NODE:
	    self.e.lastChild.data += text
	else:
	    self.e.appendChild(self.d.createTextNode(text))

    def push(self, tagname, cnf={}, **kw):
	self.e = self.add(tagname, cnf, **kw)

    def pop(self, tag=None):
	assert self.e is not self.d.documentElement
	if tag: assert self.e.tagName == tag, (self.e.tagName, tag)
	self.e = self.e.parentNode

    def begin_deeper(self, tokens):
	self.push('deeper')
    def end_deeper(self, tokens):
	if self.e.tagName == 'layout': self.pop()
	self.pop('deeper')

    def do_layout(self, tokens):
	if self.e.tagName == 'layout': self.pop()
	self.push('layout', class_=tokens[0])
	
    def do_align(self, tokens):
	self.add('align', side=tokens[0])
    
    def do_series(self, tokens): self.add('font', series=tokens[0])
    def do_family(self, tokens): self.add('font', family=tokens[0])
    def do_emph(self, tokens): self.add('font', emph=tokens[0])
    def do_noun(self, tokens): self.add('font', noun=tokens[0])

    def do_newline(self, tokens): self.add('newline')
    def do_backslash(self, tokens): self.text('\\')

    def begin_preamble(self, tokens):
        if len(tokens) == 0:
            self.push('preamble')
        elif len(tokens) == 1 and not tokens[0].startswith('['):
            self.push('preamble', data=tokens[0])
        else:
            self.push('preamble', data=str(tokens))
	self.parser.start_verbatim("\\end_preamble")

    def handle_special_char(self, ch):
	if ch == '\\-': ch = u'-'
	if ch == '\\ldots{}': ch = u'\N{horizontal ellipsis}'
	elif ch == '~': ch = u'\N{no-break space}'
	self.text(ch)

    def do_SpecialChar(self, tokens):
	self.handle_special_char(tokens[0])

    def handle_text(self, line):
        if self.e.tagName == 'layout':
            self.text(line)
        else:
            parts = line.split()
            if not parts: return
            if not re.match("^[a-zA-Z_][a-zA-Z0-9_]*$", parts[0]):
                self.text(line)
            else:
                if len(parts) == 1:
                    self.e.setAttribute(parts[0], "1")
                elif len(parts) == 2:
                    self.e.setAttribute(parts[0], parts[1])
                else:
                    self.e.setAttribute(parts[0], str(parts[1:]))

    def unknown_begin(self, what, tokens):
        if len(tokens) == 0:
            self.push(what)
        elif len(tokens) == 1 and not tokens[0].startswith('['):
            self.push(what, data=tokens[0])
        else:
            self.push(what, data=str(tokens))

    def unknown_end(self, what, tokens):
	self.pop(what)

    def unknown_command(self, what, tokens):
        if len(tokens) == 0:
            self.add(what)
        elif len(tokens) == 1 and not tokens[0].startswith('['):
            self.add(what, data=tokens[0])
        else:
            self.add(what, data=str(tokens))
	
    def do_the_end(self, tokens): pass

def LyxFontFixer(doc):
    """Change series/family/emph/noun <tags/> into style tags which surround
    the affected text"""

    keys = 'series', 'family', 'emph', 'noun'
    for layout in walkTag(doc, 'layout'):
	state = {'series': 'default', 'family': 'default',
		 'emph': 'default', 'noun': 'default'}
	for node in walkTag(doc, 'font', layout):
	    for attr in keys:
		if not node.hasAttribute(attr):
		    node.setAttribute(attr, state[attr])
	    while (node.nextSibling
		    and node.nextSibling.nodeType == ELEMENT_NODE
		    and node.nextSibling.tagName == 'font'):
		sib = node.nextSibling
		for attr in keys:
		    value = sib.getAttribute(attr)
		    if value: node.setAttribute(attr, value)
		node.parentNode.removeChild(sib)
	    for attr in keys:
		state[attr] = node.getAttribute(attr)


    for node in doc.getElementsByTagName('font'):
	for attr in keys:
	    value = node.getAttribute(attr)
	    if value == 'default': node.removeAttribute(attr)
        while (node.nextSibling and (node.nextSibling.nodeType != ELEMENT_NODE
                or (node.nextSibling.tagName != 'font'
		    and node.nextSibling.tagName != 'deeper'))):
            sib = node.nextSibling
            node.parentNode.removeChild(sib)
            node.appendChild(sib)

class Counter:
    def __init__(self, value=None):
        if value is None: value = []
        self.value = list(value)

    def advance(self):
        self.value[-1] += 1
    def push(self, value=0):
        self.value.append(value)
    def pop(self):
        self.value.pop()
    def level(self, n):
        while n > len(self.value): self.push()
        while n < len(self.value): self.pop()
        self.advance()
    def render(self):
        return ".".join(str(i) for i in self.value)
    def anchor(self):
        return "r" + "_".join(str(i) for i in self.value)

def getNodeText(n):
    result = []
    for l in n.childNodes:
        if l.nodeType == TEXT_NODE: result.append(l.data)
        if l.nodeType == ELEMENT_NODE and l.nodeName == 'font':
            result.append(getNodeText(l))
    return "".join(result)

def getNodeAnchor(n):
    for l in n.childNodes:
        if l.nodeType == ELEMENT_NODE and l.nodeName == 'label':
            return l.getAttribute('id')

tag2nesting = {'Chapter': 1, 'Section': 2, 'Subsection': 3, 'Subsubsection': 4}
def LyxTocXml(lyxdoc):
    def Node(p, n, **kw):
        n = lyxdoc.createElement(n)
        for k, v in kw.items():
            assert isinstance(k, basestring), k
            assert isinstance(v, basestring), (k, v)
            n.setAttribute(k, v)
        if p is not None: p.appendChild(n)
        return n

    n = Node(None, 'toc')
    lyxdoc.documentElement.insertBefore(n, lyxdoc.documentElement.firstChild)

    lastlevel = 0
    counter = Counter()
    for l in lyxdoc.documentElement.childNodes:
        if l.nodeType != ELEMENT_NODE: continue
        if l.nodeName != 'layout': continue
        klass = l.getAttribute('class').strip("*")
	l.setAttribute('class', klass)
        level = tag2nesting.get(klass, None)
        if level is None: continue
        counter.level(level)
        anchor = getNodeAnchor(l)
	if not anchor:
	    anchor = counter.anchor()
	    Node(l, 'label', id=anchor)
	cr = counter.render()
        text= "%s. %s" % (cr, getNodeText(l))
	l.insertBefore(lyxdoc.createTextNode(cr + " "), l.firstChild)
        Node(n, 'tocentry', href="#" + anchor, level=str(level)).appendChild(lyxdoc.createTextNode(text))


def LyxTableFixer(d):
    def maketag(c):
        tagname = c[1:-1].split()[0]
        result = d.createElement(tagname)
        for k, v in re.findall("([a-z]*)=\"([a-z]*)\"", c):
            result.setAttribute(k, v)
        return result

    def lyxtabular_to_nodes(where, data):
        chunks = re.split("(<[^<]*>)", data)
        for c in chunks:
            c = c.strip()
            if not c: continue
            assert c.startswith("<")
            if c.startswith("</"):
                where = where.parentNode
                continue
            c = maketag(c)
            if c.tagName in ('row', 'cell'):
                where.appendChild(c)
                where = c
        return where

    for l in d.getElementsByTagName('tabular'):
        children = list(l.childNodes)
        for n in children: l.removeChild(n)
        where = l
        for n in children:
            if n.nodeType == TEXT_NODE:
                where = lyxtabular_to_nodes(where, n.data)
            elif n.nodeType == ELEMENT_NODE and n.nodeName == 'text':
                for m in n.childNodes:
                    where.appendChild(m)
            else:
                where.appendChild(n)

def LyxGroupFixer(d, e, f, g):
    for n in d.getElementsByTagName('layout'):
	if n.getAttribute('class') != e: continue
	n.tagName = g
	n.removeAttribute('class')
	if n.parentNode.tagName == f: continue
	sib = n.previousSibling
	if sib and sib.tagName == f:
	    n.parentNode.removeChild(n)
	    sib.appendChild(n)
	else:
	    sib = d.createElement(f)
	    n.parentNode.insertBefore(sib, n)
	    n.parentNode.removeChild(n)
	    sib.appendChild(n)

def LyxGroupFixer2(d, e, f, g, h):
    LyxGroupFixer(d,e,f,h)
    for n in d.getElementsByTagName(h):
	text = [j for j in n.childNodes if j.nodeType == TEXT_NODE]
	if not text: continue
	text = text[0]
	split = re.split('[ \t\n]', text.data, 1) # Must not include non-break space
	assert len(split) in (1,2)
	if len(split) == 2:
	    first, rest = split
	else:
	    first, rest = split[0], ''
	assert isinstance(first, basestring)
	assert isinstance(rest, basestring)
	text.data = rest
	sib = d.createElement(g)
	sib.appendChild(d.createTextNode(first))
	n.parentNode.insertBefore(sib, n)

def mtime(fn):
    return os.stat(fn).st_mtime

def LyxGraphicsFixer(d, srcdir, destdir):
    for n in d.getElementsByTagName('graphics'):
	fn = n.getAttribute('filename')
	if not fn:
	    n.parentNode.removeChild(n)
	    continue
	srcfile = os.path.join(srcdir, fn)
	if not os.path.exists(srcfile):
	    print >>sys.stderr, "Image does not exist", fn, srcfile
	    continue
	head, tail = os.path.split(fn)
	base, ext = os.path.splitext(tail)

	if ext in ['.gif', '.png', '.jpg', '.svg']: oext = ext
	else: oext = '.png'

	destfile = os.path.join(destdir, base + oext)

	if not (os.path.exists(destfile) and mtime(destfile) > mtime(srcfile)):
	    if ext == oext:
		shutil.copy(srcfile, destfile)
	    else:
		print >>sys.stderr, "# convert", base,
		retval = os.spawnvp(os.P_WAIT, 'convert',
			['convert',
			    '-density', '96',
			    '-resize', '1000x9999>',
			    srcfile, destfile])	
		if retval:
		    print >>sys.stderr, " ->", retval
		else:
		    print >>sys.stderr
		if not os.path.exists(destfile): raise RuntimeError, "convert failed"

	w, h = getoutput(['identify', '-format', '%w %h', destfile]).split()
	n.setAttribute('src', base + oext)
	n.setAttribute('width', w)
	n.setAttribute('height', h)
   	
def ListFixer(d):
    for n in d.getElementsByTagName('layout'):
	if n.getAttribute('class') != "List": continue
	text = [j for j in n.childNodes if j.nodeType == TEXT_NODE]
	if not text: continue
	text = text[0]
	split = re.split('[ \t\n]', text.data, 1) # Must not include non-break space
	assert len(split) in (1,2)
	if len(split) == 2:
	    first, rest = split
	else:
	    first, rest = split[0], ''
	assert isinstance(first, basestring)
	assert isinstance(rest, basestring)
	text.data = rest
	n.setAttribute('class', 'Itemize')
	
EquationTemplate = """
\\documentclass[12pt]{article} 
\\usepackage{amsmath}
\\usepackage{amsthm}
\\usepackage{amssymb}
\\usepackage{bm}
\\newcommand{\\mx}[1]{\\mathbf{\\bm{#1}}} %% Matrix command
\\newcommand{\\vc}[1]{\\mathbf{\\bm{#1}}} %% Vector command 
\\newcommand{\\T}{\\text{T}}                %% Transpose
\\pagestyle{empty} 
\\begin{document} 
%s
\\end{document}
"""

def EquationProcess(v0, outdir):
    v = EquationTemplate % v0
    ref = sha.new(v).hexdigest()
    fn = os.path.join(outdir, ref + ".png")
    if not os.path.exists(fn):
	print "# Formatting equation %s\n#\t%s" % (ref, repr(v0)[:70])
	d = tempfile.mkdtemp()

	od = os.getcwd()

	ft = os.path.join(d, "eqn.tex")
	fd = os.path.join(d, "eqn.dvi")
	fp = os.path.join(d, "eqn1.png")
	open(ft, "w").write(v)
	    
	os.chdir(d)
	try:
	    res = os.spawnvp(os.P_WAIT, 'latex', [
                'latex', '-interaction', 'batchmode', ft])
	    if res: raise RuntimeError, "latex failed (%d)" % res
	    res = os.spawnvp(os.P_WAIT, 'dvipng', [
                'dvipng', '-D', '115', '-T', 'tight', '-bg', 'Transparent', fd])
	    if res: raise RuntimeError, "dvipng failed (%d)" % res
	finally:
	    os.chdir(od) 

	shutil.copy(fp, fn)
	shutil.rmtree(d)
    w, h = getoutput(['identify', '-format', '%w %h', fn]).split()
    return ref, w, h

def EquationFixer(d, outdir):
    for n in d.getElementsByTagName('formula'):
	v0 = v = getNodeText(n)
	for v1, v2 in formula_replacements:
	    v = v.replace(v1, v2)
	if v.startswith("\\begin{eqnarray"):
	    n.setAttribute("class", "blockformula")
	if v.startswith("\\[") and v.endswith("\\]"):
	    n.setAttribute("class", "blockformula")
	    v = v[2:-2]
	elif v.startswith("$$") and v.endswith("$$"):
	    n.setAttribute("class", "blockformula")
	    v = v[2:-2]
	elif v.startswith("$") and v.endswith("$"):
	    v = v[1:-1]

	if '\\' in v or "{" in v:
	    ref, w, h = EquationProcess(v0, outdir)
	    n.setAttribute('ref', ref)
	    n.setAttribute('width', w)
	    n.setAttribute('height', h)
	else:
	    n.setAttribute('processed', '1')
	    for k in n.childNodes: n.removeChild(k)
	    n.appendChild(d.createTextNode(v))

def IndexFixer(d):
    u = {}
    for n in d.getElementsByTagName('index'):
	id0 = id = n.getAttribute("id")
	n.setAttribute("term", id)
	n.setAttribute("lcterm", id.lower())
	if id in u:
	    id = "%s--%d" % (id0, u[id])
	    n.setAttribute("id", id)
	u[id0] = u.get(id0, 0) + 1

def parse(args):
    opts, args = getopt.getopt(args, "s:d:D:o:t:")

    outdir = None
    outfile = None
    infile = None
    indir = None
    stylesheet=None

    for k, v in opts:
	if k == '-s': stylesheet = v
	if k == '-D': indir = v
	if k == '-d': outdir = v
	if k == '-o': outfile = v

    if len(args) == 1:
	infile = args[0]
    elif args:
	raise SystemExit, "Too many positional arguments: %s" % args[1:]
    else: infile = '-'

    if outdir is None:
	if outfile: outdir = os.path.dirname(outfile)
	else: outfile = '.'
    if outfile is None:
	if outdir and infile:
	    base = os.path.splitext(os.path.basename(infile))[0]
	    outfile = os.path.join(outdir, base)
	elif outdir:
	    outfile = os.path.join(outdir, "index.xml")
    if indir is None:
	indir = os.path.dirname(infile)
    if outdir and not os.path.isdir(outdir): os.makedirs(outdir)

    if isinstance(infile, str):
	if infile == '-': infile = sys.stdin
	else: infile = open(infile)
    infile = (line.decode('latin-1') for line in infile)

    h = LyxTreeMaker(indir)
    p = LyxParser(h)
    for line in infile: p.feed(line.strip('\n'))
    d = h.d
    if stylesheet:
	if stylesheet.endswith(".xsl"):
	    type = "text/xsl"
	else:
	    type = mimetypes.guess_type(stylesheet)[0]
	if outdir:
	    base = os.path.basename(stylesheet)
	    target = os.path.join(outdir, base)
	    print >>sys.stderr, "copy stylesheet", target, stylesheet
	    if not os.path.exists(target):
		shutil.copy(stylesheet, target)
	    stylesheet = base
	stylesheet = d.createProcessingInstruction("xml-stylesheet",
			"type=\"%s\" href=\"%s\"" % (type, stylesheet))
	d.insertBefore(stylesheet, d.firstChild)
    LyxGroupFixer2(d, 'Description', 'descr', 'term', 'desc')
    LyxFontFixer(d)
    LyxTableFixer(d)
    LyxTocXml(d)
    ListFixer(d)
    IndexFixer(d)
    EquationFixer(d, outdir)
    LyxGroupFixer(d, 'Itemize', 'itemize', 'item')
    LyxGroupFixer(d, 'Enumerate', 'enumerate', 'item')
    LyxGraphicsFixer(d, indir, outdir)

    return d, outfile

if __name__ == '__main__':
    doc, outfile = parse(sys.argv[1:])
    docstr = (doc.toxml(encoding='utf-8')
        .replace("<layout", "\n<layout")
        .replace("<tocentry", "\n<tocentry")
        .replace("<tabular", "\n<tabular")
        .replace("<item", "\n<item") + "\n")
    if outfile:
	open(outfile, "w").write(docstr)
    else:
	print docstr
