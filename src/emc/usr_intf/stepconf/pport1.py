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
# pport1 PAGE
#************
from gi.repository import Gtk
from stepconf import preset
from stepconf.definitions import *

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

	self.w.pin1.grab_focus()
	self.w.ioaddr.set_text(self.d.ioaddr)
	self._p.in_pport_prepare = False


def pport1_finish(self):
	for pin in (10,11,12,13,15):
		p = 'pin%d' % pin
		#self.d[p] = self._p.hal_input_names[self.w[p].get_active()]
		#self.d[p] = hal_input[self.w[p].get_active()]["index"]
		self.d[p] = hal_input[self.w[p].get_active()]["name"]
	for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
		p = 'pin%d' % pin
		#self.d[p] = self._p.hal_output_names[self.w[p].get_active()]
		#self.d[p] = hal_output[self.w[p].get_active()]["index"]
		self.d[p] = hal_output[self.w[p].get_active()]["name"]
	for pin in (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17):
		p = 'pin%dinv' % pin
		self.d[p] = self.w[p].get_active()
	self.d.ioaddr = self.w.ioaddr.get_text()
	self.page_set_state('spindle',(self.a.has_spindle_speed_control() or self.a.has_spindle_encoder()) )

# pport1 callbacks
def on_exclusive_check_pp1(self, widget):
	self.a.do_exclusive_inputs(widget,1)

def on_pp1_preselect_button_clicked(self, widget):
	current_machine = self.d.get_machine_preset(self.w.pp1_preset_combo)
	if current_machine:
		None
	else:
		# Other selected
		return
	# Test output & intput & inverted
	for pin in (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17):
		p = 'pin%d' % pin
		if(p in current_machine):
			self.w['%s'%p].set_active(current_machine[p])
		p = 'pin%dinv' % pin
		if(p in current_machine):
			self.w['%s'%p].set_active(current_machine[p])

def on_pp1_preset_io_button_clicked(self, widget):
	state = self.w.pp1_preset_io_combo.get_active()
	if(state > -1):
		path = Gtk.TreePath(state)
		treeiter = self.w.pp1_preset_io_liststore.get_iter(path)
		value = self.w.pp1_preset_io_liststore.get_value(treeiter, 0)
		self.w.ioaddr.set_text(value)
	else:
		return
