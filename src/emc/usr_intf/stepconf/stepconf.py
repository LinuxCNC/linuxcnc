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
from importlib import import_module

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
from stepconf import preset
from stepconf import build_INI
from stepconf import build_HAL
from stepconf import data

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
		self.p = Pages(self)
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


#**********************************
# Handling pages
#**********************************
class Pages:
	def __init__(self, app):
		self.d = app.d      # collected data
		self.w = app.w      # widget names
		self.a = app        # parent, stepconf
		self._p = app._p    # private data

		# Import all methods from py pages
		for lib in (available_page_lib):
			mod = import_module("stepconf." + lib)
			module_dict = mod.__dict__
			try:
				to_import = mod.__all__
			except AttributeError:
				to_import = [name for name in module_dict if not name.startswith('_')]
	
			for current_function_name in to_import:
				current_function = module_dict[current_function_name]
				self.add_method(current_function, current_function_name)

	def add_method(self, method, name=None):
		if name is None:
			name = method.func_name
		setattr(self.__class__, name, method)

	#********************
	# Notebook Controls
	#********************
	def on_window1_destroy(self, *args):
		if self.warning_dialog (MESS_ABORT,False):
			Gtk.main_quit()
			return True
		else:
			return True
		return

	# seaches (available_page) from the current page forward,
	# for the next page that is True or till second-to-last page.
	# if state found True: call current page finish function.
	# If that returns False then call the next page prepare function and show page
	def on_button_fwd_clicked(self,widget):
		cur = self.w.notebook1.get_current_page()
		u = cur+1
		cur_name,cur_text,cur_state = available_page[cur]
		while u < len(available_page):
			name,text,state = available_page[u]
			self.dbg( "FWD search %s,%s,%s,%s,of %d pages"%(u,name,text,state,len(available_page)-1))
			if state:
				if not self['%s_finish'%cur_name]():
					self.w.notebook1.set_current_page(u)
					self.dbg( 'prepare %s'% name)
					self['%s_prepare'%name]()
					self.w.title_label.set_text(text)
					self.dbg("set %d current"%u)
				break
			u +=1
		# second-to-last page? change the fwd button to finish and show icon
		if u == len(available_page)-1:
			self.w.apply_image.set_visible(True)
			self.w.label_fwd.set_text(MESS_DONE)
		# last page? nothing to prepare just finish
		elif u == len(available_page):
			name,text,state = available_page[cur]
			self['%s_finish'%name]()
		# if comming from page 0 to page 1 sensitize 
		# the back button and change fwd button text
		if cur == 0:
			self.w.button_back.set_sensitive(True)
			self.w.label_fwd.set_text(MESS_FWD)

	# seaches (available_page) from the current page backward,
	# for the next page that is True or till first page.
	# if state found True: call current page finish function.
	# If that returns False then call the next page prepare function and show page
	def on_button_back_clicked(self,widget):
		cur = self.w.notebook1.get_current_page()
		u = cur-1
		cur_name,cur_text,cur_state = available_page[cur]
		while u > -1:
			name,text,state = available_page[u]
			self.dbg( "BACK search %s,%s,%s,%s,of %d pages"%(u,name,text,state,len(available_page)-1))
			if state:
				if not cur == len(available_page)-1:
					self['%s_finish'%cur_name]()
				self.w.notebook1.set_current_page(u)
				self['%s_prepare'%name]()
				self.w.title_label.set_text(text)
				self.dbg("set %d current"%u)
				break
			u -=1
		# Not last page? change finish button text and hide icon
		if u <= len(available_page):
			self.w.apply_image.set_visible(False)
			self.w.label_fwd.set_text(MESS_FWD)
		# page 0 ? de-sensitize the back button and change fwd button text 
		if u == 0:
			self.w.button_back.set_sensitive(False)
			self.w.label_fwd.set_text(MESS_START)
	
	def set_buttons_sensitive(self,fstate,bstate):
		self.w.button_fwd.set_sensitive(fstate)
		self.w.button_back.set_sensitive(bstate)
	
	# Sets the visual state of a list of page(s)
	# The page names must be the one used in available_page
	# If a pages state is false it won't be seen or it's functions called.
	# if you deselect the current page it will show till next time it is cycled
	def page_set_state(self,page_list,state):
		self.dbg("page_set_state() %s ,%s"%(page_list,state))
		for i,data in enumerate(available_page):
			name,text,curstate = data
			if name in page_list:
				available_page[i][2] = state
				self.dbg("State changed to %s"% state)
				break

	#####################################################
	# All Page Methods
	#####################################################
	#***************
	# Intialize
	#***************
	def initialize(self):
		# one time initialized data
		self.w.title_label.set_text(available_page[0][1])
		self.w.button_back.set_sensitive(False)
		self.w.label_fwd.set_text(MESS_START)
		if self._p.debug:
			self.w.window1.set_title('Stepconf -debug mode')
		# halui custom table
		renderer = Gtk.CellRendererText()
		column = Gtk.TreeViewColumn("Index", renderer, text=0)
		column.set_reorderable(False)
		self.w.viewTable1.append_column(column)
		renderer = Gtk.CellRendererText()
		renderer.set_property('editable', True)
		renderer.connect("edited", self.on_halui_row_changed)
		column = Gtk.TreeViewColumn("MDI__COMMAND", renderer, text=1)
		self.w.viewTable1.append_column(column)

		# base
		# Axis
		axis_type = [
			{'type':"XYZ", 'index':XYZ},
			{'type':"XYZA", 'index':XYZA},
			{'type':"XZ (Lathe)", 'index':XZ},
			{'type':"XYUV (Foam)", 'index':XYUV}
		]
		self.w.axis_liststore.clear()
		for mydict in axis_type:
			treeiter = self.w.axis_liststore.append([mydict["type"], mydict["index"]])
		self.w.axes.set_active(0)
		# Machine
		self.w.base_preset_liststore.clear()
		self.w.base_preset_liststore.append([_("Other"), 0])
		for mydict in preset.preset_machines:
			treeiter = self.w.base_preset_liststore.append([mydict["human"], mydict["index"]])
		self.w.base_preset_combo.set_active(0)
		# Driver
		self.w.driver_liststore.clear()
		self.w.driver_liststore.append([_("Other"), 0])
		for mydict in preset.preset_machines:
			treeiter = self.w.driver_liststore.append([mydict["human"], mydict["index"]])
		self.w.drivertype.set_active(0)

		# pport1 combo boxes
		model = self.w.output_list
		model.clear()
		for pin in hal_output:
			model.append((pin["human"],))

		model = self.w.input_list
		model.clear()
		for pin in hal_input:
			model.append((pin["human"],))

		# parport preset
		self.w.pp1_preset_io_liststore.clear()
		for myport in self.d.lparport:
			treeiter = self.w.pp1_preset_io_liststore.append([myport])
		if(self.d.lparport):
			self.w.pp1_preset_io_combo.set_active(0)
			self.d.ioaddr = self.d.lparport[0]

		# preset list for pp1
		self.w.pp1_preset_liststore.clear()
		self.w.pp1_preset_liststore.append([_("Other"), 0])
		for mydict in preset.preset_machines:
			treeiter = self.w.pp1_preset_liststore.append([mydict["human"], mydict["index"]])
		self.w.pp1_preset_combo.set_active(0)

		# pport2 comboboxes
		model = self.w.pp2_output_list
		model.clear()
		for pin in hal_output:
			# First functions not admitted
			if not pin["index"] in( 0,1,2,3,4,5,6,7):
				model.append((pin["human"],))
		model = self.w.pp2_input_list
		model.clear()
		#for name in self._p.human_input_names: model.append((name,))
		for pin in hal_input:
			model.append((pin["human"],))
		self.intro_prepare()

		# axis preset prepare
		for axis in ('x','y','z','u','v'):
			self.w[axis + "preset_liststore"].clear()
			self.w[axis + "preset_liststore"].append([_("Other"), 0])
			for mydict in preset.preset_machines:
				treeiter = self.w[axis + "preset_liststore"].append([mydict["human"], mydict["index"]])
			self.w[axis + "preset_combo"].set_active(0)

		# Options page
		self.w.probe_x_pos.set_text("%d" % self.d.probe_x_pos)
		self.w.probe_y_pos.set_text("%d" % self.d.probe_y_pos)
		self.w.probe_z_pos.set_text("%d" % self.d.probe_z_pos)
		self.w.probe_sensor_height.set_text("%d" % self.d.probe_sensor_height)
	
	#************
	# INTRO PAGE
	#************
	def intro_prepare(self):
		pass
	def intro_finish(self):
		pass

	# BOILER CODE
	def __getitem__(self, item):
		return getattr(self, item)
	def __setitem__(self, item, value):
		return setattr(self, item, value)

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

