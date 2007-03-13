#!/usr/bin/python
"""mkpre.py: Convert ASCII art into something that groff can render to
text, HTML, or postscript/pdf

Copyright (C) 2006 Jeff EpleR
License: GPL V2
"""

def substall(s, r):
    for a, b in r: s = s.replace(a, b)
    return s

def mkpre(s):
    msubst = [('\\', '\\\\'), (' ', '\\ ')]
    hsubst = [('&', '&amp;'), ('<', '&lt;'), ('>', '&gt;'),
		('  ', ' &nbsp;'), ('\n', '<BR>'), ('\\', '&#92;')]

    h = "".join(s)
    print ".ie '\\*[.T]'html' .HTML <PRE>%s</PRE>" % substall(h, hsubst)
    print ".el \\{\\"
    print ".ft CR"
    for i, line in enumerate(s):
	if i != 0:
	    print r".PP"
	print substall(line.strip("\n"), msubst)
    print ".ft R"
    print r"\}"

import sys
mkpre(list(sys.stdin))
