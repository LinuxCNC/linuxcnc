#!/usr/bin/env python
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#************
# BASIC PAGE
#************
from gi.repository import GObject
from stepconf.definitions import *
from stepconf import preset

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
	self.d.select_combo_machine(self.w.base_preset_combo, preset_index)
	preset_index = self.d.drivertype
	self.d.select_combo_machine(self.w.drivertype, preset_index)
	
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
	current_machine = self.d.get_machine_preset(self.w.drivertype)
	if current_machine:
		self.d.drivertype = current_machine["index"]
	else:
		# Other selected
		self.d.drivertype = 0
	current_machine = self.d.get_machine_preset(self.w.base_preset_combo)
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

	current_machine = self.d.get_machine_preset(self.w.drivertype)
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
	minperiod = self.d.minperiod(steptime, stepspace, latency)
	maxhz = int(1e9 / minperiod)
	if not self.d.doublestep(steptime): maxhz /= 2
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
		self.d.set_axis_unit_defaults(not widget.get_active())

def on_base_preselect_button_clicked(self, widget):
	current_machine = self.d.get_machine_preset(self.w.base_preset_combo)
	if current_machine:
		self.base_general_preset(current_machine)
		ctx = self.w.base_preselect_button.get_style_context()
		ctx.remove_class('normal')
		ctx.add_class('selected')

def base_general_preset(self, current_machine):
	# base
	self.d.select_combo_machine(self.w.drivertype, current_machine["index"])
	# pport1
	self.pport1_prepare()
	self.d.select_combo_machine(self.w.pp1_preset_combo, current_machine["index"])
	self.on_pp1_preselect_button_clicked(None)
	self.pport1_finish()
	# axis
	for axis in ('x','y','z','u','v'):
		self.axis_prepare(axis)
		self.d.select_combo_machine(self.w[axis + "preset_combo"], current_machine["index"])
		self.preset_axis(axis)
		self.axis_done(axis)
	# options: preset probe coordinates  in options page
	self.option_preset()
	return


