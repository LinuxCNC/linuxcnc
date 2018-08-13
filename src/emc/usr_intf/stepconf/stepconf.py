#!/usr/bin/env python
# -*- encoding: utf-8 -*-
#
#    This is stepconf, a graphical configuration editor for LinuxCNC
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#
#    stepconf 1.1 revamped by Chris Morley 2014
#    replaced Gnome Druid as that is not available in future linux distrubutions
#    and because of GTK/GLADE bugs, the GLADE file could only be edited with Ubuntu 8.04
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
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

import sys
import os
import locale, gettext
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import GObject
from gi.repository import Gdk
from optparse import Option, OptionParser
import xml.dom.minidom
import math
import textwrap
import commands
import shutil
import time
from multifilebuilder_gtk3 import MultiFileBuilder

reload(sys)
sys.setdefaultencoding('utf8')

import traceback

#**********************************
# translations,locale
DOMAIN = "linuxcnc"
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
LOCALEDIR = os.path.join(BASE, "share", "locale")
gettext.install(DOMAIN, localedir=LOCALEDIR, unicode=True)
locale.setlocale(locale.LC_ALL, '')
locale.bindtextdomain(DOMAIN, LOCALEDIR)
gettext.bindtextdomain(DOMAIN, LOCALEDIR)

#**********************************
# Due to traslation put here module with locale
from stepconf.definitions import *
from stepconf import build_INI
from stepconf import build_HAL
from stepconf import data
from stepconf import pages

#**********************************
# otherwise, on hardy the user is shown spurious "[application] closed
# unexpectedly" messages but denied the ability to actually "report [the]
# problem"
def excepthook(exc_type, exc_obj, exc_tb):
	try:
		w = app.w.window1
	except NameError:
		w = None
	lines = traceback.format_exception(exc_type, exc_obj, exc_tb)
	m = Gtk.MessageDialog(w,
			Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
			Gtk.MessageType.ERROR, Gtk.ButtonsType.OK,
			_("Stepconf encountered an error.  The following "
 			"information may be useful in troubleshooting:\n\n")
			+ "".join(lines))
	m.show()
	m.run()
	m.destroy()
sys.excepthook = excepthook

#**********************************
# Widgets class
#**********************************
# a class for holding the glade widgets rather then searching for them each time
class Widgets:
	def __init__(self, xml):
		self._xml = xml
	def __getattr__(self, attr):
		r = self._xml.get_object(attr)
		if r is None:
			raise AttributeError, "No widget %r" % attr
		return r
	def __getitem__(self, attr):
		r = self._xml.get_object(attr)
		if r is None:
			raise IndexError, "No widget %r" % attr
		return r

#**********************************
# Main class
#**********************************
class StepconfApp:
	def __init__(self, dbgstate):
		self.debug = dbgstate

		######################################
		#######  INIT private data ###########
		######################################
		# Private data holds and init paths and general variables.
		self._p = data.Private_Data()
		self._p.debug = self.debug

		######################################
		#########  INIT data #################
		######################################
		self.d = data.Data()
		# Try find parport
		self.d.lparport = self.find_parport()
		# Set xyzuv axes defaults depending on units (MM or INCH)
		self.set_axis_unit_defaults(MM)

		# build the glade files
		self.builder = MultiFileBuilder()
		self.builder.set_translation_domain(DOMAIN)
		self.builder.add_from_file(os.path.join(self._p.datadir,'main_page.glade'))
		window = self.builder.get_object("window1")
		notebook1 = self.builder.get_object("notebook1")
		for reference,title,state in (available_page):
			if reference == 'intro':
				continue
			#dbg("loading glade page REFERENCE:%s TITLE:%s STATE:%s"% (reference,title,state))
			self.builder.add_from_file(os.path.join(self._p.datadir, '%s.glade'%reference))
			page = self.builder.get_object(reference)
			notebook1.append_page(page, Gtk.Label(reference))
		notebook1.set_show_tabs(False)

		self.w = Widgets(self.builder)
		self.p = pages.Pages(self)
		self.INI = build_INI.INI(self)
		self.HAL = build_HAL.HAL(self)
		self.builder.set_translation_domain(DOMAIN) # for locale translations
		self.builder.connect_signals( self.p ) # register callbacks from Pages class
		#wiz_pic = Gdk.pixbuf_new_from_file(wizard)

		image = Gtk.Image()
		image.set_from_file(self._p.wizard)
		wiz_pic = image.get_pixbuf()
		self.w.wizard_image.set_from_pixbuf(wiz_pic)
		self.load_preferences()
		self.p.initialize()
		window.show()
		#self.w.xencoderscale.realize()

	def find_parport(self):
		# Try to find parallel port
		lparport=[]
		# open file.
		try:
			in_file = open("/proc/ioports","r")
		except:
			print "Unable to open /proc/ioports"
			return([])
	
		try:
			for line in in_file:
				if "parport" in line:
					tmprow = line.strip()
					lrow = tmprow.split(":")
					address_range = lrow[0].strip()
					init_address = address_range.split("-")[0].strip()
					lparport.append("0x" + init_address)
		except:
			print "Error find parport"
			in_file.close()
			return([])
		in_file.close()
		if lparport == []:
			return([])
		return(lparport)

	# change the XYZ axis defaults to metric or imperial
	# This only sets data that makes sense to change eg gear ratio don't change
	def set_axis_unit_defaults(self, units=MM):
		if units == INCH: # imperial
			for i in ('x','y','z','a','u','v'):
				self.d[i+'maxvel'] = 1
				self.d[i+'maxacc'] = 30
				self.d[i+'homevel'] = .05
				self.d[i+'leadscrew'] = 20
				if not i == 'z':
					self.d[i+'minlim'] = 0
					self.d[i+'maxlim'] = 8
				else:
					self.d.zminlim = -4
					self.d.zmaxlim = 0
		else: # metric
			for i in ('x','y','z','a','u','v'):
				self.d[i+'maxvel'] = 25
				self.d[i+'maxacc'] = 750
				self.d[i+'homevel'] = 1.5
				self.d[i+'leadscrew'] = 5
				if not i =='z':
					self.d[i+'minlim'] = 0
					self.d[i+'maxlim'] = 200
				else:
					self.d.zminlim = -100
					self.d.zmaxlim = 0

	def load_preferences(self):
		# set preferences if they exist
		link = short = advanced = show_pages = False
		filename = os.path.expanduser("~/.stepconf-preferences")
		if os.path.exists(filename):
			version = 0.0
			d = xml.dom.minidom.parse(open(filename, "r"))
			for n in d.getElementsByTagName("property"):
				name = n.getAttribute("name")
				text = n.getAttribute('value')
				if name == "version":
					version = eval(text)
				if name == "always_shortcut":
					short = eval(text)
				if name == "always_link":
					link = eval(text)
				if name == "sim_hardware":
					sim_hardware = eval(text)
				if name == "machinename":
					self.d._lastconfigname = text
				if name == "chooselastconfig":
					self._chooselastconfig = eval(text)
			# these are set from the hidden preference file
			self.d.createsymlink = link
			self.d.createshortcut = short
			self.d.sim_hardware = sim_hardware
	
	# write stepconf's hidden preference file
	def save_preferences(self):
		filename = os.path.expanduser("~/.stepconf-preferences")
		d2 = xml.dom.minidom.getDOMImplementation().createDocument(
							None, "int-pncconf", None)
		e2 = d2.documentElement
	
		n2 = d2.createElement('property')
		e2.appendChild(n2)
		n2.setAttribute('type', 'float')
		n2.setAttribute('name', "version")
		n2.setAttribute('value', str("%f"%self.d._preference_version))
	
		n2 = d2.createElement('property')
		e2.appendChild(n2)
		n2.setAttribute('type', 'bool')
		n2.setAttribute('name', "always_shortcut")
		n2.setAttribute('value', str("%s"% self.d.createshortcut))
	
		n2 = d2.createElement('property')
		e2.appendChild(n2)
		n2.setAttribute('type', 'bool')
		n2.setAttribute('name', "always_link")
		n2.setAttribute('value', str("%s"% self.d.createsymlink))
	
		n2 = d2.createElement('property')
		e2.appendChild(n2)
		n2.setAttribute('type', 'bool')
		n2.setAttribute('name', "sim_hardware")
		n2.setAttribute('value', str("%s"% self.d.sim_hardware))
	
		n2 = d2.createElement('property')
		e2.appendChild(n2)
		n2.setAttribute('type', 'bool')
		n2.setAttribute('name', "chooselastconfig")
		n2.setAttribute('value', str("%s"% self.d._chooselastconfig))
	
		n2 = d2.createElement('property')
		e2.appendChild(n2)
		n2.setAttribute('type', 'string')
		n2.setAttribute('name', "machinename")
		n2.setAttribute('value', str("%s"%self.d.machinename))

		"""
		n2 = d2.createElement('property')
		e2.appendChild(n2)
		n2.setAttribute('type', 'string')
		n2.setAttribute('name', "units")
		n2.setAttribute('value', str("%s"%self.d.units))
		"""

		d2.writexml(open(filename, "wb"), addindent="  ", newl="\n")



###############################################################################
# starting with 'stepconf -d' gives debug messages
if __name__ == "__main__":
	usage = "usage: Stepconf -[options]"
	parser = OptionParser(usage=usage)
	parser.add_option("-d", action="store_true", dest="debug",help="Print debug info and ignore realtime/kernel tests")
	(options, args) = parser.parse_args()
	if options.debug:
		app = StepconfApp(dbgstate=True)
	else:
		app = StepconfApp(False)

	# Prepare Style
	cssProvider = Gtk.CssProvider()
	cssProvider.load_from_data(style)
	screen = Gdk.Screen.get_default()
	styleContext = Gtk.StyleContext()
	styleContext.add_provider_for_screen(screen, cssProvider, Gtk.STYLE_PROVIDER_PRIORITY_USER)

	Gtk.main()

