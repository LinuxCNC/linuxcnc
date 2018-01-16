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

import hashlib
from gi.repository import Gtk
from stepconf import preset
from stepconf.definitions import *

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

"""
def hz(self, axname):
	steprev = getattr(self.d, axname+"steprev")
	microstep = getattr(self.d, axname+"microstep")
	pulleynum = getattr(self.d, axname+"pulleynum")
	pulleyden = getattr(self.d, axname+"pulleyden")
	leadscrew = getattr(self.d, axname+"leadscrew")
	maxvel = getattr(self.d, axname+"maxvel")
	if self.d.units or axname == 'a': leadscrew = 1./leadscrew
	pps = leadscrew * steprev * microstep * (pulleynum/pulleyden) * maxvel
	return abs(pps)
"""

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
def do_exclusive_inputs(self, pin,port):
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

"""
def find_input(self, input):
	inputs = set((10, 11, 12, 13, 15))
	for i in inputs:
		pin = getattr(self.d, "pin%d" % i)
		inv = getattr(self.d, "pin%dinv" % i)
		if pin == input: return i
	return None
"""

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

