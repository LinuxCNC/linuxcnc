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


import sys
import re
import os
from xcommon import *

x = Document(None, "xref", None)

def Node(n, **kw):
    n = x.createElement(n)
    for k, v in kw.items():
	assert isinstance(k, basestring), k
	assert isinstance(v, basestring), (k, v)
	n.setAttribute(k, v)
    x.documentElement.appendChild(n)

for fn in sys.argv[1:]:
    fh = os.path.splitext(fn)[0] + ".html"
    d = parse(fn)
    for n in walkTag(d, 'index'):
	anchor = n.getAttribute('id')
	term = n.getAttribute('term')
	lcterm = n.getAttribute('lcterm')
	Node('index', anchor=anchor, term=term, lcterm=lcterm, src=fh)
    for n in walkTag(d, 'label'):
	anchor = n.getAttribute('id')
	if re.match("^r[0-9_]*$", anchor): continue
	Node('label', anchor=anchor, src=fh)
sys.stdout.write(x.toprettyxml("  "))
