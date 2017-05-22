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
# pport2 PAGE
#************
from stepconf.definitions import *

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
	self.page_set_state('spindle',(self.a.has_spindle_speed_control() or self.a.has_spindle_encoder()) )

# pport2 callbacks:
def on_pp2_direction_changed(self, widget):
	state = widget.get_active()
	for i in (2,3,4,5,6,7,8,9):
		self.w['pp2_pin%s_in_box'%i].set_visible(state)
		self.w['pp2_pin%s_out_box'%i].set_visible(not state)

def on_exclusive_check_pp2(self, widget):
	self.a.do_exclusive_inputs(widget,2)

