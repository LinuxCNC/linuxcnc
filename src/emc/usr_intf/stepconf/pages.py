#!/usr/bin/env python
# -*- encoding: utf-8 -*-
#
#    This is stepconf, a graphical configuration editor for LinuxCNC
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#    stepconf 1.1 revamped by Chris Morley 2014
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
# This presents and collects the data from the GUI pages
#
# To add pages:
# add the glade file to directory. the first container name will be the reference name (delete the window)
# add the reference name, Text name and active state to private data variable: self.available_page of stepconnf.py
# add function call here: <reference name>_prepare() and <reference name>_finish()
# add GLADE callbacks for the page here.
# add large or common function calls to stepconf.py

import os
import sys
import shutil
import errno
import commands
import hashlib
from gi.repository import Gtk
from gi.repository import GObject
import xml.dom.minidom
from importlib import import_module

from stepconf.definitions import *
from stepconf import preset
from stepconf.axis_helper_functions import *

reload(sys)
sys.setdefaultencoding('utf8')

#**********************************
# Handling pages
#**********************************
class Pages:
<<<<<<< HEAD
	def __init__(self, app):
		self.d = app.d      # collected data
		self.w = app.w      # widget names
		self.a = app        # parent, stepconf
		self._p = app._p    # private data

		"""
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
		"""
	"""
	def add_method(self, method, name=None):
		if name is None:
			name = method.func_name
		setattr(self.__class__, name, method)
	"""

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

		# preset list spindle
		self.w.spindle_preset_liststore.clear()
		self.w.spindle_preset_liststore.append([_("Other"), 0])
		for mydict in preset.preset_machines:
			treeiter = self.w.spindle_preset_liststore.append([mydict["human"], mydict["index"]])
		self.w.spindle_preset_combo.set_active(0)

		# Options page
		self.w.probe_x_pos.set_text("%d" % self.d.probe_x_pos)
		self.w.probe_y_pos.set_text("%d" % self.d.probe_y_pos)
		self.w.probe_z_pos.set_text("%d" % self.d.probe_z_pos)
		self.w.probe_sensor_height.set_text("%d" % self.d.probe_sensor_height)

	####################################################################
	####################################################################
	#************
	# MAIN PAGE
	#************
	# UNUSED
	def main_page_prepare():
		print("PREPARE")
		pass
		
	def main_page_finish():
		print("FINISH")
		pass

	####################################################################
	####################################################################
	#************
	# INTRO PAGE
	#************
	def intro_prepare(self):
		pass
	def intro_finish(self):
		pass

	####################################################################
	####################################################################
	#***********
	# start PAGE
	#***********
	def start_prepare(self):
		self.w.createsymlink.set_active(self.d.createsymlink)
		self.w.createshortcut.set_active(self.d.createshortcut)
		self.w.createsimconfig.set_active(self.d.sim_hardware)
	
	def start_finish(self):
		if self.w.importmach.get_active():
			print 'Import Mach config'
			from stepconf import import_mach
			self.load('/tmp/temp.stepconf', self)
			if not self._p.debug:
				os.remove('/tmp/temp.stepconf')
		elif not self.w.createconfig.get_active():
			filter = Gtk.FileFilter()
			filter.add_pattern("*.stepconf")
			filter.set_name(_("LinuxCNC 'stepconf' configuration files"))
			dialog = Gtk.FileChooserDialog(_("Modify Existing Configuration"),
				self.w.window1, Gtk.FileChooserAction.OPEN,
				(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
				Gtk.STOCK_OPEN, Gtk.ResponseType.OK))
			dialog.set_default_response(Gtk.ResponseType.OK)
			dialog.add_filter(filter)
			if not self.d._lastconfigname == "" and self.d._chooselastconfig:
				dialog.set_filename(os.path.expanduser("~/linuxcnc/configs/%s.stepconf"% self.d._lastconfigname))
			dialog.add_shortcut_folder(os.path.expanduser("~/linuxcnc/configs"))
			dialog.set_current_folder(os.path.expanduser("~/linuxcnc/configs"))
			dialog.show_all()
			result = dialog.run()
			if result == Gtk.ResponseType.OK:
				filename = dialog.get_filename()
				dialog.destroy()
				self.load(filename, self)
			else:
				dialog.destroy()
				return True
		self.d.createsymlink = self.w.createsymlink.get_active()
		self.d.createshortcut = self.w.createshortcut.get_active()
		self.d.sim_hardware = self.w.createsimconfig.get_active()
	
	# callbacks
	def on_machinename_changed(self, *args):
		temp = self.w.machinename.get_text()
		self.w.confdir.set_text("~/linuxcnc/configs/%s" % temp.replace(" ","_"))
	
	
	def load(self, filename, app=None, force=False):
		def str2bool(s):
			return s == 'True'
	
		converters = {'string': str, 'float': float, 'int': int, 'bool': str2bool, 'eval': eval}
	
		d = xml.dom.minidom.parse(open(filename, "r"))
		for n in d.getElementsByTagName("property"):
			name = n.getAttribute("name")
			conv = converters[n.getAttribute('type')]
			text = n.getAttribute('value')
			setattr(self.d, name, conv(text))
	
		warnings = []
		for f, m in self.d.md5sums:
			m1 = self.md5sum(f)
			if m1 and m != m1:
				warnings.append(_("File %r was modified since it was written by stepconf") % f)
		if warnings:
			warnings.append("")
			warnings.append(_("Saving this configuration file will discard configuration changes made outside stepconf."))
			if app:
				dialog = Gtk.MessageDialog(app.w.window1,
					Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
					Gtk.MessageType.WARNING, Gtk.ButtonsType.OK,
						 "\n".join(warnings))
				dialog.show_all()
				dialog.run()
				dialog.destroy()
			else:
				for para in warnings:
					for line in textwrap.wrap(para, 78): print line
					print
				print
				if force: return
				response = raw_input(_("Continue? "))
				if response[0] not in _("yY"): raise SystemExit, 1
	
		for p in (10,11,12,13,15):
			pin = "pin%d" % p
			p = self.d[pin]
		for p in (1,2,3,4,5,6,7,8,9,14,16,17):
			pin = "pin%d" % p
			p = self.d[pin]

	####################################################################
	####################################################################
	#************
	# BASIC PAGE
	#************
	def base_prepare(self):
		self.w.drivetime_expander.set_expanded(True)
		self.w.machinename.set_text(self.d.machinename)
		# AXES
		#self.w.axes.set_active(self.d.axes)
		liststore = self.w.axes.get_model ()
		treeiter = liststore.get_iter_first()
		while treeiter != None:
			name, row_id = liststore[treeiter][:2]
			if(row_id == self.d.axes):
				self.w.axes.set_active_iter(treeiter)
				break
			treeiter = liststore.iter_next(treeiter)
		
		self.w.units.set_active(self.d.units)
		self.w.latency.set_value(self.d.latency)
		self.w.steptime.set_value(self.d.steptime)
		self.w.stepspace.set_value(self.d.stepspace)
		self.w.dirsetup.set_value(self.d.dirsetup)
		self.w.dirhold.set_value(self.d.dirhold)
	
		# Preset
		preset_index = self.d.global_preset
		self.select_combo_machine(self.w.base_preset_combo, preset_index)
		preset_index = self.d.drivertype
		self.select_combo_machine(self.w.drivertype, preset_index)
		
		self.w.ioaddr.set_text(self.d.ioaddr)
		self.w.machinename.grab_focus()
		self.w.ioaddr2.set_text(self.d.ioaddr2) 
		#self.w.ioaddr3.set_text(self.d.ioaddr3)
		#self.w.pp3_direction.set_active(self.d.pp3_direction)
		if self.d.number_pports>2:
			 self.w.radio_pp3.set_active(True)
		elif self.d.number_pports>1:
			 self.w.radio_pp2.set_active(True)
		else:
			 self.w.radio_pp1.set_active(True)
	
		ctx = self.w.base_preselect_button.get_style_context()
		ctx.remove_class('selected')
		ctx.add_class('normal')
	
	def base_finish(self):
		self.w.drivetime_expander.set_expanded(False)
		machinename = self.w.machinename.get_text()
		self.d.machinename = machinename.replace(" ","_")
		tree_iter = self.w.axes.get_active_iter()
		if tree_iter != None:
			model = self.w.axes.get_model()
			name, row_id = model[tree_iter][:2]
			self.d.axes = row_id
		else:
			self.d.axes = 0
	
		#self.d.axes = self.w.axes.get_active()
		self.d.units = self.w.units.get_active()
		self.d.steptime = self.w.steptime.get_value()
		self.d.stepspace = self.w.stepspace.get_value()
		self.d.dirsetup = self.w.dirsetup.get_value()
		self.d.dirhold = self.w.dirhold.get_value()
		self.d.latency = self.w.latency.get_value()
		if self.w.radio_pp3.get_active() and self.w.radio_pp2.get_active():
			self.d.number_pports = 3
		elif self.w.radio_pp2.get_active():
			self.d.number_pports = 2
		else:
			self.d.number_pports = 1
	
		# Preset
		current_machine = self.get_machine_preset(self.w.drivertype)
		if current_machine:
			self.d.drivertype = current_machine["index"]
		else:
			# Other selected
			self.d.drivertype = 0
		current_machine = self.get_machine_preset(self.w.base_preset_combo)
		if current_machine:
			self.d.global_preset = current_machine["index"]
		else:
			# Other selected
			self.d.global_preset = 0
		
		self.page_set_state('pport2',self.w.radio_pp2.get_active())
		# Get item selected in combobox
		tree_iter = self.w.axes.get_active_iter()
		model = self.w.axes.get_model()
		text_selected = model[tree_iter][0]
		self.dbg("active axes: %s = %d"% (text_selected,self.d.axes))
		self.page_set_state('axisz','Z' in text_selected)
		self.page_set_state('axisy','Y' in text_selected)
		self.page_set_state('axisu','U' in text_selected)
		self.page_set_state('axisv','V' in text_selected)
		self.page_set_state('axisa','A' in text_selected)
	
	def on_drivertype_changed(self, widget):
		# List axis page widgets
		lwidget=[
			"steptime",
			"stepspace",
			"dirhold",
			"dirsetup"
		]
	
		current_machine = self.get_machine_preset(self.w.drivertype)
		if current_machine:
			#self.d.drivertype = current_machine["index"]
			None
		else:
			# Other selected
			for w in lwidget:
				self.w['%s'%w].set_sensitive(1)
			return
	
		for w in lwidget:
			if(w in current_machine):
				self.w['%s'%w].set_text(str(current_machine[w]))
				self.w['%s'%w].set_sensitive(0)
		self.calculate_ideal_period()
	
	def calculate_ideal_period(self):
		steptime = self.w.steptime.get_value()
		stepspace = self.w.stepspace.get_value()
		latency = self.w.latency.get_value()
		minperiod = self.minperiod(steptime, stepspace, latency)
		maxhz = int(1e9 / minperiod)
		if not self.doublestep(steptime): maxhz /= 2
		self.w.baseperiod.set_text("%d ns" % minperiod)
		self.w.maxsteprate.set_text("%d Hz" % maxhz)
	
	#**************
	# Latency test
	#**************
	def run_latency_test(self):
		self.latency_pid = os.spawnvp(os.P_NOWAIT, "latency-test", ["latency-test"])
		self.w['window1'].set_sensitive(0)
		GObject.timeout_add(15, self.latency_running_callback)
	
	def latency_running_callback(self):
		pid, status = os.waitpid(self.latency_pid, os.WNOHANG)
		if pid:
			self.w['window1'].set_sensitive(1)
			return False
		return True
	 
	# Basic page callbacks
	def on_pp2_checkbutton_toggled(self, *args): 
		i = self.w.pp2_checkbutton.get_active()   
		self.w.pp2_direction.set_sensitive(i)
		self.w.ioaddr2.set_sensitive(i)
		if i == 0:
			self.w.pp3_checkbutton.set_active(i)
			self.w.ioaddr3.set_sensitive(i)
	
	def on_pp3_checkbutton_toggled(self, *args): 
		i = self.w.pp3_checkbutton.get_active() 
		if self.w.pp2_checkbutton.get_active() ==0:
			i=0
			self.w.pp3_checkbutton.set_active(0)
		self.w.pp3_direction.set_sensitive(i)
		self.w.ioaddr3.set_sensitive(i)
	
	def on_latency_test_clicked(self, widget):
		self.run_latency_test()
	
	def on_calculate_ideal_period(self, widget):
		self.calculate_ideal_period()
	
	def on_units_changed(self, widget):
		if not self.d.units == widget.get_active():
			# change the XYZ axis defaults to metric or imperial
			# This erases any entered data that would make sense to change
			self.a.set_axis_unit_defaults(not widget.get_active())
	
	def on_base_preselect_button_clicked(self, widget):
		current_machine = self.get_machine_preset(self.w.base_preset_combo)
		if current_machine:
			self.base_general_preset(current_machine)
			ctx = self.w.base_preselect_button.get_style_context()
			ctx.remove_class('normal')
			ctx.add_class('selected')
	
	def base_general_preset(self, current_machine):
		# base
		self.select_combo_machine(self.w.drivertype, current_machine["index"])
		# pport1
		self.pport1_prepare()
		self.select_combo_machine(self.w.pp1_preset_combo, current_machine["index"])
		self.on_pp1_preselect_button_clicked(None)
		self.pport1_finish()
		# axis
		for axis in ('x','y','z','u','v'):
			self.axis_prepare(axis)
			self.select_combo_machine(self.w[axis + "preset_combo"], current_machine["index"])
			self.preset_axis(axis)
			self.axis_done(axis)
		# spindle
		self.spindle_prepare()
		self.select_combo_machine(self.w.spindle_preset_combo, current_machine["index"])
		self.on_spindle_preset_button_clicked(None)
		self.spindle_finish()
		# options: preset probe coordinates  in options page
		self.option_preset()
		return
		
	####################################################################
	####################################################################
	#************
	# pport1 PAGE
	#************
	def pport1_prepare(self):
		self._p.in_pport_prepare = True
		# OUTPUT
		for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
			p = 'pin%d' % pin
			self.w[p].set_wrap_width(3)
			# Search element where pin == pin
			lcurrent_function = filter(lambda element: element['name'] == self.d[p], hal_output)
			if(lcurrent_function != []):
				# Just first element 
				current_function = lcurrent_function[0]
				self.w[p].set_active(current_function["index"])
				p = 'pin%dinv' % pin
				self.w[p].set_active(self.d[p])
		# INPUT
		for pin in (10,11,12,13,15):
			p = 'pin%d' % pin
			self.w[p].set_wrap_width(3)
			# Search element where pin == pin
			lcurrent_function = filter(lambda element: element['name'] == self.d[p], hal_input)
			if(lcurrent_function != []):
				# Just first element
				current_function = lcurrent_function[0]
				self.w[p].set_active(current_function["index"])
				p = 'pin%dinv' % pin
				self.w[p].set_active(self.d[p])
	
		# Debounce
		self.w.chk_debounce_home_inputs.set_active(self.d.debounce_home_inputs)
		self.w.chk_debounce_limit_inputs.set_active(self.d.debounce_limit_inputs)
	
		self.w.pin1.grab_focus()
		self.w.ioaddr.set_text(self.d.ioaddr)
	
		# Preset
		preset_index = self.d.pport1_preset
		self.select_combo_machine(self.w.pp1_preset_combo, preset_index)
		self.pport1_execute_preset()
		
		self._p.in_pport_prepare = False
	
	
	def pport1_finish(self):
		for pin in (10,11,12,13,15):
			p = 'pin%d' % pin
			self.d[p] = hal_input[self.w[p].get_active()]["name"]
		for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
			p = 'pin%d' % pin
			self.d[p] = hal_output[self.w[p].get_active()]["name"]
		for pin in (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17):
			p = 'pin%dinv' % pin
			self.d[p] = self.w[p].get_active()
		self.d.ioaddr = self.w.ioaddr.get_text()
	
		# Debounce
		self.d.debounce_home_inputs = self.w.chk_debounce_home_inputs.get_active()
		self.d.debounce_limit_inputs = self.w.chk_debounce_limit_inputs.get_active()
	
		# Save preset
		current_machine = self.get_machine_preset(self.w.pp1_preset_combo)
		if current_machine:
			self.d.pport1_preset = current_machine["index"]
		else:
			# Other selected
			self.d.pport1_preset = 0
	
		self.check_spindle_speed_control()
		self.check_spindle_encoder()
		self.page_set_state('spindle',(self._p.has_spindle_speed_control or self._p.has_spindle_encoder) )
	
	# pport1 callbacks
	def on_exclusive_check_pp1(self, widget):
		self.do_exclusive_inputs(widget,1)
	
	def on_pp1_preselect_button_clicked(self, widget):
		self.pport1_execute_preset()
	
	def pport1_execute_preset(self):
		current_machine = self.get_machine_preset(self.w.pp1_preset_combo)
		if current_machine:
			None
		else:
			# Other selected
			# Enable pin selection
			for pin in (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17):
				p = 'pin%d' % pin
				self.w['%s'%p].set_sensitive(1)
				p = 'pin%dinv' % pin
				self.w['%s'%p].set_sensitive(1)
			return
	
		# Test output & intput & inverted
		for pin in (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17):
			p = 'pin%d' % pin
			if(p in current_machine):
				self.w['%s'%p].set_active(current_machine[p])
				self.w['%s'%p].set_sensitive(0)
			p = 'pin%dinv' % pin
			if(p in current_machine):
				self.w['%s'%p].set_active(current_machine[p])
				self.w['%s'%p].set_sensitive(0)
	
	
	def on_pp1_preset_io_combo_changed(self, widget):
		state = self.w.pp1_preset_io_combo.get_active()
		if(state > -1):
			path = Gtk.TreePath(state)
			treeiter = self.w.pp1_preset_io_liststore.get_iter(path)
			value = self.w.pp1_preset_io_liststore.get_value(treeiter, 0)
			self.w.ioaddr.set_text(value)
		else:
			return

	####################################################################
	####################################################################
	#************
	# pport2 PAGE
	#************
	def pport2_prepare(self):
		self._p.in_pport_prepare = True
		# OUTPUT
		for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
			p = 'pp2_pin%d' % pin
			self.w[p].set_wrap_width(3)
			# Search element where pin == pin
			lcurrent_function = filter(lambda element: element['name'] == self.d[p], hal_output)
			if(lcurrent_function != []):
				# Just first element 
				current_function = lcurrent_function[0]
	            # Missing first 8 elements (check initialize function on pages.py)
				self.w[p].set_active(current_function["index"]-8)
				p = 'pp2_pin%dinv' % pin
				self.w[p].set_active(self.d[p])
		# INPUT
		for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
			p = 'pp2_pin%d_in' % pin
			self.w[p].set_wrap_width(3)
			lcurrent_function = filter(lambda element: element['name'] == self.d[p], hal_input)
			if(lcurrent_function != []):
				# Just first element
				current_function = lcurrent_function[0]
				self.w[p].set_active(current_function["index"])
				p = 'pp2_pin%d_in_inv' % pin
				self.w[p].set_active(self.d[p])
	
		self.w.pp2_pin1.grab_focus()
		self.w.pp2_direction.set_active(self.d.pp2_direction)
		self.on_pp2_direction_changed(self.w.pp2_direction)
		self.w.ioaddr2.set_text(self.d.ioaddr2)
		self._p.in_pport_prepare = False
	
	def pport2_finish(self):
		for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
			p = 'pp2_pin%d' % pin
			self.d[p] = hal_output[self.w[p].get_active()+8]["name"]
			p = 'pp2_pin%dinv' % pin
			self.d[p] = self.w[p].get_active()
		for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
			p = 'pp2_pin%d_in' % pin
			self.d[p] = hal_input[self.w[p].get_active()]["name"]
			p = 'pp2_pin%d_in_inv' % pin
			self.d[p] = self.w[p].get_active()
		self.d.pp2_direction = self.w.pp2_direction.get_active()
		self.d.ioaddr2 = self.w.ioaddr2.get_text()
	
		"""
		# Save preset
		current_machine = self.get_machine_preset(self.w.pp2_preset_combo)
		if current_machine:
			self.d.pport2_preset = current_machine["index"]
		else:
			# Other selected
			self.d.pport2_preset = 0
			return
		"""
		self.check_spindle_speed_control()
		self.check_spindle_encoder()
		self.page_set_state('spindle',(self._p.has_spindle_speed_control or self._p.has_spindle_encoder) )
	
	# pport2 callbacks:
	def on_pp2_direction_changed(self, widget):
		state = widget.get_active()
		for i in (2,3,4,5,6,7,8,9):
			self.w['pp2_pin%s_in_box'%i].set_visible(state)
			self.w['pp2_pin%s_out_box'%i].set_visible(not state)
	
	def on_exclusive_check_pp2(self, widget):
		self.do_exclusive_inputs(widget,2)
	####################################################################
	####################################################################
	####### AXIS #######
	def axis_prepare(self, axis):
		def set_text(n):
			self.w[axis + n].set_text("%s" % self.d[axis + n])
			# Set a name for this widget. Necessary for css id
			self.w[axis + n].set_name("%s%s" % (axis, n))
		def set_active(n):
			self.w[axis + n].set_active(self.d[axis + n])
	
		set_text("steprev")
		set_text("microstep")
		set_text("pulleynum")
		set_text("pulleyden")
		set_text("leadscrew")
		set_text("maxvel")
		set_text("maxacc")
		set_text("homepos")
		set_text("minlim")
		set_text("maxlim")
		set_text("homesw")
		set_text("homevel")
		set_active("latchdir")
	
		if axis == "a":
			self.w[axis + "screwunits"].set_text(_("degree / rev"))
			self.w[axis + "velunits"].set_text(_("deg / s"))
			self.w[axis + "accunits"].set_text(_(u"deg / s²"))
			self.w[axis + "accdistunits"].set_text(_("deg"))
			self.w[axis + "scaleunits"].set_text(_("Steps / deg"))
		elif self.d.units:
			self.w[axis + "screwunits"].set_text(_("mm / rev"))
			self.w[axis + "velunits"].set_text(_("mm / s"))
			self.w[axis + "accunits"].set_text(_(u"mm / s²"))
			self.w[axis + "accdistunits"].set_text(_("mm"))
			self.w[axis + "scaleunits"].set_text(_("Steps / mm"))
		else:
			self.w[axis + "screwunits"].set_text(_("rev / in"))
			self.w[axis + "velunits"].set_text(_("in / s"))
			self.w[axis + "accunits"].set_text(_(u"in / s²"))
			self.w[axis + "accdistunits"].set_text(_("in"))
			self.w[axis + "scaleunits"].set_text(_("Steps / in"))
	
		#self.check_switch_limits(axis)
	
		self.w[axis + "steprev"].grab_focus()
		GObject.idle_add(lambda: self.update_pps(axis))
		
		# Prepare preset
		preset_index = self.d[axis + "preset"]
		self.select_combo_machine(self.w[axis + "preset_combo"], preset_index)
		self.preset_axis(axis)
	
	def axis_done(self, axis):
		def get_text(n):
			self.d[axis + n] = float(self.w[axis + n].get_text())
		def get_active(n):
			self.d[axis + n] = self.w[axis + n].get_active()
		get_text("steprev")
		get_text("microstep")
		get_text("pulleynum") 
		get_text("pulleyden")
		get_text("leadscrew")
		get_text("maxvel")
		get_text("maxacc")
		get_text("homepos")
		get_text("minlim")
		get_text("maxlim")
		get_text("homesw")
		get_text("homevel")
		get_active("latchdir")
		# Save preset
		current_machine = self.get_machine_preset(self.w[axis + "preset_combo"])
		if current_machine:
			self.d[axis + "preset"] = current_machine["index"]
		else:
			# Other selected
			self.d[axis + "preset"] = 0
	
	def check_switch_limits(self, axis):
		inputs = self.build_input_set()
		#thisaxishome = set([ALL_HOME, ALL_LIMIT_HOME, "home-" + axis, "min-home-" + axis,
		#					"max-home-" + axis, "both-home-" + axis])
		thisaxishome = set([d_hal_input[ALL_HOME], d_hal_input[ALL_LIMIT_HOME], "home-" + axis, "min-home-" + axis,
							"max-home-" + axis, "both-home-" + axis])
		# Test if exists limit switches
		homes = bool(inputs & thisaxishome)
		if (homes == False):
			self.w[axis + "homesw"].set_sensitive(homes)
			self.w[axis + "homevel"].set_sensitive(homes)
			self.w[axis + "latchdir"].set_sensitive(homes)
		else:
			current_machine = self.get_machine_preset(self.w[axis + "preset_combo"])
			if current_machine:
				if (axis + "homesw" in current_machine):
					self.w[axis + "homesw"].set_sensitive(0)
				if (axis + "homevel" in current_machine):
					self.w[axis + "homevel"].set_sensitive(0)
				if (axis + "latchdir" in current_machine):
					self.w[axis + "latchdir"].set_sensitive(0)
			else:
				# Other selected
				self.w[axis + "homesw"].set_sensitive(homes)
				self.w[axis + "homevel"].set_sensitive(homes)
				self.w[axis + "latchdir"].set_sensitive(homes)
				return
	
	def preset_axis(self, axis):
		# List axis page widgets (except latchdir)
		lwidget=[
			axis + "steprev",
			axis + "microstep",
			axis + "pulleynum",
			axis + "pulleyden",
			axis + "leadscrew",
			axis + "maxvel",
			axis + "maxacc",
			axis + "homepos",
			axis + "minlim",
			axis + "maxlim",
			axis + "homesw",
			axis + "homevel"
		]
	
		current_machine = self.get_machine_preset(self.w[axis + "preset_combo"])
		if current_machine:
			None
		else:
			# Other selected
			for w in lwidget:
				self.w['%s'%w].set_sensitive(1)
			self.check_switch_limits(axis)
			return
	
		for w in lwidget:
			if(w in current_machine):
				self.w['%s'%w].set_text(str(current_machine[w]))
				self.w['%s'%w].set_sensitive(0)
			else:
				self.w['%s'%w].set_sensitive(1)
		if(axis + "latchdir" in current_machine):
			self.w['%slatchdir'%axis].set_active(current_machine[axis + "latchdir"])
			#self.w['%slatchdir'%axis].set_sensitive(0)
		#else:
		#	self.w['%slatchdir'%axis].set_sensitive(1)
		# 
		self.check_switch_limits(axis)
	
	
	# for Axis page calculation updates
	def update_pps(self, axis):
		def get(n):
			return float(self.w[axis + n].get_text())
		self.axis_sanity_test(axis)
		try:
			pitch = get("leadscrew")
			step = get("steprev")
			micro = get("microstep")
			pullnum = get("pulleynum")
			pulldem = get("pulleyden")
			if self.d.units == MM or axis == 'a':
				pitch = 1./pitch
			pps = (pitch * step * micro * (pullnum / pulldem) * get("maxvel"))
			if pps == 0:
				raise ValueError
			pps = abs(pps)
			acctime = get("maxvel") / get("maxacc")
			accdist = acctime * .5 * get("maxvel")
			self.w[axis + "acctime"].set_text("%.4f" % acctime)
			self.w[axis + "accdist"].set_text("%.4f" % accdist)
			self.w[axis + "hz"].set_text("%.1f" % pps)
			scale = self.d[axis + "scale"] = (1.0 * pitch * step * micro * (pullnum / pulldem))
			self.w[axis + "scale"].set_text("%.1f" % scale)
			temp = "Axis Scale: %d × %d × (%.1f ÷ %.1f) × %.3f ="% (int(step),int(micro),(pullnum),(pulldem),pitch)
			self.w[axis + "scaledescr"].set_text(temp)
			self.set_buttons_sensitive(1,1)
			self.w[axis + "axistest"].set_sensitive(1)
		except (ValueError, ZeroDivisionError): # Some entries not numbers or not valid
			self.w[axis + "acctime"].set_text("")
			self.w[axis + "accdist"].set_text("")
			self.w[axis + "hz"].set_text("")
			self.w[axis + "scale"].set_text("")
			self.set_buttons_sensitive(0,0)
			self.w[axis + "axistest"].set_sensitive(0)
	
	def axis_sanity_test(self, axis):
		# I hate the inner function
		def get(n):
			return float(self.w[axis + n].get_text())
	
		# List of field with background color can change
		datalist = ('steprev','microstep','pulleynum','pulleyden','leadscrew',
					'maxvel','maxacc')
		mystyle =""
		for i in datalist:
			# Damn! this is a bug. GTKBuilder sets the widget name to be the builder ID.
			#widget_name = Gtk.Buildable.get_name(self.w[axis+i])
			ctx = self.w[axis+i].get_style_context()
			try:
				a=get(i)
				if a <= 0:
					raise ValueError
			except:
				ctx.add_class('invalid')
			else:
				ctx.remove_class('invalid')

	def on_update_axis_params(self, *args):
		update_axis_params(*args)

	def on_jogminus_pressed(self, *args):
		self._p.jogminus = 1
		self.update_axis_test()
	
	def on_jogminus_released(self, *args):
		self._p.jogminus = 0
		self.update_axis_test()
	
	def on_jogplus_pressed(self, *args):
		self._p.jogplus = 1
		self.update_axis_test()
	
	def on_jogplus_released(self, *args):
		self._p.jogplus = 0
		self.update_axis_test()
	####################################################################
	####################################################################
	#*******************
	# AXIS X PAGE
	#*******************
	def axisx_prepare(self):
		self.axis_prepare('x')
	def axisx_finish(self):
		self.axis_done('x')
	# AXIS X callbacks
	def on_xsteprev_changed(self, *args): self.update_pps('x')
	def on_xmicrostep_changed(self, *args): self.update_pps('x')
	def on_xpulleyden_changed(self, *args): self.update_pps('x')
	def on_xpulleynum_changed(self, *args): self.update_pps('x')
	def on_xleadscrew_changed(self, *args): self.update_pps('x')
	def on_xmaxvel_changed(self, *args): self.update_pps('x')
	def on_xmaxacc_changed(self, *args): self.update_pps('x')
	def on_xaxistest_clicked(self, *args): self.test_axis('x')
	def on_xpreset_button_clicked(self, *args): self.preset_axis('x')

	####################################################################
	####################################################################
	#********************
	# AXIS Y
	#********************
	def axisy_prepare(self):
		self.axis_prepare('y')
	def axisy_finish(self):
		self.axis_done('y')
	# AXIS Y callbacks
	def on_ysteprev_changed(self, *args): self.update_pps('y')
	def on_ymicrostep_changed(self, *args): self.update_pps('y')
	def on_ypulleyden_changed(self, *args): self.update_pps('y')
	def on_ypulleynum_changed(self, *args): self.update_pps('y')
	def on_yleadscrew_changed(self, *args): self.update_pps('y')
	def on_ymaxvel_changed(self, *args): self.update_pps('y')
	def on_ymaxacc_changed(self, *args): self.update_pps('y')
	def on_yaxistest_clicked(self, *args): self.test_axis('y')
	def on_ypreset_button_clicked(self, *args): self.preset_axis('y')

	####################################################################
	####################################################################
	#********************
	# AXIS Z PAGE
	#********************
	def axisz_prepare(self):
		self.axis_prepare('z')
	def axisz_finish(self):
		self.axis_done('z')
	# AXIS Z callbacks
	def on_zsteprev_changed(self, *args): self.update_pps('z')
	def on_zmicrostep_changed(self, *args): self.update_pps('z')
	def on_zpulleyden_changed(self, *args): self.update_pps('z')
	def on_zpulleynum_changed(self, *args): self.update_pps('z')
	def on_zleadscrew_changed(self, *args): self.update_pps('z')
	def on_zmaxvel_changed(self, *args): self.update_pps('z')
	def on_zmaxacc_changed(self, *args): self.update_pps('z')
	def on_zaxistest_clicked(self, *args): self.test_axis('z')
	def on_zpreset_button_clicked(self, *args): self.preset_axis('z')

	####################################################################
	####################################################################
	#********************
	# AXIS A PAGE
	#********************
	def axisa_prepare(self):
		self.axis_prepare('a')
	def axisa_finish(self):
		self.axis_done('a')
	# AXIS A callbacks
	def on_asteprev_changed(self, *args): self.update_pps('a')
	def on_amicrostep_changed(self, *args): self.update_pps('a')
	def on_apulleyden_changed(self, *args): self.update_pps('a')
	def on_apulleynum_changed(self, *args): self.update_pps('a')
	def on_aleadscrew_changed(self, *args): self.update_pps('a')
	def on_amaxvel_changed(self, *args): self.update_pps('a')
	def on_amaxacc_changed(self, *args): self.update_pps('a')
	def on_aaxistest_clicked(self, *args): self.test_axis('a')
	def on_apreset_button_clicked(self, *args): self.preset_axis('a')

	####################################################################
	####################################################################
	#********************
	# AXIS U PAGE
	#********************
	def axisu_prepare(self):
		self.axis_prepare('u')
	def axisu_finish(self):
		self.axis_done('u')
	# AXIS U callbacks
	def on_usteprev_changed(self, *args): self.update_pps('u')
	def on_umicrostep_changed(self, *args): self.update_pps('u')
	def on_upulleyden_changed(self, *args): self.update_pps('u')
	def on_upulleynum_changed(self, *args): self.update_pps('u')
	def on_uleadscrew_changed(self, *args): self.update_pps('u')
	def on_umaxvel_changed(self, *args): self.update_pps('u')
	def on_umaxacc_changed(self, *args): self.update_pps('u')
	def on_uaxistest_clicked(self, *args): self.test_axis('u')
	def on_upreset_button_clicked(self, *args): self.preset_axis('u')

	####################################################################
	####################################################################
	#********************
	# AXIS V PAGE
	#********************
	def axisv_prepare(self):
		self.axis_prepare('v')
	def axisv_finish(self):
		self.axis_done('v')
	# AXIS V callbacks
	def on_vsteprev_changed(self, *args): self.update_pps('v')
	def on_vmicrostep_changed(self, *args): self.update_pps('v')
	def on_vpulleyden_changed(self, *args): self.update_pps('v')
	def on_vpulleynum_changed(self, *args): self.update_pps('v')
	def on_vleadscrew_changed(self, *args): self.update_pps('v')
	def on_vmaxvel_changed(self, *args): self.update_pps('v')
	def on_vmaxacc_changed(self, *args): self.update_pps('v')
	def on_vaxistest_clicked(self, *args): self.test_axis('v')
	def on_vpreset_button_clicked(self, *args): self.preset_axis('v')

	####################################################################
	####################################################################
	#*******************
	# SPINDLE PAGE
	#*******************
	def spindle_prepare(self):
		self.w['spindlecarrier'].set_text("%s" % self.d.spindlecarrier)
		self.w['spindlespeed1'].set_text("%s" % self.d.spindlespeed1)
		self.w['spindlespeed2'].set_text("%s" % self.d.spindlespeed2)
		self.w['spindlepwm1'].set_text("%s" % self.d.spindlepwm1)
		self.w['spindlepwm2'].set_text("%s" % self.d.spindlepwm2)
		self.w['spindlecpr'].set_text("%s" % self.d.spindlecpr)
		self.w['spindlenearscale'].set_value(self.d.spindlenearscale * 100)
		self.w['spindlefiltergain'].set_value(self.d.spindlefiltergain)
		self.w['usespindleatspeed'].set_active(self.d.usespindleatspeed)
	
		if self._p.has_spindle_encoder:
			self.w.spindlecpr.show()
			self.w.spindlecprlabel.show()
			self.w.spindlefiltergain.show()
			self.w.spindlefiltergainlabel.show()
			self.w.spindlenearscale.show()
			self.w.usespindleatspeed.show()
			self.w.spindlenearscaleunitlabel.show()
		else:
			self.w.spindlecpr.hide()
			self.w.spindlecprlabel.hide()
			self.w.spindlefiltergain.hide()
			self.w.spindlefiltergainlabel.hide()
			self.w.spindlenearscale.hide()
			self.w.usespindleatspeed.hide()
			self.w.spindlenearscaleunitlabel.hide()
	
		self.w.output.set_sensitive(self._p.has_spindle_speed_control)
		# Preset
		preset_index = self.d.spindle_preset
		self.select_combo_machine(self.w.spindle_preset_combo, preset_index)
		self.spindle_execute_preset()
	
	def spindle_finish(self):
		self.d.spindlecarrier = float(self.w.spindlecarrier.get_text())
		self.d.spindlespeed1 = float(self.w.spindlespeed1.get_text())
		self.d.spindlespeed2 = float(self.w.spindlespeed2.get_text())
		self.d.spindlepwm1 = float(self.w.spindlepwm1.get_text())
		self.d.spindlepwm2 = float(self.w.spindlepwm2.get_text())
		self.d.spindlecpr = float(self.w.spindlecpr.get_text())
		self.d.spindlenearscale = self.w.spindlenearscale.get_value()/100
		self.d.spindlefiltergain = self.w.spindlefiltergain.get_value()
		self.d.usespindleatspeed = self.w['usespindleatspeed'].get_active()
		# Save preset
		current_machine = self.get_machine_preset(self.w.spindle_preset_combo)
		if current_machine:
			self.d.spindle_preset = current_machine["index"]
		else:
			# Other selected
			self.d.spindle_preset = 0
	
	# Spindle page callbacks
	def on_usespindleatspeed_toggled(self,*args):
		self.w.spindlenearscale.set_sensitive(self.w.usespindleatspeed.get_active())
	
	def on_spindle_preset_button_clicked(self, widget):
		self.spindle_execute_preset()
	
	def spindle_execute_preset(self):
		# List spindle page widgets
		lwidget=[
			'usespindleatspeed',
			'spindlecarrier',
			'spindlecpr',
			'spindlefiltergain',
			'spindlenearscale',
			'spindlepwm1',
			'spindlepwm2',
			'spindlespeed1',
			'spindlespeed2'
		]
	
		current_machine = self.get_machine_preset(self.w.spindle_preset_combo)
		if current_machine:
			None
		else:
			# Other selected
			for w in lwidget:
				self.w['%s'%w].set_sensitive(1)
			return
	
		for w in lwidget:
			if(w in current_machine):
				self.w['%s'%w].set_text(str(current_machine[w]))
				self.w['%s'%w].set_sensitive(0)
			else:
				self.w['%s'%w].set_sensitive(1)

	####################################################################
	####################################################################
	#***************
	# options PAGE
	#***************
	def options_prepare(self):
		self.w.classicladder.set_active(self.d.classicladder)
		self.w.modbus.set_active(self.d.modbus)
		self.w.digitsin.set_value(self.d.digitsin)
		self.w.digitsout.set_value(self.d.digitsout)
		self.w.s32in.set_value(self.d.s32in)
		self.w.s32out.set_value(self.d.s32out)
		self.w.floatsin.set_value(self.d.floatsin)
		self.w.floatsout.set_value(self.d.floatsout)
		self.w.halui.set_active(self.d.halui_custom)
		self.page_set_state('halui_page', self.w.halui.get_active())
		self.w.ladderconnect.set_active(self.d.ladderconnect)
		self.on_classicladder_toggled()
		if (self.d.tool_change_type == TOOL_CHANGE_MANUAL):
			self.w.manualtoolchange.set_active(True)
		else:
			self.w.manualtoolchange.set_active(False)
		if  not self.w.createconfig.get_active():
		   if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custom.clp" % self.d.machinename)):
				self.w.radiobutton4.set_active(True)
		# Set default probe value
		self.w.probe_x_pos.set_text("%.1f" % self.d.probe_x_pos)
		self.w.probe_y_pos.set_text("%.1f" % self.d.probe_y_pos)
		self.w.probe_z_pos.set_text("%.1f" % self.d.probe_z_pos)
		self.w.probe_sensor_height.set_text("%.1f" % self.d.probe_sensor_height)
	
		# Check for tool lenght sensor
		inputs = self.build_input_set()
		if (d_hal_input[PROBE] in inputs):
			self.w.option_probe_expander.set_expanded(1)
			self.w.probe_x_pos.set_sensitive(1)
			self.w.probe_y_pos.set_sensitive(1)
			self.w.probe_z_pos.set_sensitive(1)
			self.w.probe_sensor_height.set_sensitive(1)
		else:
			self.w.option_probe_expander.set_expanded(0)
			self.w.probe_x_pos.set_sensitive(0)
			self.w.probe_y_pos.set_sensitive(0)
			self.w.probe_z_pos.set_sensitive(0)
			self.w.probe_sensor_height.set_sensitive(0)
	
	def options_finish(self):
		SIG = self._p
		self.d.classicladder = self.w.classicladder.get_active()
		self.d.modbus = self.w.modbus.get_active()
		self.d.digitsin = self.w.digitsin.get_value()
		self.d.digitsout = self.w.digitsout.get_value()
		self.d.s32in = self.w.s32in.get_value()
		self.d.s32out = self.w.s32out.get_value()
		self.d.floatsin = self.w.floatsin.get_value()
		self.d.floatsout = self.w.floatsout.get_value()
		self.d.halui_custom = self.w.halui.get_active()  
		self.d.ladderconnect = self.w.ladderconnect.get_active()
		if(self.w.manualtoolchange.get_active() == True):
			self.d.tool_change_type = TOOL_CHANGE_MANUAL
		self.d.probe_x_pos = float(self.w.probe_x_pos.get_text())
		self.d.probe_y_pos = float(self.w.probe_y_pos.get_text())
		self.d.probe_z_pos = float(self.w.probe_z_pos.get_text())
		self.d.probe_sensor_height = float(self.w.probe_sensor_height.get_text())
	
		if self.d.classicladder:
		   if self.w.radiobutton1.get_active() == True:
			  if self.d.tempexists:
				   self.d.laddername='TEMP.clp'
			  else:
				   self.d.laddername= 'blank.clp'
				   self.d.ladderhaltype = 0
		   if self.w.radiobutton2.get_active() == True:
			  self.d.laddername = 'estop.clp'
			  inputs = self.build_input_set()
			  if SIG.ESTOP_IN not in inputs:
				 self.warning_dialog(MESS_NO_ESTOP,True)
				 return True # don't advance the page
			  self.d.ladderhaltype = 1
		   if self.w.radiobutton3.get_active() == True:
				 self.d.laddername = 'serialmodbus.clp'
				 self.d.modbus = 1
				 self.w.modbus.set_active(self.d.modbus) 
				 self.d.ladderhaltype = 0          
		   if self.w.radiobutton4.get_active() == True:
			  self.d.laddername='custom.clp'
		   else:
			   if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custom.clp" % self.d.machinename)):
				  if not self.warning_dialog(MESS_CL_REWRITE,False):
					 return True # don't advance the page
		   if self.w.radiobutton1.get_active() == False:
			  if os.path.exists(os.path.join(self._p.distdir, "configurable_options/ladder/TEMP.clp")):
				 if not self.warning_dialog(MESS_CL_EDITTED,False):
				   return True # don't advance the page
	
	def option_preset(self):
		# Use machine selected on base 
		current_machine = self.get_machine_preset(self.w.base_preset_combo)
		if current_machine:
			None
		else:
			return
			
		lwidget=[
			"probe_x_pos",
			"probe_y_pos",
			"probe_z_pos",
			"probe_sensor_height",
		]
		
		for w in lwidget:
			if(w in current_machine):
				self.w['%s'%w].set_text(str(current_machine[w]))
		# Save theese parameters early
		self.d.probe_x_pos = float(self.w.probe_x_pos.get_text())
		self.d.probe_y_pos = float(self.w.probe_y_pos.get_text())
		self.d.probe_z_pos = float(self.w.probe_z_pos.get_text())
		self.d.probe_sensor_height = float(self.w.probe_sensor_height.get_text())
	
	def on_probe_x_pos_changed(self, *args):
		self.options_sanity_test()
	def on_probe_y_pos_changed(self, *args):
		self.options_sanity_test()
	def on_probe_z_pos_changed(self, *args):
		self.options_sanity_test()
	def on_probe_sensor_height_changed(self, *args):
		self.options_sanity_test()
	
	def options_sanity_test(self):
		def get(n):
			return float(n.get_text())
	
		datalist = (self.w.probe_x_pos, self.w.probe_y_pos, self.w.probe_z_pos, self.w.probe_sensor_height)
	
		for i in datalist:
			ctx = i.get_style_context()
			try:
				a=get(i)
				if a <= 0:
					#raise ValueError
					ctx.add_class('invalid')
			except:
				ctx.add_class('invalid')
			else:
				ctx.remove_class('invalid')
	
				
	# options page callback
	def on_loadladder_clicked(self, *args):
		self.load_ladder(self)
	
	def on_classicladder_toggled(self, *args):
		i= self.w.classicladder.get_active()
		self.w.ladder_box.set_sensitive(i)
		if  self.w.createconfig.get_active():
			self.w.radiobutton4.set_sensitive(False)
		else:
			self.w.radiobutton4.set_sensitive(i)
		if not i:
			self.w.clpins_expander.set_expanded(False)
	
	def on_halui_toggled(self, *args):
		self.page_set_state('halui_page', self.w.halui.get_active())
	
	
	#**************
	# LADDER TEST
	#**************
	def load_ladder(self,w):         
		newfilename = os.path.join(self._p.distdir, "configurable_options/ladder/TEMP.clp")    
		self.d.modbus = self.w.modbus.get_active()
		self.halrun = halrun = os.popen("halrun -Is", "w")
		halrun.write(""" 
			  loadrt threads period1=%(period)d name1=fast fp1=0 period2=1000000 name2=slow\n
			  loadrt classicladder_rt numPhysInputs=%(din)d numPhysOutputs=%(dout)d numS32in=%(sin)d numS32out=%(sout)d\
					 numFloatIn=%(fin)d numFloatOut=%(fout)d\n
			  addf classicladder.0.refresh slow\n
			  start\n
					  """ % {
					  'period': 50000,
					  'din': self.w.digitsin.get_value(),
					  'dout': self.w.digitsout.get_value(),
					  'sin': self.w.s32in.get_value(),
					  'sout': self.w.s32out.get_value(), 
					  'fin':self.w.floatsin.get_value(),
					  'fout':self.w.floatsout.get_value(),
				 })
		if self.w.radiobutton1.get_active() == True:
			if self.d.tempexists:
			   self.d.laddername='TEMP.clp'
			else:
			   self.d.laddername= 'blank.clp'
		if self.w.radiobutton2.get_active() == True:
			self.d.laddername= 'estop.clp'
		if self.w.radiobutton3.get_active() == True:
			self.d.laddername = 'serialmodbus.clp'
			self.d.modbus = True
			self.w.modbus.set_active(self.d.modbus)
		if self.w.radiobutton4.get_active() == True:
			self.d.laddername='custom.clp'
			originalfile = filename = os.path.expanduser("~/linuxcnc/configs/%s/custom.clp" % self.d.machinename)
		else:
			filename = os.path.join(self._p.distdir, "configurable_options/ladder/"+ self.d.laddername)        
		if self.d.modbus == True: 
			halrun.write("loadusr -w classicladder --modmaster --newpath=%(newfilename)s %(filename)s\
				\n" %          { 'newfilename':newfilename ,'filename':filename })
		else:
			halrun.write("loadusr -w classicladder --newpath=%(newfilename)s %(filename)s\n" % { 'newfilename':newfilename ,'filename':filename })
		halrun.flush()
		halrun.close()
		if os.path.exists(newfilename):
			self.d.tempexists = True
			self.w.newladder.set_text('Edited ladder program')
			self.w.radiobutton1.set_active(True)
		else:
			self.d.tempexists = 0

	####################################################################
	####################################################################
	#***************
	# halui PAGE
	#***************
	def halui_page_prepare(self):
		# Clear listore
		self.w.lstStore1.clear()
		# Populate treeview
		for num, mdi_command in enumerate(self.d.halui_list_custom):
			self.w.lstStore1.append([num+1, mdi_command])
	
	def halui_page_finish(self):
		self.d.halui_list_custom = []
		# Get first row
		treeiter = self.w.lstStore1.get_iter_first()
		if treeiter == None:
			return True
		self.d.halui_list_custom.append(self.w.lstStore1.get_value(treeiter, 1))
		while treeiter != None:
			treeiter = self.w.lstStore1.iter_next(treeiter)
			if treeiter != None:
				current_value = self.w.lstStore1.get_value(treeiter, 1).strip()
				# Check if value contains data
				if len(current_value) > 2:
					self.d.halui_list_custom.append(current_value)
	
	def on_halui_btnAdd_clicked(self, *args):
		next_index = len(self.w.lstStore1) +1
		self.w.lstStore1.append([next_index, ""])
	
	def on_halui_btnDel_clicked(self, *args):
		select = self.w.viewTable1.get_selection()
		model, treeiter = select.get_selected()
		if treeiter != None:
			# Remove selected row
			self.w.lstStore1.remove(treeiter)
			# Get first row
			treeiter = self.w.lstStore1.get_iter_first()
			if treeiter == None:
				return
			index = 1
			self.w.lstStore1.set_value(treeiter, 0, index)
			index = index +1
			# Cicle lstStore1 to update index
			while treeiter != None:
				treeiter = self.w.lstStore1.iter_next(treeiter)
				if treeiter != None:
					# Change index
					self.w.lstStore1.set_value(treeiter, 0, index)
					index = index +1
	
	def on_halui_btnUp_clicked(self, *args):
		select = self.w.viewTable1.get_selection()
		model, treeiter = select.get_selected()
		if treeiter != None:
			prev_treeiter = model.iter_previous(treeiter)
			current_index = model[treeiter][0]
			# Move up and update first column (index)
			if((current_index -1) > 0):
				self.w.lstStore1.move_before(treeiter, prev_treeiter)
				self.w.lstStore1.set_value(treeiter, 0, current_index -1)
				self.w.lstStore1.set_value(prev_treeiter, 0, current_index)
	
	def on_halui_btnDown_clicked(self, *args):
		select = self.w.viewTable1.get_selection()
		model, treeiter = select.get_selected()
		if treeiter != None:
			next_treeiter = model.iter_next(treeiter)
			current_index = model[treeiter][0]
			# Move down and update first column (index)
			if(next_treeiter != None):
				self.w.lstStore1.move_after(treeiter, next_treeiter)
				self.w.lstStore1.set_value(treeiter, 0, current_index +1)
				self.w.lstStore1.set_value(next_treeiter, 0, current_index)
	
	def on_halui_row_changed(self, *args):
		newvalue = args[2]
		if (len(newvalue.strip()) <2):
			return
		select = self.w.viewTable1.get_selection()
		model, treeiter = select.get_selected()
		self.w.lstStore1.set_value(treeiter, 1, newvalue)

	####################################################################
	####################################################################
	#************
	# GUI PAGE
	#************
	def gui_page_prepare(self):
		self.w.gui_select_axis.set_active(self.d.select_axis)
		self.w.gui_select_gmoccapy.set_active(self.d.select_gmoccapy)
		self.w.ladderconnect.set_active(self.d.ladderconnect)
	
		if(self.d.guitype == GUI_IS_GLADEVCP):
			self.w.gui_rdo_gladevcp.set_active(True)
			self.on_gui_rdo_gladevcp_toggled(True)
		elif(self.d.guitype == GUI_IS_PYVCP):
			self.w.gui_rdo_pyvcp.set_active(True)
			self.on_gui_rdo_pyvcp_toggled(True)
		elif(self.d.guitype == GUI_IS_GMOCCAPY):
			None
		else:
			# Default none
			self.w.gui_rdo_nogui.set_active(True)
	
		# Gladevcp
		#self.w.gui_gladevcp.set_active(self.d.gladevcp)
		self.w.centerembededgvcp.set_active(self.d.centerembededgvcp)
		self.w.sideembededgvcp.set_active(self.d.sideembededgvcp)
		if(self.d.gladevcptype == GLADEVCP_DEFAULT):
			self.w.gui_rdo_default_gladevcp.set_active(True)
		elif(self.d.gladevcptype == GLADEVCP_CUSTOM):
			self.w.gui_rdo_custom_gladevcp.set_active(True)
		elif(self.d.gladevcptype == GLADEVCP_NONE):
			self.w.gui_rdo_none_gladevcp.set_active(True)
		
		# Pyvcp
		#self.w.gui_pyvcp.set_active(self.d.pyvcp)
		if(self.d.pyvcptype == PYVCP_DEFAULT):
			self.w.gui_rdo_default_pyvcp.set_active(True)
		elif(self.d.pyvcptype == PYVCP_CUSTOM):
			self.w.gui_rdo_custom_pyvcp.set_active(True)
		elif(self.d.pyvcptype == PYVCP_NONE):
			self.w.gui_rdo_none_pyvcp.set_active(True)
	
		# Get max speed (speed / minutes)
		if self.d.axes == 0: maxspeed = max(self.d.xmaxvel, self.d.ymaxvel, self.d.zmaxvel)* 60 # X Y Z
		elif self.d.axes == 1: maxspeed = max(self.d.xmaxvel, self.d.ymaxvel, self.d.zmaxvel)* 60 # X Y Z A (A is too fast, excluded)
		elif self.d.axes == 2: maxspeed = max(self.d.xmaxvel, self.d.zmaxvel)* 60 # X Z
		elif self.d.axes == 3: maxspeed = max(self.d.xmaxvel, self.d.ymaxvel, self.d.umaxvel, self.d.vmaxvel)* 60 # X Y U V
		self._p.jog_maxspeed = maxspeed
	
	def gui_page_finish(self):
		self.d.select_axis = self.w.gui_select_axis.get_active()
		self.d.select_gmoccapy = self.w.gui_select_gmoccapy.get_active()
		
		# Gladevcp
		if(self.w.gui_rdo_default_gladevcp.get_active() == True):
			self.d.gladevcptype = GLADEVCP_DEFAULT # default gladevcp
			self.d.gladevcpname = FILE_GLADEVCP_DEFAULT_GUI
		elif(self.w.gui_rdo_custom_gladevcp.get_active() == True):
			self.d.gladevcptype = GLADEVCP_CUSTOM # custom gladevcp
			self.d.gladevcpname = FILE_GLADEVCP_CUSTOM_GUI
		elif(self.w.gui_rdo_none_gladevcp.get_active() == True):
			self.d.gladevcptype = GLADEVCP_NONE # no gladevcp
			self.d.gladevcpname = ""
	
		# Pyvcp
		if(self.w.gui_rdo_default_pyvcp.get_active() == True):
			self.d.pyvcptype = PYVCP_DEFAULT
			self.d.pyvcpname = FILE_PYVCP_DEFAULT_GUI # name for [DISPLAY] section in INI file
			self.d.pyvcphaltype = 1
		elif(self.w.gui_rdo_custom_pyvcp.get_active() == True):
			self.d.pyvcptype = PYVCP_CUSTOM
			self.d.pyvcpname = FILE_PYVCP_CUSTOM_GUI # name for [DISPLAY] section in INI file
		elif(self.w.gui_rdo_none_pyvcp.get_active() == True):
			self.d.pyvcptype = PYVCP_NONE
			self.d.pyvcpname = ""
	
		if(self.w.gui_select_axis.get_active() == True):
			if(self.w.gui_rdo_gladevcp.get_active() == True):
				self.d.guitype = GUI_IS_GLADEVCP
			elif (self.w.gui_rdo_pyvcp.get_active() == True):
				self.d.guitype = GUI_IS_PYVCP
			else:
				self.d.guitype = GUI_IS_NONE
		elif(self.w.gui_select_gmoccapy.get_active() == True):
			self.d.guitype = GUI_IS_GMOCCAPY
		else:
			self.d.guitype = GUI_IS_NONE
	
		# Halui connections
		if(self.d.guitype == GUI_IS_PYVCP or self.d.guitype == GUI_IS_GLADEVCP):
			# Check if at least one default panel is selected
			if((self.d.pyvcptype == PYVCP_DEFAULT) or (self.d.gladevcptype == GLADEVCP_DEFAULT)): # default panel
				# MDI_COMMAND
				self.create_halui_mdi()
				self.d.halui = 1 # enable writing mdi_commands
	
			# Prepare hal components like or2, mux2, logic, etc.
			self.prepare_hal_components_for_single()
			# Single hal files
			if(self.d.guitype == GUI_IS_GLADEVCP and self.d.gladevcptype == PYVCP_DEFAULT):
				self.create_gladevcp_hal_single() # gladevcp
			elif(self.d.guitype == GUI_IS_PYVCP and self.d.pyvcptype == PYVCP_DEFAULT):
				self.create_pyvcp_hal_single() # pyvcp
	
		self.d.centerembededgvcp = self.w.centerembededgvcp.get_active()
		self.d.sideembededgvcp = self.w.sideembededgvcp.get_active()
	
	def on_gui_select_axis_toggled(self,*args):
		a= self.w.gui_select_axis.get_active()
		self.w.gui_rdo_gladevcp.set_sensitive(a)
		self.w.gui_rdo_pyvcp.set_sensitive(a)
		self.w.gui_rdo_nogui.set_sensitive(a)
	
		if(a == True):
			self.on_gui_rdo_gladevcp_toggled()
			self.on_gui_rdo_pyvcp_toggled()
			self.on_gui_rdo_nogui_toggled()
	
	def on_gui_select_gmoccapy_toggled(self,*args):
		c= self.w.gui_select_gmoccapy.get_active()
		g = False
		self.w.gui_rdo_default_gladevcp.set_sensitive(g)
		self.w.gui_rdo_custom_gladevcp.set_sensitive(g)
		self.w.gui_rdo_none_gladevcp.set_sensitive(g)
		self.w.gui_gladevcp_box.set_sensitive(g)
	
		self.w.gui_rdo_default_pyvcp.set_sensitive(g)
		self.w.gui_rdo_custom_pyvcp.set_sensitive(g)
		self.w.gui_rdo_none_pyvcp.set_sensitive(g)
		self.w.gui_pyvcp_box.set_sensitive(g)
	
	def on_gui_rdo_gladevcp_toggled(self,*args):
		g= self.w.gui_rdo_gladevcp.get_active()
		self.w.gui_rdo_default_gladevcp.set_sensitive(g)
		self.w.gui_rdo_custom_gladevcp.set_sensitive(g)
		self.w.gui_rdo_none_gladevcp.set_sensitive(g)
		self.w.gui_gladevcp_box.set_sensitive(g) 
	
	def on_gui_rdo_pyvcp_toggled(self,*args):
		p= self.w.gui_rdo_pyvcp.get_active()
		self.w.gui_rdo_default_pyvcp.set_sensitive(p)
		self.w.gui_rdo_custom_pyvcp.set_sensitive(p)
		self.w.gui_rdo_none_pyvcp.set_sensitive(p)
		self.w.gui_pyvcp_box.set_sensitive(p)
	
	def on_gui_rdo_nogui_toggled(self,*args):
		g= self.w.gui_rdo_gladevcp.get_active()
		self.w.gui_rdo_default_gladevcp.set_sensitive(g)
		self.w.gui_rdo_custom_gladevcp.set_sensitive(g)
		self.w.gui_rdo_none_gladevcp.set_sensitive(g)
		self.w.gui_gladevcp_box.set_sensitive(g)
		p= self.w.gui_rdo_pyvcp.get_active()
		self.w.gui_rdo_default_pyvcp.set_sensitive(p)
		self.w.gui_rdo_custom_pyvcp.set_sensitive(p)
		self.w.gui_rdo_none_pyvcp.set_sensitive(p)
		self.w.gui_pyvcp_box.set_sensitive(p)
	
	def create_halui_mdi(self):
		# X
		self._p.halui_list.append(MDI_G54X0)
		self._p.halui_mdi_x_null = len(self._p.halui_list) -1
		self._p.halui_list.append(MDI_G54TOX0)
		self._p.halui_mdi_x_zero = len(self._p.halui_list) -1
	
		# Y
		if self.d.axes in(0, 1, 3):
			self._p.halui_list.append(MDI_G54Y0)
			self._p.halui_mdi_y_null = len(self._p.halui_list) -1
			self._p.halui_list.append(MDI_G54TOY0)
			self._p.halui_mdi_y_zero = len(self._p.halui_list) -1
		# Z
		if self.d.axes in(0, 1, 2):
			self._p.halui_list.append(MDI_G54Z0)
			self._p.halui_mdi_z_null = len(self._p.halui_list) -1
			self._p.halui_list.append(MDI_G54TOZ0)
			self._p.halui_mdi_z_zero = len(self._p.halui_list) -1
		# A
		if self.d.axes == 1:
			self._p.halui_list.append(MDI_G54A0)
			self._p.halui_mdi_a_null = len(self._p.halui_list) -1
			self._p.halui_list.append(MDI_G54TOA0)
			self._p.halui_mdi_a_zero = len(self._p.halui_list) -1
		# UV
		if self.d.axes == 3:
			self._p.halui_list.append(MDI_G54U0)
			self._p.halui_mdi_u_null = len(self._p.halui_list) -1
			self._p.halui_list.append(MDI_G54TOU0)
			self._p.halui_mdi_u_zero = len(self._p.halui_list) -1
	
			self._p.halui_list.append(MDI_G54V0)
			self._p.halui_mdi_v_null = len(self._p.halui_list) -1
			self._p.halui_list.append(MDI_G54TOV0)
			self._p.halui_mdi_v_zero = len(self._p.halui_list) -1
	
		# Set save position
		self._p.halui_list.append("G30.1")
		self._p.halui_mdi_set_position = len(self._p.halui_list) -1
		self._p.halui_list.append("G30")
		self._p.halui_mdi_goto_position = len(self._p.halui_list) -1
	
		# TOOL LENGHT SENSOR
		inputs = self.build_input_set()
		if (d_hal_input[PROBE] in inputs):
			self._p.halui_list.append("o<probe_tool_lenght> call")
			self.d.halui_probe_tool_lenght = len(self._p.halui_list) -1
	
	def prepare_hal_components_for_single(self):
		#####################
		### OR2
		OR = self._p.maxor2
		# SPINDLE SPEED
		self._p.or2_spindle_speed = OR
		OR += 1
		self._p.maxor2 = OR
	
		
		#####################
		### AND2
		AND2 = self._p.maxand2
		# E-STOP
		self._p.and2_estop = AND2
		AND2 += 1
		self._p.maxand2 = AND2
	
		#####################
		### NOT
		NOT = self._p.maxnot
		# E-STOP
		self._p.not_estop = NOT
		NOT += 1
		self._p.maxnot = NOT
	
		#####################
		### TOGGLE
		TOGGLE = self._p.maxtoggle
		# E-STOP
		self._p.toggle_estop = TOGGLE
		TOGGLE += 1
		self._p.maxtoggle = TOGGLE
	
		#####################
		### TOGGLE2NIST
		TOGGLE2NIST = self._p.maxtoggle2nist
		# E-STOP
		self._p.toggle2nist_estop = TOGGLE2NIST
		TOGGLE2NIST += 1
		self._p.maxtoggle2nist = TOGGLE2NIST
	
		#####################
		### NEAR
		NEAR = self._p.maxnear
		# SPINDLE SPEED
		self._p.near_spindle = NEAR
		NEAR += 1
		self._p.near_cmd_spindle = NEAR
		NEAR += 1
		self._p.maxnear = NEAR
	
		#####################
		### MUX2
		MUX2 = self._p.maxmux2
		# SPINDLE
		self._p.mux2_spindle_speed = MUX2
		MUX2 += 1
		self._p.maxmux2 = MUX2
	
		#####################
		### LOGIC
		LOGIC = self._p.maxlogic
		# SPINDLE
		self._p.logic_spindle_speed = LOGIC
		LOGIC += 1
		self._p.maxlogic = LOGIC
	
		#####################
		### LUT5
		LUT5 = self._p.maxlut5
		# SPINDLE
		self._p.lut5_spindle_speed = LUT5
		LUT5 += 1
		self._p.maxlut5 = LUT5
	
	#***************
	# PYVCP TEST
	#***************
	def on_show_pyvcp_clicked(self,*args):
		self.test_pyvcp_panel()
	
	def test_pyvcp_panel(self):
		if self.w.gui_rdo_none_pyvcp.get_active() == True:
			# No panel selected
			return
	
		if self.w.gui_rdo_default_pyvcp.get_active() == True:
			# Prepare file
			panel = "pyvcp_test.xml"
			folder = "/tmp"
			filepath = os.path.join(folder, panel)
			self.create_pyvcp_panel(filepath)
			halrun = os.popen("cd %s\nhalrun -Is > /dev/null"%(folder), "w" )
			if self._p.debug:
				halrun.write("echo\n")
			halrun.write("loadusr -Wn displaytest pyvcp -c displaytest %(panel)s\n" %{'panel':panel,})
			halrun.write("waitusr displaytest\n")
			halrun.flush()
			halrun.close()
			os.remove(filepath)
		elif self.w.gui_rdo_custom_pyvcp.get_active() == True:
			panel = FILE_PYVCP_CUSTOM_GUI
			folder = os.path.expanduser("~/linuxcnc/configs/%s" % self.d.machinename)
			filepath = os.path.join(folder, panel)
			if not os.path.exists(filepath):
				self.warning_dialog (_("""You specified there is an existing pyvcp, \
				But there is not one in the machine-named folder.."""),True)
				return
			halrun = os.popen("cd %(folder)s\nhalrun -Is > /dev/null"% {'folder':folder,}, "w" )    
			halrun.write("loadusr -Wn displaytest pyvcp -c displaytest %(panel)s\n" %{'panel':panel,})
			halrun.write("waitusr displaytest\n")
			halrun.flush()
			halrun.close()
	
	#***************
	# GLADEVCP TEST
	#***************
	def on_show_gladepvc_clicked(self,*args):
		self.test_gladevcp_panel()
	
	def test_gladevcp_panel(self):
		pos = "+50+50"
		size = "200x200"
		options = ""
	
		if self.w.gui_rdo_none_gladevcp.get_active() == True:
			# No panel selected
			return
	
		if self.w.gui_rdo_default_gladevcp.get_active() == True:
			# Prepare file
			panel = "gvcp_test.glade"
			folder = "/tmp"
			filepath = os.path.join(folder, panel)
			self.create_gladevcp_panel(filepath) # Create panel
			halrun = os.popen("cd %s\nhalrun -Is > /dev/null"%(folder), "w" )
			if self._p.debug:
				halrun.write("echo\n")
			halrun.write("loadusr -Wn displaytest gladevcp -g %(size)s%(pos)s -c displaytest %(option)s %(panel)s\n" %{
							'size':size,'pos':pos,'option':options, 'panel':filepath})
			halrun.write("waitusr displaytest\n")
			halrun.flush()
			halrun.close()
			os.remove(filepath)
		elif self.w.gui_rdo_custom_gladevcp.get_active() == True:
			panel = FILE_GLADEVCP_CUSTOM_GUI
			folder = os.path.expanduser("~/linuxcnc/configs/%s" % self.d.machinename)
			filepath = os.path.join(folder, panel)
			if not os.path.exists(filepath):
				self.warning_dialog (_("""You specified there is an existing gladefile, \
				But there is not one in the machine-named folder.."""),True)
				return
	
			halrun = os.popen("cd %s\nhalrun -Is > /dev/null"%(folder), "w" )
			if self._p.debug:
				halrun.write("echo\n")
			halrun.write("loadusr -Wn displaytest gladevcp -g %(size)s%(pos)s -c displaytest %(option)s %(panel)s\n" %{
							'size':size,'pos':pos,'option':options, 'panel':filepath})
			halrun.write("waitusr displaytest\n")
			halrun.flush()
			halrun.close()

	####################################################################
	####################################################################
	#*************
	# FINISH PAGE
	#*************
	def finished_prepare(self):
		pass
	def finished_finish(self):
		self.build_config()
	
	def build_config(self):
		base = self.build_base()
		self.a.save_preferences()
		# Reset md5sums for recalculate
		self.d.md5sums = []
		self.a.INI.write_inifile(base)
		self.a.HAL.write_halfile(base)
		self.save(base)
		self.copy(base, "tool.tbl")
		if self.warning_dialog(MESS_QUIT,False):
			Gtk.main_quit()
	
	def build_base(self):
		base = os.path.expanduser("~/linuxcnc/configs/%s" % self.d.machinename)
		ncfiles = os.path.expanduser("~/linuxcnc/nc_files")
		if not os.path.exists(ncfiles):
			try:
				os.makedirs(ncfiles)
			except os.error, detail:
				if detail.errno != errno.EEXIST: raise
		
			examples = os.path.join(self._p.program_path, "share", "linuxcnc", "ncfiles")
			if not os.path.exists(examples):
				examples = os.path.join(self._p.program_path, "nc_files")
			if os.path.exists(examples):
				os.symlink(examples, os.path.join(ncfiles, "examples"))
		try:
			os.makedirs(base)
		except os.error, detail:
			if detail.errno != errno.EEXIST: raise
		return base
	
	
	def copy(self, base, filename):
		dest = os.path.join(base, filename)
		if not os.path.exists(dest):
			shutil.copy(os.path.join(self._p.distdir, filename), dest)
	
	def save(self,basedir):
		base = basedir
	
		if self.d.classicladder: 
			if not self.d.laddername == "custom.clp":
				filename = os.path.join(self._p.distdir, "configurable_options/ladder/%s" % self.d.laddername)
				original = os.path.expanduser("~/linuxcnc/configs/%s/custom.clp" % self.d.machinename)
				if os.path.exists(filename):     
					if os.path.exists(original):
						print "custom file already exists"
						shutil.copy( original,os.path.expanduser("~/linuxcnc/configs/%s/custom_backup.clp" % self.d.machinename) ) 
						print "made backup of existing custom"
					shutil.copy( filename,original)
					print "copied ladder program to usr directory"
					print"%s" % filename
				else:
					print "Master or temp ladder files missing from configurable_options dir"
	
		# Extra subroutine
		if (self.d.guitype == GUI_IS_PYVCP or self.d.guitype == GUI_IS_GLADEVCP ):
			if(self.d.pyvcptype == PYVCP_DEFAULT or self.d.gladevcptype == GLADEVCP_DEFAULT): # default panel
				self.create_simple_probe_routine()
		if (self.d.guitype == GUI_IS_PYVCP):
			originalname = os.path.expanduser("~/linuxcnc/configs/%s/%s" % (self.d.machinename, self.d.pyvcpname))
			if(self.d.pyvcptype == PYVCP_DEFAULT): # default panel
				# Create default panel
				self.create_pyvcp_panel(originalname)
			elif(self.d.pyvcptype == PYVCP_CUSTOM): # custom panel
				if os.path.exists(originalname):
					 print "custom PYVCP file already exists"
				else:
					self.create_pyvcp_custom(originalname)
					print "created PYVCP panel %s" % self.d.pyvcpname
			elif(self.d.pyvcptype == PYVCP_NONE): # no panel
				None
			else:
				print "Master PYVCP files missing from configurable_options dir"
	
		elif(self.d.guitype == GUI_IS_GLADEVCP):
			originalname = os.path.expanduser("~/linuxcnc/configs/%s/%s" % (self.d.machinename, self.d.gladevcpname))
			if(self.d.gladevcptype == GLADEVCP_DEFAULT): # default panel
				# Create default panel
				self.create_gladevcp_panel(originalname)
			elif(self.d.gladevcptype == GLADEVCP_CUSTOM): # custom panel
				if os.path.exists(originalname):
					 print "custom GLADEVCP file already exists"
				else:
					self.create_gladevcp_custom(originalname)
					print "created GLADEVCP panel %s" % self.d.gladevcpname
			elif(self.d.gladevcptype == GLADEVCP_NONE): # no panel
				None
			else:
				print "Master GladeVCP files missing from configurable_options dir"
			# Create gladevcp.py file
			originalname = os.path.expanduser("~/linuxcnc/configs/%s/%s" % (self.d.machinename, FILE_GLADEVCP_HANDLER))
			self.create_gladevcp_py(originalname)
	
		if(self.d.manual_tool_change == True):
			self.create_tool_change_routine()
			self.create_tool_job_begin_routine()
	
		filename = "%s.stepconf" % base
		d = xml.dom.minidom.getDOMImplementation().createDocument(None, "stepconf", None)
		e = d.documentElement
	
		for k, v in sorted(self.d.__dict__.iteritems()):
			if k.startswith("_"): continue
			n = d.createElement('property')
			e.appendChild(n)
	
			if isinstance(v, float): n.setAttribute('type', 'float')
			elif isinstance(v, bool): n.setAttribute('type', 'bool')
			elif isinstance(v, int): n.setAttribute('type', 'int')
			elif isinstance(v, list): n.setAttribute('type', 'eval')
			else: n.setAttribute('type', 'string')
	
			n.setAttribute('name', k)
			n.setAttribute('value', str(v))
		
		d.writexml(open(filename, "wb"), addindent="  ", newl="\n")
		print("%s" % base)
	
		# see http://freedesktop.org/wiki/Software/xdg-user-dirs
		desktop = commands.getoutput("""
			test -f ${XDG_CONFIG_HOME:-~/.config}/user-dirs.dirs && . ${XDG_CONFIG_HOME:-~/.config}/user-dirs.dirs
			echo ${XDG_DESKTOP_DIR:-$HOME/Desktop}""")
		if self.d.createsymlink:
			shortcut = os.path.join(desktop, self.d.machinename)
			if os.path.exists(desktop) and not os.path.exists(shortcut):
				os.symlink(base,shortcut)
	
		if self.d.createshortcut and os.path.exists(desktop):
			if os.path.exists(self._p.program_path + "/scripts/linuxcnc"):
				scriptspath = (self._p.program_path + "/scripts/linuxcnc")
			else:
				scriptspath ="linuxcnc"
	
			filename = os.path.join(desktop, "%s.desktop" % self.d.machinename)
			file = open(filename, "w")
			print >>file,"[Desktop Entry]"
			print >>file,"Version=1.0"
			print >>file,"Terminal=false"
			print >>file,"Name=" + _("launch %s") % self.d.machinename
			print >>file,"Exec=%s %s/%s.ini" \
						 % ( scriptspath, base, self.d.machinename )
			print >>file,"Type=Application"
			print >>file,"Comment=" + _("Desktop Launcher for LinuxCNC config made by Stepconf")
			print >>file,"Icon=%s"% self._p.linuxcncicon
			file.close()
			# Ubuntu 10.04 require launcher to have execute permissions
			os.chmod(filename,0775)
			
	####################################################################
	####################################################################
	####################################################################
	####################################################################
	#*******************
	# GUI Helper functions
	#*******************
	# print debug strings
	def dbg(self,str):
		if not self._p.debug:
			return
		print "DEBUG: %s"%str
	
	# pop up dialog
	def warning_dialog(self,message,is_ok_type):
		if is_ok_type:
		   dialog = Gtk.MessageDialog(self.w.window1,
				Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
				Gtk.MessageType.WARNING, Gtk.ButtonsType.OK,message)
		   dialog.show_all()
		   result = dialog.run()
		   dialog.destroy()
		   return True
		else:   
			dialog = Gtk.MessageDialog(self.w.window1,
			   Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
			   Gtk.MessageType.QUESTION, Gtk.ButtonsType.YES_NO, message)
			dialog.show_all()
			result = dialog.run()
			dialog.destroy()
			if result == Gtk.ResponseType.YES:
				return True
			else:
				return False


	def get_machine_preset(self, combo):
		tree_iter = combo.get_active_iter()
		if tree_iter != None:
			model = combo.get_model()
			name, row_id = model[tree_iter][:2]
			
			lcurrent_machine = filter(lambda element: element['index'] == row_id, preset.preset_machines)
			if(lcurrent_machine != []):
				# Just first element
				current_machine = lcurrent_machine[0]
				return(current_machine)
		else:
			# Other selected
			return(None)
	
	def select_combo_machine(self, combo, index):
		liststore = combo.get_model ()
		treeiter = liststore.get_iter_first()
		while treeiter != None:
			name, row_id = liststore[treeiter][:2]
			if(row_id == index):
				combo.set_active_iter(treeiter)
				return
			treeiter = liststore.iter_next(treeiter)
	
	def hz(self, axname):
		steprev = self.d[axname+"steprev"]
		microstep = self.d[axname+"microstep"]
		pulleynum = self.d[axname+"pulleynum"]
		pulleyden = self.d[axname+"pulleyden"]
		leadscrew = self.d[axname+"leadscrew"]
		maxvel = self.d[axname+"maxvel"]
		if self.d.units == MM or axname == 'a':
			leadscrew = 1./leadscrew
		pps = leadscrew * steprev * microstep * (pulleynum/pulleyden) * maxvel
		return abs(pps)
	
	def doublestep(self, steptime=None):
		if steptime is None: steptime = self.d.steptime
		return steptime <= 5000
	
	def minperiod(self, steptime=None, stepspace=None, latency=None):
		if steptime is None: steptime = self.d.steptime
		if stepspace is None: stepspace = self.d.stepspace
		if latency is None: latency = self.d.latency
		if self.doublestep(steptime):
			return max(latency + steptime + stepspace + 5000, 4*steptime)
		else:
			return latency + max(steptime, stepspace)
	
	def maxhz(self):
		return 1e9 / self.minperiod()
	
	def ideal_period(self):
		xhz = self.hz('x')
		yhz = self.hz('y')
		zhz = self.hz('z')
		uhz = self.hz('u')
		vhz = self.hz('v')
		ahz = self.hz('a')
		if self.d.axes == 1:
			pps = max(xhz, yhz, zhz, ahz)
		elif self.d.axes == 0:
			pps = max(xhz, yhz, zhz)
		elif self.d.axes == 2:
			pps = max(xhz, zhz)
		elif self.d.axes == 3:
			pps = max(xhz, yhz, uhz, vhz)
		else:
			print 'error in ideal period calculation - number of axes unrecognized'
			return
		if self.doublestep():
			base_period = 1e9 / pps
		else:
			base_period = .5e9 / pps
		if base_period > 100000: base_period = 100000
		if base_period < self.minperiod(): base_period = self.minperiod()
		return int(base_period)
	
	def md5sum(self, filename):
		try:
			f = open(filename, "rb")
		except IOError:
			print "error open %s file" % filename
			return None
		else:
			return hashlib.md5(f.read()).hexdigest()
	
	def add_md5sum(self, filename, mode="r"):
		md5 = self.md5sum(filename)
		self.d.md5sums.append((filename, md5))
	
	def __getitem__(self, item):
		return getattr(self, item)
	def __setitem__(self, item, value):
		return setattr(self, item, value)
	
	# check for spindle output signals
	def check_spindle_speed_control(self):
		# Check pp1 for output signals
		pp1_check =  d_hal_output[PWM] in (self.d.pin1, self.d.pin2, self.d.pin3, self.d.pin4, self.d.pin5, self.d.pin6,
			self.d.pin7, self.d.pin8, self.d.pin9, self.d.pin14, self.d.pin16, self.d.pin17)
		if pp1_check is True:
			self._p.has_spindle_speed_control = True
			return True
	
		# now check port 2, which can be set to 'in' or 'out' mode: so can have
		# other pins number to check then pp1
		# output pins:
		for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
			p = 'pp2_pin%d' % pin
			if self.d[p] == d_hal_output[PWM]:
				self._p.has_spindle_speed_control = True
				return True
	
		# if we get to here - there are no spindle control signals
		self._p.has_spindle_speed_control = False
		return False
	
	def check_spindle_encoder(self):
		# pp1 input pins
		if d_hal_input[PPR] in (self.d.pin10, self.d.pin11, self.d.pin12, self.d.pin13, self.d.pin15): return True
		if d_hal_input[PHA] in (self.d.pin10, self.d.pin11, self.d.pin12, self.d.pin13, self.d.pin15): return True
	
		# pp2 input pins
		for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
			p = 'pp2_pin%d_in' % pin
			if self.d[p] in (d_hal_input[PPR], d_hal_input[PHA]):
				self._p.has_spindle_encoder = False
				return True
	
		# if we get to here - there are no spindle encoder signals
		self._p.has_spindle_encoder = False
		return False

	# pport functions
	# disallow some signal combinations
	def do_exclusive_inputs(self, pin, port):
		# If initializing the Pport pages we don't want the signal calls to register here.
		# if we are working in here we don't want signal calls because of changes made in here
		# GTK supports signal blocking but then you can't assign signal names in GLADE -slaps head
		if self._p.in_pport_prepare or self._p.recursive_block: return
		self._p.recursive_block = True
		v = pin.get_active()
		ex = exclusive_input.get(v, ())
		
		# This part is probably useless. It is just an exercise with the GTK3 combobox.
		tree_iter = pin.get_active_iter()
		if tree_iter != None:
			model = pin.get_model()
			current_text = model[tree_iter][0]
		# Find function with current selected index
		lcurrent_function = filter(lambda element: element['index'] == v, hal_input)
		current_function = lcurrent_function[0]
		name = current_function["name"]
		index = current_function["index"]
	
		# search pport1 for the illegal signals and change them to unused.
		self.dbg( 'looking for %s in pport1'%name)
		for pin1 in (10,11,12,13,15):
			p = 'pin%d' % pin1
			if self.w[p] == pin: continue
			v1 = hal_input[self.w[p].get_active()]
			if v1["index"] in ex or v1["name"] == name:
				self.dbg( 'found %s, at %s'%(name,p))
				#self.w[p].set_active(self._p.hal_input_names.index(UNUSED_INPUT))
				self.w[p].set_active(UNUSED_INPUT)
				if not port ==1: # if on the other page must change the data model too
					self.dbg( 'found on other pport page')
					self.d[p] = d_hal_input[UNUSED_INPUT]
		# search pport2 for the illegal signals and change them to unused.
		self.dbg( 'looking for %s in pport2'%name)
		for pin1 in (2,3,4,5,6,7,8,9,10,11,12,13,15):
			p2 = 'pp2_pin%d_in' % pin1
			if self.w[p2] == pin: continue
			#v2 = self._p.hal_input_names[self.w[p2].get_active()]
			v2 = hal_input[self.w[p2].get_active()]
			if v2["index"] in ex or v2["name"] == name:
				self.dbg( 'found %s, at %s'%(name,p2))
				#self.w[p2].set_active(self._p.hal_input_names.index(UNUSED_INPUT))
				self.w[p2].set_active(UNUSED_INPUT)
				if not port ==2:# if on the other page must change the data model too
					self.dbg( 'found on other pport page')
					self.d[p2] = d_hal_input[UNUSED_INPUT]
		self._p.recursive_block = False
	
	
	#**********************************
	# Common helper functions
	#**********************************
	def build_input_set(self):
		input_set =[self.d.pin10, self.d.pin11, self.d.pin12, self.d.pin13, self.d.pin15]
		if self.d.number_pports > 1:
			#print "More pport"
			if self.d.pp2_direction:# Input option
				in_list =(2,3,4,5,6,7,8,9,10,11,12,13,15)
			else:
				in_list =(10,11,12,13,15)
			for pin in (in_list):
				p = 'pp2_pin%d_in' % pin
				input_set +=(self.d[p],)
		return set(input_set)
	
	def build_output_set(self):
		output_set =(self.d.pin1, self.d.pin2, self.d.pin3, self.d.pin4, self.d.pin5,
			self.d.pin6, self.d.pin7, self.d.pin8, self.d.pin9, self.d.pin14, self.d.pin16,
			self.d.pin17)
		if self.d.number_pports > 1:
			if self.d.pp2_direction:# Input option
				out_list =(1,14,16,17)
			else:
				out_list =(1,2,3,4,5,6,7,8,9,14,16,17)
			for pin in (out_list):
				p = 'pp2_pin%d' % pin
				output_set += (self.d[p],)
		return set(output_set)
	
	def find_output(self, output):
		found_list = []
		out_list = set((1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 16, 17))
		port = 0
		for i in out_list:
			pin = self.d["pin%d" % i]
			inv = self.d["pin%dinv" % i]
			if pin == output: found_list.append((i,port))
		if self.d.number_pports > 1:
			port = 1
			if self.d.pp2_direction:# Input option
				out_list =(1,14,16,17)
			else:
				out_list =(1,2,3,4,5,6,7,8,9,14,16,17)
			for i in (out_list):
				pin = self.d['pp2_pin%d' % i]
				if pin == output: found_list.append((i,port))
		return found_list
	
	
	def home_sig(self, axis):
		inputs = self.build_input_set()
		thisaxishome = set((d_hal_input[ALL_HOME], d_hal_input[ALL_LIMIT_HOME], "home-" + axis, "min-home-" + axis,
							"max-home-" + axis, "both-home-" + axis))
		for i in inputs:
			if i in thisaxishome: return i

	####################################################################
	####################################################################
	# BOILER CODE
	def __getitem__(self, item):
		return getattr(self, item)
	def __setitem__(self, item, value):
		return setattr(self, item, value)
=======
    def __init__(self, app):
        self.d = app.d      # collected data
        self.w = app.w      # widget names
        self.a = app        # parent, stepconf
        self._p = app._p    # private data
        global debug
        debug = self.a.debug
        global dbg
        dbg = self.a.dbg

#********************
# Notebook Controls
#********************
    def on_window1_destroy(self, *args):
        if self.a.warning_dialog (self._p.MESS_ABORT,False):
            Gtk.main_quit()
            return True
        else:
            return True

    # seaches (self._p.available_page) from the current page forward,
    # for the next page that is True or till second-to-last page.
    # if state found True: call current page finish function.
    # If that returns False then call the next page prepare function and show page
    def on_button_fwd_clicked(self,widget):
        cur = self.w.notebook1.get_current_page()
        u = cur+1
        cur_name,cur_text,cur_state = self._p.available_page[cur]
        while u < len(self._p.available_page):
            name,text,state = self._p.available_page[u]
            dbg( "FWD search %s,%s,%s,%s,of %d pages"%(u,name,text,state,len(self._p.available_page)-1))
            if state:
                if not self['%s_finish'%cur_name]():
                    self.w.notebook1.set_current_page(u)
                    dbg( 'prepare %s'% name)
                    self['%s_prepare'%name]()
                    self.w.title_label.set_text(text)
                    dbg("set %d current"%u)
                break
            u +=1
        # second-to-last page? change the fwd button to finish and show icon
        if u == len(self._p.available_page)-1:
            self.w.apply_image.set_visible(True)
            self.w.label_fwd.set_text(self._p.MESS_DONE)
        # last page? nothing to prepare just finish
        elif u == len(self._p.available_page):
            name,text,state = self._p.available_page[cur]
            self['%s_finish'%name]()
        # if comming from page 0 to page 1 sensitize 
        # the back button and change fwd button text
        if cur == 0:
            self.w.button_back.set_sensitive(True)
            self.w.label_fwd.set_text(self._p.MESS_FWD)

    # seaches (self._p.available_page) from the current page backward,
    # for the next page that is True or till first page.
    # if state found True: call current page finish function.
    # If that returns False then call the next page prepare function and show page
    def on_button_back_clicked(self,widget):
        cur = self.w.notebook1.get_current_page()
        u = cur-1
        cur_name,cur_text,cur_state = self._p.available_page[cur]
        while u > -1:
            name,text,state = self._p.available_page[u]
            dbg( "BACK search %s,%s,%s,%s,of %d pages"%(u,name,text,state,len(self._p.available_page)-1))
            if state:
                if not cur == len(self._p.available_page)-1:
                    self['%s_finish'%cur_name]()
                self.w.notebook1.set_current_page(u)
                self['%s_prepare'%name]()
                self.w.title_label.set_text(text)
                dbg("set %d current"%u)
                break
            u -=1
        # Not last page? change finish button text and hide icon
        if u <= len(self._p.available_page):
            self.w.apply_image.set_visible(False)
            self.w.label_fwd.set_text(self._p.MESS_FWD)
        # page 0 ? de-sensitize the back button and change fwd button text 
        if u == 0:
            self.w.button_back.set_sensitive(False)
            self.w.label_fwd.set_text(self._p.MESS_START)

    def set_buttons_sensitive(self,fstate,bstate):
        self.w.button_fwd.set_sensitive(fstate)
        self.w.button_back.set_sensitive(bstate)

    # Sets the visual state of a list of page(s)
    # The page names must be the one used in self._p.available_page
    # If a pages state is false it won't be seen or it's functions called.
    # if you deselect the current page it will show till next time it is cycled
    def page_set_state(self,page_list,state):
        dbg("page_set_state() %s ,%s"%(page_list,state))
        for i,data in enumerate(self._p.available_page):
            name,text,curstate = data
            if name in page_list:
                self._p.available_page[i][2] = state
                dbg("State changed to %s"% state)
                break

#####################################################
# All Page Methods
#####################################################
#***************
# Initialize
#***************
    def initialize(self):
        # one time initialized data
        liststore = self.w.drivertype.get_model()
        for i in self._p.alldrivertypes:
            #self.w.drivertype.append_text(i[1])
            liststore.append([i[1]])
        #self.w.drivertype.append_text(_("Other"))
        liststore.append([_("Other")])
        self.w.title_label.set_text(self._p.available_page[0][1])
        self.w.button_back.set_sensitive(False)
        self.w.label_fwd.set_text(self._p.MESS_START)
        if debug:
            self.w.window1.set_title('Stepconf -debug mode')
        # halui table
        renderer = Gtk.CellRendererText()
        column = Gtk.TreeViewColumn("Index", renderer, text=0)
        column.set_reorderable(False)
        self.w.viewTable1.append_column(column)
        renderer = Gtk.CellRendererText()
        renderer.set_property('editable', True)
        renderer.connect("edited", self.on_halui_row_changed)
        column = Gtk.TreeViewColumn("MDI__COMMAND", renderer, text=1)
        self.w.viewTable1.append_column(column)
        # pport1 combo boxes
        model = self.w.output_list
        model.clear()
        for name in self._p.human_output_names: model.append((name,))
        model = self.w.input_list
        model.clear()
        for name in self._p.human_input_names: model.append((name,))
        # pport2 comboboxes
        model = self.w.pp2_output_list
        model.clear()
        for ind,name in enumerate(self._p.human_output_names):
            if not ind in( 0,1,2,3,4,5,6,7):
                model.append((name,))
        model = self.w.pp2_input_list
        model.clear()
        for name in self._p.human_input_names: model.append((name,))
        self.intro_prepare()

#************
# INTRO PAGE
#************
    def intro_prepare(self):
        pass
    def intro_finish(self):
        pass

#***********
# start PAGE
#***********
    def start_prepare(self):
        self.w.createsymlink.set_active(self.d.createsymlink)
        self.w.createshortcut.set_active(self.d.createshortcut)
        self.w.createsimconfig.set_active(self.d.sim_hardware)

    def start_finish(self):
        if self.w.importmach.get_active():
            print 'Import Mach config'
            from stepconf import import_mach
            self.d.load('/tmp/temp.stepconf', self)
            if not debug:
                os.remove('/tmp/temp.stepconf')
        elif not self.w.createconfig.get_active():
            filter = Gtk.FileFilter()
            filter.add_pattern("*.stepconf")
            filter.set_name(_("LinuxCNC 'stepconf' configuration files"))
            dialog = Gtk.FileChooserDialog(_("Modify Existing Configuration"),
                self.w.window1, Gtk.FileChooserAction.OPEN,
                (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                Gtk.STOCK_OPEN, Gtk.ResponseType.OK))
            dialog.set_default_response(Gtk.ResponseType.OK)
            dialog.add_filter(filter)
            if not self.d._lastconfigname == "" and self.d._chooselastconfig:
                dialog.set_filename(os.path.expanduser("~/linuxcnc/configs/%s.stepconf"% self.d._lastconfigname))
            dialog.add_shortcut_folder(os.path.expanduser("~/linuxcnc/configs"))
            dialog.set_current_folder(os.path.expanduser("~/linuxcnc/configs"))
            dialog.show_all()
            result = dialog.run()
            if result == Gtk.ResponseType.OK:
                filename = dialog.get_filename()
                dialog.destroy()
                self.d.load(filename, self)
            else:
                dialog.destroy()
                return True
        self.d.createsymlink = self.w.createsymlink.get_active()
        self.d.createshortcut = self.w.createshortcut.get_active()
        self.d.sim_hardware = self.w.createsimconfig.get_active()

    # callbacks
    def on_machinename_changed(self, *args):
        temp = self.w.machinename.get_text()
        self.w.confdir.set_text("~/linuxcnc/configs/%s" % temp.replace(" ","_"))
    def on_drivertype_changed(self, *args):
        self.a.update_drivertype_info()

#************
# BASIC PAGE
#************
    def base_prepare(self):
        self.w.drivetime_expander.set_expanded(True)
        self.w.machinename.set_text(self.d.machinename)
        self.w.axes.set_active(self.d.axes)
        self.w.units.set_active(self.d.units)
        self.w.latency.set_value(self.d.latency)
        self.w.steptime.set_value(self.d.steptime)
        self.w.stepspace.set_value(self.d.stepspace)
        self.w.dirsetup.set_value(self.d.dirsetup)
        self.w.dirhold.set_value(self.d.dirhold)
        self.w.drivertype.set_active(self.a.drivertype_toindex())
        self.w.ioaddr.set_text(self.d.ioaddr)
        self.w.machinename.grab_focus()
        self.w.ioaddr2.set_text(self.d.ioaddr2) 
        #self.w.ioaddr3.set_text(self.d.ioaddr3)
        #self.w.pp3_direction.set_active(self.d.pp3_direction)
        if self.d.number_pports>2:
             self.w.radio_pp3.set_active(True)
        elif self.d.number_pports>1:
             self.w.radio_pp2.set_active(True)
        else:
             self.w.radio_pp1.set_active(True)

    def base_finish(self):
        self.w.drivetime_expander.set_expanded(False)
        machinename = self.w.machinename.get_text()
        self.d.machinename = machinename.replace(" ","_")
        self.d.axes = self.w.axes.get_active()
        self.d.units = self.w.units.get_active()
        self.d.drivertype = self.a.drivertype_toid(self.w.drivertype.get_active())
        self.d.steptime = self.w.steptime.get_value()
        self.d.stepspace = self.w.stepspace.get_value()
        self.d.dirsetup = self.w.dirsetup.get_value()
        self.d.dirhold = self.w.dirhold.get_value()
        self.d.latency = self.w.latency.get_value()
        if self.w.radio_pp3.get_active() and self.w.radio_pp2.get_active():
            self.d.number_pports = 3
        elif self.w.radio_pp2.get_active():
            self.d.number_pports = 2
        else:
            self.d.number_pports = 1
        self.page_set_state('pport2',self.w.radio_pp2.get_active())
        # Get item selected in combobox
        tree_iter = self.w.axes.get_active_iter()
        model = self.w.axes.get_model()
        text_selected = model[tree_iter][0]
        dbg("active axes: %s = %d"% (text_selected,self.d.axes))
        self.page_set_state('axisz','Z' in text_selected)
        self.page_set_state('axisy','Y' in text_selected)
        self.page_set_state('axisu','U' in text_selected)
        self.page_set_state('axisv','V' in text_selected)
        self.page_set_state('axisa','A' in text_selected)

    # Basic page callbacks
    def on_pp2_checkbutton_toggled(self, *args): 
        i = self.w.pp2_checkbutton.get_active()   
        self.w.pp2_direction.set_sensitive(i)
        self.w.ioaddr2.set_sensitive(i)
        if i == 0:
           self.w.pp3_checkbutton.set_active(i)
           self.w.ioaddr3.set_sensitive(i)

    def on_pp3_checkbutton_toggled(self, *args): 
        i = self.w.pp3_checkbutton.get_active() 
        if self.w.pp2_checkbutton.get_active() ==0:
          i=0  
          self.w.pp3_checkbutton.set_active(0)
        self.w.pp3_direction.set_sensitive(i)
        self.w.ioaddr3.set_sensitive(i)

    def on_latency_test_clicked(self,widget):
        self.a.run_latency_test()

    def on_calculate_ideal_period(self,widget):
        self.a.calculate_ideal_period()

    def on_units_changed(self,widget):
        if not self.d.units == widget.get_active():
            # change the XYZ axis defaults to metric or imperial
            # This erases any entered data that would make sense to change
            self.d.set_axis_unit_defaults(not widget.get_active())

#***************
# options PAGE
#***************
    def options_prepare(self):
        self.w.pyvcp.set_active(self.d.pyvcp)
        self.on_pyvcp_toggled()
        if  not self.w.createconfig.get_active():
           if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custompanel.xml" % self.d.machinename)):
                self.w.radiobutton8.set_active(True)
        self.w.select_axis.set_active(self.d.select_axis)
        self.w.select_gmoccapy.set_active(self.d.select_gmoccapy)
        self.w.classicladder.set_active(self.d.classicladder)
        self.w.modbus.set_active(self.d.modbus)
        self.w.digitsin.set_value(self.d.digitsin)
        self.w.digitsout.set_value(self.d.digitsout)
        self.w.s32in.set_value(self.d.s32in)
        self.w.s32out.set_value(self.d.s32out)
        self.w.floatsin.set_value(self.d.floatsin)
        self.w.floatsout.set_value(self.d.floatsout)
        self.w.halui.set_active(self.d.halui)
        self.page_set_state('halui_page', self.w.halui.get_active())
        self.w.ladderconnect.set_active(self.d.ladderconnect)
        self.w.pyvcpconnect.set_active(self.d.pyvcpconnect)
        self.on_classicladder_toggled()
        self.w.manualtoolchange.set_active(self.d.manualtoolchange)
        if  not self.w.createconfig.get_active():
           if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custom.clp" % self.d.machinename)):
                self.w.radiobutton4.set_active(True)

    def options_finish(self):
        SIG = self._p
        self.d.select_axis = self.w.select_axis.get_active()
        self.d.select_gmoccapy = self.w.select_gmoccapy.get_active()
        self.d.pyvcp = self.w.pyvcp.get_active()
        self.d.classicladder = self.w.classicladder.get_active()
        self.d.modbus = self.w.modbus.get_active()
        self.d.digitsin = self.w.digitsin.get_value()
        self.d.digitsout = self.w.digitsout.get_value()
        self.d.s32in = self.w.s32in.get_value()
        self.d.s32out = self.w.s32out.get_value()
        self.d.floatsin = self.w.floatsin.get_value()
        self.d.floatsout = self.w.floatsout.get_value()
        self.d.halui = self.w.halui.get_active()
        self.d.pyvcpconnect = self.w.pyvcpconnect.get_active()  
        self.d.ladderconnect = self.w.ladderconnect.get_active()   
        self.d.manualtoolchange = self.w.manualtoolchange.get_active()       
        if self.d.classicladder:
           if self.w.radiobutton1.get_active() == True:
              if self.d.tempexists:
                   self.d.laddername='TEMP.clp'
              else:
                   self.d.laddername= 'blank.clp'
                   self.d.ladderhaltype = 0
           if self.w.radiobutton2.get_active() == True:
              self.d.laddername = 'estop.clp'
              inputs = self.a.build_input_set()
              if SIG.ESTOP_IN not in inputs:
                 self.a.warning_dialog(self._p.MESS_NO_ESTOP,True)
                 return True # don't advance the page
              self.d.ladderhaltype = 1
           if self.w.radiobutton3.get_active() == True:
                 self.d.laddername = 'serialmodbus.clp'
                 self.d.modbus = 1
                 self.w.modbus.set_active(self.d.modbus) 
                 self.d.ladderhaltype = 0          
           if self.w.radiobutton4.get_active() == True:
              self.d.laddername='custom.clp'
           else:
               if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custom.clp" % self.d.machinename)):
                  if not self.a.warning_dialog(self._p.MESS_CL_REWRITE,False):
                     return True # don't advance the page
           if self.w.radiobutton1.get_active() == False:
              if os.path.exists(os.path.join(self._p.distdir, "configurable_options/ladder/TEMP.clp")):
                 if not self.a.warning_dialog(self._p.MESS_CL_EDITTED,False):
                   return True # don't advance the page
        if self.d.pyvcp == True:
           if self.w.radiobutton5.get_active() == True:
              self.d.pyvcpname = "blank.xml"
              self.pyvcphaltype = 0
           if self.w.radiobutton6.get_active() == True:
              self.d.pyvcpname = "spindle.xml"
              self.d.pyvcphaltype = 1
           if self.w.radiobutton8.get_active() == True:
              self.d.pyvcpname = "custompanel.xml"
           else:
              if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custompanel.xml" % self.d.machinename)):
                 if not self.a.warning_dialog(self._p.MESS_PYVCP_REWRITE,False):
                   return True

    # options page callback
    def on_loadladder_clicked(self, *args):
        self.a.load_ladder(self)

    def on_display_pyvcp_clicked(self,*args):
        self.a.testpanel(self)

    def on_classicladder_toggled(self, *args):
        i= self.w.classicladder.get_active()
        self.w.ladder_box.set_sensitive(i)
        if  self.w.createconfig.get_active():
            self.w.radiobutton4.set_sensitive(False)
        else:
            self.w.radiobutton4.set_sensitive(i)
        if not i:
            self.w.clpins_expander.set_expanded(False)

    def on_pyvcp_toggled(self,*args):
        i= self.w.pyvcp.get_active()
        if  self.w.createconfig.get_active():
            self.w.radiobutton8.set_sensitive(False)
        else:
            self.w.radiobutton8.set_sensitive(i)
        self.w.pyvcp_box.set_sensitive(i)

    def on_halui_toggled(self, *args):
        self.page_set_state('halui_page', self.w.halui.get_active())

#***************
# halui PAGE
#***************
    def halui_page_prepare(self):
        # Clear listore
        self.w.lstStore1.clear()
        # Populate treeview
        for num, mdi_command in enumerate(self.d.halui_list):
            self.w.lstStore1.append([num+1, mdi_command])

    def halui_page_finish(self):
        self.d.halui_list = []
        # Get first row
        treeiter = self.w.lstStore1.get_iter_first()
        if treeiter == None:
            return False
        self.d.halui_list.append(self.w.lstStore1.get_value(treeiter, 1))
        while treeiter != None:
            treeiter = self.w.lstStore1.iter_next(treeiter)
            if treeiter != None:
                current_value = self.w.lstStore1.get_value(treeiter, 1).strip()
                # Check if value contains data
                if len(current_value) > 2:
                    self.d.halui_list.append(current_value)

    def on_halui_btnAdd_clicked(self, *args):
        next_index = len(self.w.lstStore1) +1
        self.w.lstStore1.append([next_index, ""])

    def on_halui_btnDel_clicked(self, *args):
        select = self.w.viewTable1.get_selection()
        model, treeiter = select.get_selected()
        if treeiter != None:
            # Remove selected row
            self.w.lstStore1.remove(treeiter)
            # Get first row
            treeiter = self.w.lstStore1.get_iter_first()
            if treeiter == None:
                return
            index = 1
            self.w.lstStore1.set_value(treeiter, 0, index)
            index = index +1
            # Cicle lstStore1 to update index
            while treeiter != None:
                treeiter = self.w.lstStore1.iter_next(treeiter)
                if treeiter != None:
                    # Change index
                    self.w.lstStore1.set_value(treeiter, 0, index)
                    index = index +1

    def on_halui_btnUp_clicked(self, *args):
        select = self.w.viewTable1.get_selection()
        model, treeiter = select.get_selected()
        if treeiter != None:
            prev_treeiter = model.iter_previous(treeiter)
            current_index = model[treeiter][0]
            # Move up and update first column (index)
            if((current_index -1) > 0):
                self.w.lstStore1.move_before(treeiter, prev_treeiter)
                self.w.lstStore1.set_value(treeiter, 0, current_index -1)
                self.w.lstStore1.set_value(prev_treeiter, 0, current_index)

    def on_halui_btnDown_clicked(self, *args):
        select = self.w.viewTable1.get_selection()
        model, treeiter = select.get_selected()
        if treeiter != None:
            next_treeiter = model.iter_next(treeiter)
            current_index = model[treeiter][0]
            # Move down and update first column (index)
            if(next_treeiter != None):
                self.w.lstStore1.move_after(treeiter, next_treeiter)
                self.w.lstStore1.set_value(treeiter, 0, current_index +1)
                self.w.lstStore1.set_value(next_treeiter, 0, current_index)

    def on_halui_row_changed(self, *args):
        newvalue = args[2]
        if len(newvalue.strip()) < 2:
            return
        select = self.w.viewTable1.get_selection()
        model, treeiter = select.get_selected()
        self.w.lstStore1.set_value(treeiter, 1, newvalue)

#************
# pport1 PAGE
#************
    def pport1_prepare(self):
        self._p.in_pport_prepare = True
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pin%d' % pin
            self.w[p].set_wrap_width(3)
            self.w[p].set_active(self._p.hal_output_names.index(self.d[p]))
            p = 'pin%dinv' % pin
            self.w[p].set_active(self.d[p])
        for pin in (10,11,12,13,15):
            p = 'pin%d' % pin
            self.w[p].set_wrap_width(3)
            self.w[p].set_active(self._p.hal_input_names.index(self.d[p]))
            p = 'pin%dinv' % pin
            self.w[p].set_active(self.d[p])
        self.w.pin1.grab_focus()
        self.w.ioaddr.set_text(self.d.ioaddr)
        self._p.in_pport_prepare = False

    def pport1_finish(self):
        for pin in (10,11,12,13,15):
            p = 'pin%d' % pin
            self.d[p] = self._p.hal_input_names[self.w[p].get_active()]
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pin%d' % pin
            self.d[p] = self._p.hal_output_names[self.w[p].get_active()]
        for pin in (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17):
            p = 'pin%dinv' % pin
            self.d[p] = self.w[p].get_active()
        self.d.ioaddr = self.w.ioaddr.get_text()
        self.page_set_state('spindle',(self.a.has_spindle_speed_control() or self.a.has_spindle_encoder()) )

    # pport1 callbacks
    def on_exclusive_check_pp1(self, widget):
        self.a.do_exclusive_inputs(widget,1)

    def on_preselect_button_clicked(self, widget):
        state = self.w.preset_combo.get_active()
        print state
        if state == 0:
            self.a.preset_sherline_outputs()
        elif state ==1:
            self.a.preset_xylotex_outputs()
        elif state ==2:
            self.a.preset_tb6560_3axes_outputs()
        elif state ==3:
            self.a.preset_tb6560_4axes_outputs()

#************
# pport2 PAGE
#************
    def pport2_prepare(self):
        self._p.in_pport_prepare = True
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pp2_pin%d' % pin
            self.w[p].set_wrap_width(3)
            self.w[p].set_active(self._p.hal_output_names.index(self.d[p])-8)
            p = 'pp2_pin%dinv' % pin
            self.w[p].set_active(self.d[p])

        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = 'pp2_pin%d_in' % pin
            self.w[p].set_wrap_width(3)
            self.w[p].set_active(self._p.hal_input_names.index(self.d[p]))
            p = 'pp2_pin%d_in_inv' % pin
            self.w[p].set_active(self.d[p])
        self.w.pp2_pin1.grab_focus()
        self.w.pp2_direction.set_active(self.d.pp2_direction)
        self.on_pp2_direction_changed(self.w.pp2_direction)
        self.w.ioaddr2.set_text(self.d.ioaddr2)
        self._p.in_pport_prepare = False

    def pport2_finish(self):
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pp2_pin%d' % pin
            self.d[p] = self._p.hal_output_names[self.w[p].get_active()+8]
            p = 'pp2_pin%dinv' % pin
            self.d[p] = self.w[p].get_active()
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = 'pp2_pin%d_in' % pin
            self.d[p] = self._p.hal_input_names[self.w[p].get_active()]
            p = 'pp2_pin%d_in_inv' % pin
            self.d[p] = self.w[p].get_active()
        self.d.pp2_direction = self.w.pp2_direction.get_active()
        self.d.ioaddr2 = self.w.ioaddr2.get_text()
        self.page_set_state('spindle',(self.a.has_spindle_speed_control() or self.a.has_spindle_encoder()) )

    # pport2 callbacks:
    def on_pp2_direction_changed(self,widget):
        state = widget.get_active()
        for i in (2,3,4,5,6,7,8,9):
            self.w['pp2_pin%s_in_box'%i].set_visible(state)
            self.w['pp2_pin%s_out_box'%i].set_visible(not state)

    def on_exclusive_check_pp2(self, widget):
        self.a.do_exclusive_inputs(widget,2)


#*******************
# AXIS X PAGE
#*******************
    def axisx_prepare(self):
        self.axis_prepare('x')
    def axisx_finish(self):
        self.axis_done('x')
    # AXIS X callbacks
    def on_xsteprev_changed(self, *args): self.a.update_pps('x')
    def on_xmicrostep_changed(self, *args): self.a.update_pps('x')
    def on_xpulleyden_changed(self, *args): self.a.update_pps('x')
    def on_xpulleynum_changed(self, *args): self.a.update_pps('x')
    def on_xleadscrew_changed(self, *args): self.a.update_pps('x')
    def on_xmaxvel_changed(self, *args): self.a.update_pps('x')
    def on_xmaxacc_changed(self, *args): self.a.update_pps('x')
    def on_xaxistest_clicked(self, *args): self.a.test_axis('x')

#********************
# AXIS Y
#********************
    def axisy_prepare(self):
        self.axis_prepare('y')
    def axisy_finish(self):
        self.axis_done('y')
    # AXIS Y callbacks
    def on_ysteprev_changed(self, *args): self.a.update_pps('y')
    def on_ymicrostep_changed(self, *args): self.a.update_pps('y')
    def on_ypulleyden_changed(self, *args): self.a.update_pps('y')
    def on_ypulleynum_changed(self, *args): self.a.update_pps('y')
    def on_yleadscrew_changed(self, *args): self.a.update_pps('y')
    def on_ymaxvel_changed(self, *args): self.a.update_pps('y')
    def on_ymaxacc_changed(self, *args): self.a.update_pps('y')
    def on_yaxistest_clicked(self, *args): self.a.test_axis('y')

#********************
# AXIS Z PAGE
#********************
    def axisz_prepare(self):
        self.axis_prepare('z')
    def axisz_finish(self):
        self.axis_done('z')
    # AXIS Z callbacks
    def on_zsteprev_changed(self, *args): self.a.update_pps('z')
    def on_zmicrostep_changed(self, *args): self.a.update_pps('z')
    def on_zpulleyden_changed(self, *args): self.a.update_pps('z')
    def on_zpulleynum_changed(self, *args): self.a.update_pps('z')
    def on_zleadscrew_changed(self, *args): self.a.update_pps('z')
    def on_zmaxvel_changed(self, *args): self.a.update_pps('z')
    def on_zmaxacc_changed(self, *args): self.a.update_pps('z')
    def on_zaxistest_clicked(self, *args): self.a.test_axis('z')

#********************
# AXIS U PAGE
#********************
    def axisu_prepare(self):
        self.axis_prepare('u')
    def axisu_finish(self):
        self.axis_done('u')
    # AXIS U callbacks
    def on_usteprev_changed(self, *args): self.a.update_pps('u')
    def on_umicrostep_changed(self, *args): self.a.update_pps('u')
    def on_upulleyden_changed(self, *args): self.a.update_pps('u')
    def on_upulleynum_changed(self, *args): self.a.update_pps('u')
    def on_uleadscrew_changed(self, *args): self.a.update_pps('u')
    def on_umaxvel_changed(self, *args): self.a.update_pps('u')
    def on_umaxacc_changed(self, *args): self.a.update_pps('u')
    def on_uaxistest_clicked(self, *args): self.a.test_axis('u')

#********************
# AXIS V PAGE
#********************
    def axisv_prepare(self):
        self.axis_prepare('v')
    def axisv_finish(self):
        self.axis_done('v')
    # AXIS V callbacks
    def on_vsteprev_changed(self, *args): self.a.update_pps('v')
    def on_vmicrostep_changed(self, *args): self.a.update_pps('v')
    def on_vpulleyden_changed(self, *args): self.a.update_pps('v')
    def on_vpulleynum_changed(self, *args): self.a.update_pps('v')
    def on_vleadscrew_changed(self, *args): self.a.update_pps('v')
    def on_vmaxvel_changed(self, *args): self.a.update_pps('v')
    def on_vmaxacc_changed(self, *args): self.a.update_pps('v')
    def on_vaxistest_clicked(self, *args): self.a.test_axis('v')

#********************
# AXIS A PAGE
#********************
    def axisa_prepare(self):
        self.axis_prepare('a')
    def axisa_finish(self):
        self.axis_done('a')
    # AXIS A callbacks
    def on_asteprev_changed(self, *args): self.a.update_pps('a')
    def on_amicrostep_changed(self, *args): self.a.update_pps('a')
    def on_apulleyden_changed(self, *args): self.a.update_pps('a')
    def on_apulleynum_changed(self, *args): self.a.update_pps('a')
    def on_aleadscrew_changed(self, *args): self.a.update_pps('a')
    def on_amaxvel_changed(self, *args): self.a.update_pps('a')
    def on_amaxacc_changed(self, *args): self.a.update_pps('a')
    def on_aaxistest_clicked(self, *args): self.a.test_axis('a')

#*********************
# General Axis methods and callbacks
#*********************

    def on_jogminus_pressed(self, *args):
        self.a.jogminus = 1
        self.a.update_axis_test()

    def on_jogminus_released(self, *args):
        self.a.jogminus = 0
        self.a.update_axis_test()

    def on_jogplus_pressed(self, *args):
        self.a.jogplus = 1
        self.a.update_axis_test()
    def on_jogplus_released(self, *args):
        self.a.jogplus = 0
        self.a.update_axis_test()

    def update_axis_params(self, *args):
        self.a.update_axis_test()

    def axis_prepare(self, axis):
        def set_text(n):
            self.w[axis + n].set_text("%s" % self.d[axis + n])
            # Set a name for this widget. Necessary for css id
            self.w[axis + n].set_name("%s%s" % (axis, n))
        def set_active(n):
            self.w[axis + n].set_active(self.d[axis + n])
        SIG = self._p
        set_text("steprev")
        set_text("microstep")
        set_text("pulleynum")
        set_text("pulleyden")
        set_text("leadscrew")
        set_text("maxvel")
        set_text("maxacc")
        set_text("homepos")
        set_text("minlim")
        set_text("maxlim")
        set_text("homesw")
        set_text("homevel")
        set_active("latchdir")

        if axis == "a":
            self.w[axis + "screwunits"].set_text(_("degree / rev"))
            self.w[axis + "velunits"].set_text(_("deg / s"))
            self.w[axis + "accunits"].set_text(_(u"deg / s²"))
            self.w[axis + "accdistunits"].set_text(_("deg"))
            self.w[axis + "scaleunits"].set_text(_("Steps / deg"))
        elif self.d.units:
            self.w[axis + "screwunits"].set_text(_("mm / rev"))
            self.w[axis + "velunits"].set_text(_("mm / s"))
            self.w[axis + "accunits"].set_text(_(u"mm / s²"))
            self.w[axis + "accdistunits"].set_text(_("mm"))
            self.w[axis + "scaleunits"].set_text(_("Steps / mm"))
        else:
            self.w[axis + "screwunits"].set_text(_("rev / in"))
            self.w[axis + "velunits"].set_text(_("in / s"))
            self.w[axis + "accunits"].set_text(_(u"in / s²"))
            self.w[axis + "accdistunits"].set_text(_("in"))
            self.w[axis + "scaleunits"].set_text(_("Steps / in"))

        inputs = self.a.build_input_set()
        thisaxishome = set((SIG.ALL_HOME, SIG.ALL_LIMIT_HOME, "home-" + axis, "min-home-" + axis,
                            "max-home-" + axis, "both-home-" + axis))
        # Test if exists limit switches
        homes = bool(inputs & thisaxishome)
        self.w[axis + "homesw"].set_sensitive(homes)
        self.w[axis + "homevel"].set_sensitive(homes)
        self.w[axis + "latchdir"].set_sensitive(homes)

        self.w[axis + "steprev"].grab_focus()
        GObject.idle_add(lambda: self.a.update_pps(axis))

    def axis_done(self, axis):
        def get_text(n): self.d[axis + n] = float(self.w[axis + n].get_text())
        def get_active(n): self.d[axis + n] = self.w[axis + n].get_active()
        get_text("steprev")
        get_text("microstep")
        get_text("pulleynum")
        get_text("pulleyden")
        get_text("leadscrew")
        get_text("maxvel")
        get_text("maxacc")
        get_text("homepos")
        get_text("minlim")
        get_text("maxlim")
        get_text("homesw")
        get_text("homevel")
        get_active("latchdir")

#*********
# SPINDLE PAGE
#*********
    def spindle_prepare(self):
        SIG = self._p
        self.w['spindlecarrier'].set_text("%s" % self.d.spindlecarrier)
        self.w['spindlespeed1'].set_text("%s" % self.d.spindlespeed1)
        self.w['spindlespeed2'].set_text("%s" % self.d.spindlespeed2)
        self.w['spindlepwm1'].set_text("%s" % self.d.spindlepwm1)
        self.w['spindlepwm2'].set_text("%s" % self.d.spindlepwm2)
        self.w['spindlecpr'].set_text("%s" % self.d.spindlecpr)
        self.w['spindlenearscale'].set_value(self.d.spindlenearscale * 100)
        self.w['spindlefiltergain'].set_value(self.d.spindlefiltergain)
        self.w['usespindleatspeed'].set_active(self.d.usespindleatspeed)

        if self.a.has_spindle_encoder():
            self.w.spindlecpr.show()
            self.w.spindlecprlabel.show()
            self.w.spindlefiltergain.show()
            self.w.spindlefiltergainlabel.show()
            self.w.spindlenearscale.show()
            self.w.usespindleatspeed.show()
            self.w.spindlenearscaleunitlabel.show()
        else:
            self.w.spindlecpr.hide()
            self.w.spindlecprlabel.hide()
            self.w.spindlefiltergain.hide()
            self.w.spindlefiltergainlabel.hide()
            self.w.spindlenearscale.hide()
            self.w.usespindleatspeed.hide()
            self.w.spindlenearscaleunitlabel.hide()

        self.w.output.set_sensitive(self.a.has_spindle_speed_control())

    def spindle_finish(self):
        self.d.spindlecarrier = float(self.w.spindlecarrier.get_text())
        self.d.spindlespeed1 = float(self.w.spindlespeed1.get_text())
        self.d.spindlespeed2 = float(self.w.spindlespeed2.get_text())
        self.d.spindlepwm1 = float(self.w.spindlepwm1.get_text())
        self.d.spindlepwm2 = float(self.w.spindlepwm2.get_text())
        self.d.spindlecpr = float(self.w.spindlecpr.get_text())
        self.d.spindlenearscale = self.w.spindlenearscale.get_value()/100
        self.d.spindlefiltergain = self.w.spindlefiltergain.get_value()
        self.d.usespindleatspeed = self.w['usespindleatspeed'].get_active()

    # Spindle page callbacks
    def on_usespindleatspeed_toggled(self,*args):
        self.w.spindlenearscale.set_sensitive(self.w.usespindleatspeed.get_active())

#*************
# FINISH PAGE
#*************
    def finished_prepare(self):
        self.w.label_simulator_warning.set_visible(self.w.createsimconfig.get_active())
    def finished_finish(self):
        self.a.buid_config()

# BOILER CODE
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)
>>>>>>> upstream/master
