#!/usr/bin/env python
# coding: utf-8
#
# Copyright (c) 2012 Nick Drobchenko aka Nick from cnc-club.ru
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

################## USAGE #################
#
# make sure this file is executable
# args are :
#    -r, to replace all icons
##########################################

import sys

import pygtk
pygtk.require('2.0')

from lxml import etree
import os
import getopt

optlist, args = getopt.getopt(sys.argv[1:], 'w:r')

optlist = dict(optlist)
renew_all = "-r" in optlist

xml = etree.parse("icons.svg")
for x in xml.findall(".//{http://www.w3.org/2000/svg}title") :
	try :
		id_ = x.getparent().get("id")
		if not os.path.isfile("../%s.png" % (x.text)) or renew_all :
			w = float(os.popen("inkscape icons.svg --query-id=%s --query-width " % id_).read())
			h = float(os.popen("inkscape icons.svg --query-id=%s --query-height" % id_).read())

			if w > h :
				w, h = 80, 80 * h / w
			else :
				h, w = 80, 80 * w / h
			try :
				s = "inkscape icons.svg --export-png=../%s.png --export-id-only --export-id=%s --export-area-snap --export-width=%spx --export-height=%spx " % (x.text, id_, w, h)
				print os.popen(s).read()
				print "Created %s" % x.text
			except Exception, e :
				print e
		else : print "Skipping %s" % x.text
	except Exception, e :
		print
		print "Error with the file %s.png!" % (x.text)
		print e
		print
	print


