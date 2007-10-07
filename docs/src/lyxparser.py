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

import os
import sys
import getopt
import shutil
import StringIO

class LyxParser:
    def __init__(self, handler):
	self.handler = handler
	handler.parser = self
	self.in_verbatim = 0
	self.firstline = True

    def feed(self, line):
	if self.in_verbatim:
	    if line.startswith(self.end_verbatim):
		self.in_verbatim = False
	    else:
		return self.do_verbatim(line)

	if self.firstline and line.startswith("#"): return
	self.firstline = False


	if line.startswith("\\"):
	    tokens = line.split(None, 2)
	    if line.startswith("\\begin_"):
		what = tokens[0][7:]
		self.do_begin(what, tokens[1:])
	    elif line.startswith("\\end_"):		
		what = tokens[0][5:]
		self.do_end(what, tokens[1:])
	    else:
		what = tokens[0][1:]
		self.do_command(what, tokens[1:])
	else:
	    self.do_text(line)

    def do_text(self, line):
	parts = line.split("\\SpecialChar ", 1)
	a = parts[0]
	if a:
	    self.handler.handle_text(a.strip("\n"))
	if len(parts) == 2:
	    self.handler.handle_special_char(parts[1].strip())

    do_verbatim = do_text

    def do_end(self, what, tokens):
	f = getattr(self.handler, "end_" + what, None)
	if f: f(tokens)
	else: self.handler.unknown_end(what, tokens)

    def do_begin(self, what, tokens):
	f = getattr(self.handler, "begin_" + what, None)
	if f: f(tokens)
	else: self.handler.unknown_begin(what, tokens)

    def do_command(self, what, tokens):
	f = getattr(self.handler, "do_" + what, None)
	if f: f(tokens)
	else: self.handler.unknown_command(what, tokens)

    def start_verbatim(self, end_verbatim):
	self.in_verbatim = True
	self.end_verbatim = end_verbatim
