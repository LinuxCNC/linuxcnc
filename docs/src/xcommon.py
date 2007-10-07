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

from xml.dom.minidom import getDOMImplementation, parse, parseString
import xml.dom.minidom

Document = getDOMImplementation().createDocument
TEXT_NODE = xml.dom.minidom.Node.TEXT_NODE
ELEMENT_NODE = xml.dom.minidom.Node.ELEMENT_NODE

def walkTag(doc, tag, root=None):
    if root is None: root = doc.documentElement
    for n in root.childNodes:
	if n.nodeType != ELEMENT_NODE: continue
	if n.tagName == tag:
	    yield n
	for m in walkTag(doc, tag, n):
	    yield m

del getDOMImplementation
