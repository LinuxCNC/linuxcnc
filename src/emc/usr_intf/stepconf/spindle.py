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
#*********
# SPINDLE PAGE
#*********
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
