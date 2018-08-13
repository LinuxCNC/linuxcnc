#!/usr/bin/env python
# -*- coding: utf-8 -*-
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

#********************
# AXIS GENERAL PAGE
#********************
import sys
import time
import hal
from gi.repository import Gtk
from gi.repository import GObject
from stepconf import preset
from stepconf.definitions import *


#**********
# Axis Test
#***********
def test_axis(self, axis):
	if not self.check_for_rt():
		return

	# Retrive user setting for maxvel and maxacc on current axis tab
	# Test if there is some data saved
	testvel = self.d[axis + "testmaxvel"]
	testacc = self.d[axis + "testmaxacc"]
	# Check if not null and not empty and not string "None"
	if(testvel != None and testvel != "" and testvel != "None"):
		vel = float(testvel)
	else:
		vel = float(self.w[axis + "maxvel"].get_text())

	# Check if not null and not empty and not string "None"
	if(testacc != None and testacc != "" and testacc != "None"):
		acc = float(testacc)
	else:
		acc = float(self.w[axis + "maxacc"].get_text())

	scale = self.d[axis + "scale"]
	maxvel = float(self.w[axis + "maxvel"].get_text()) * 1.5
	#maxvel = 1.5 * vel
	if self.doublestep():
			period = int(1e9 / maxvel / scale)
	else:
			period = int(.5e9 / maxvel / scale)
	
	steptime = self.w.steptime.get_value()
	stepspace = self.w.stepspace.get_value()
	latency = self.w.latency.get_value()
	minperiod = self.minperiod()
	
	if period < minperiod:
		period = minperiod
		if self.doublestep():
			maxvel = 1e9 / minperiod / abs(scale)
		else:
			maxvel = 1e9 / minperiod / abs(scale)
	if period > 100000:
		period = 100000

	# halrun print a point "." on console, but I have not found out why.
	self.halrun = os.popen("halrun -Is", "w")
	if self._p.debug:
		self.halrun.write("echo\n")
	axnum = "xyza".index(axis)
	step = axis + "step"
	dir = axis + "dir"
	
	self.halrun.write("""
		loadrt steptest
		loadrt stepgen step_type=0
		""")

	port3name=port2name=port2dir=port3dir=""
	if self.d.number_pports>2:
		 port3name = ' '+self.d.ioaddr3
		 if self.d.pp3_direction: # Input option
			port3dir =" in"
		 else: 
			port3dir =" out"
	if self.d.number_pports>1:
		 port2name = ' '+self.d.ioaddr2
		 if self.d.pp2_direction: # Input option
			port2dir =" in"
		 else: 
			port2dir =" out"
	self.halrun.write( "loadrt hal_parport cfg=\"%s out%s%s%s%s\"\n" % (self.d.ioaddr, port2name, port2dir, port3name, port3dir))
	self.halrun.write("""
		loadrt threads period1=%(period)d name1=fast fp1=0 period2=1000000 name2=slow
		addf stepgen.make-pulses fast
		addf parport.0.write fast
		"""%{'period': period})
	
	if self.d.number_pports>1:
		self.halrun.write( "addf parport.0.write fast\n")
	if self.d.number_pports>2:
		self.halrun.write( "addf parport.0.write fast\n")
	temp = self.find_output(axis +'step')
	step_pin = temp[0][0]
	temp = self.find_output(axis +'dir')
	dir_pin = temp[0][0]
	self.halrun.write("""
		addf stepgen.capture-position slow
		addf steptest.0 slow
		addf stepgen.update-freq slow

		net step stepgen.0.step => parport.0.pin-%(steppin)02d-out
		net dir stepgen.0.dir => parport.0.pin-%(dirpin)02d-out
		net cmd steptest.0.position-cmd => stepgen.0.position-cmd
		net fb stepgen.0.position-fb => steptest.0.position-fb

		setp parport.0.pin-%(steppin)02d-out-reset 1
		setp stepgen.0.steplen 1
		setp stepgen.0.dirhold %(dirhold)d
		setp stepgen.0.dirsetup %(dirsetup)d
		setp stepgen.0.position-scale %(scale)f
		setp steptest.0.epsilon %(onestep)f

		setp stepgen.0.enable 1
	""" % {
		'steppin': step_pin,
		'dirpin': dir_pin,
		'dirhold': self.d.dirhold + self.d.latency,
		'dirsetup': self.d.dirsetup + self.d.latency,
		'onestep': abs(1. / self.d[axis + "scale"]),
		'scale': self.d[axis + "scale"],
	})
	
	if self.doublestep():
		self.halrun.write("""
			setp parport.0.reset-time %(resettime)d
			setp stepgen.0.stepspace 0
			addf parport.0.reset fast
		""" % {
			'resettime': self.d['steptime']
		})
	amp_signals = self.find_output(d_hal_output[AMP])
	for pin in amp_signals:
		amp,amp_port = pin
		self.halrun.write("setp parport.%(portnum)d.pin-%(enablepin)02d-out 1\n"
			% {'enablepin': amp,'portnum': amp_port})
	
	estop_signals = self.find_output(d_hal_output[ESTOP])
	for pin in estop_signals:
		estop,e_port = pin
		self.halrun.write("setp parport.%(portnum)d.pin-%(estoppin)02d-out 1\n"
			% {'estoppin': estop,'portnum': e_port})
	
	for pin in 1,2,3,4,5,6,7,8,9,14,16,17:
		inv = getattr(self.d, "pin%dinv" % pin)
		if inv:
			self.halrun.write("setp parport.0.pin-%(pin)02d-out-invert 1\n"
				% {'pin': pin})
	if self.d.number_pports > 1:
		if self.d.pp2_direction:# Input option
			out_list =(1,14,16,17)
		else:
			out_list =(1,2,3,4,5,6,7,8,9,14,16,17)
		for pin in (out_list):
			inv = getattr(self.d, "pp2_pin%dinv" % pin)
			if inv:
				self.halrun.write("setp parport.1.pin-%(pin)02d-out-invert 1\n"
				% {'pin': pin})
	if self._p.debug:
		self.halrun.write("loadusr halmeter sig cmd -g 275 415\n")
	
	if axis == "a":
		self.w.testvelunit.set_text(_("deg / s"))
		self.w.testaccunit.set_text(_(u"deg / s²"))
		self.w.testampunit.set_text(_("deg"))
		self.w.testvel.set_increments(1,5)
		self.w.testacc.set_increments(1,5)
		self.w.testamplitude.set_increments(1,5)
		self.w.testvel.set_range(0, maxvel)
		self.w.testacc.set_range(1, 360000)
		self.w.testamplitude.set_range(0, 1440)
		self.w.testvel.set_digits(1)
		self.w.testacc.set_digits(1)
		self.w.testamplitude.set_digits(1)
		self.w.testamplitude.set_value(10)
	elif (self.d.units == MM):
		self.w.testvelunit.set_text(_("mm / s"))
		self.w.testaccunit.set_text(_(u"mm / s²"))
		self.w.testampunit.set_text(_("mm"))
		self.w.testvel.set_increments(1,5)
		self.w.testacc.set_increments(1,5)
		self.w.testamplitude.set_increments(1,5)
		self.w.testvel.set_range(0, maxvel)
		self.w.testacc.set_range(1, 100000)
		self.w.testamplitude.set_range(0, 1000)
		self.w.testvel.set_digits(2)
		self.w.testacc.set_digits(2)
		self.w.testamplitude.set_digits(2)
		self.w.testamplitude.set_value(15)
	else:
		self.w.testvelunit.set_text(_("in / s"))
		self.w.testaccunit.set_text(_(u"in / s²"))
		self.w.testampunit.set_text(_("in"))
		self.w.testvel.set_increments(.1,5)
		self.w.testacc.set_increments(1,5)
		self.w.testamplitude.set_increments(.1,5)
		self.w.testvel.set_range(0, maxvel)
		self.w.testacc.set_range(1, 3600)
		self.w.testamplitude.set_range(0, 36)
		self.w.testvel.set_digits(1)
		self.w.testacc.set_digits(1)
		self.w.testamplitude.set_digits(1)
		self.w.testamplitude.set_value(.5)
	
	self.jogplus = self.jogminus = 0
	self.w.testdir.set_active(0)
	self.w.run.set_active(0)
	self.w.testacc.set_value(acc)
	self.w.testvel.set_value(vel)
	self._p.axis_under_test = axis
	self.update_axis_test()

	self.halrun.write("start\n")
	self.halrun.flush()

	self.w.dlgTestAxis.set_title(_("%s Axis Test") % axis.upper())
	self.w.dlgTestAxis.show_all()
	result = self.w.dlgTestAxis.run()
	if result == Gtk.ResponseType.OK:
		# Save test parameters
		self.d[axis + "testmaxvel"] = self.w.testvel.get_value()
		self.d[axis + "testmaxacc"] = self.w.testacc.get_value()
	self.w.dlgTestAxis.hide()
	
	if amp_signals:
		for pin in amp_signals:
			amp,amp_port = pin
			self.halrun.write("setp parport.%(portnum)d.pin-%(enablepin)02d-out 0\n"
			% {'enablepin': amp,'portnum': amp_port})
	if estop_signals:
		for pin in estop_signals:
			estop,e_port = pin
			self.halrun.write("setp parport.%(portnum)d.pin-%(estoppin)02d-out 0\n"
			% {'estoppin': estop,'portnum': e_port})

	time.sleep(.001)
	self.halrun.close()


# Check for realtime-capable LinuxCNC.
# Returns True if the running version of LinuxCNC is realtime-capable
# (or if debug is enabled), returns False otherwise.
def check_for_rt(self):
	is_realtime_capable = False
	try:
		if hal.is_sim:
			self.warning_dialog(MESS_NO_REALTIME,True)
		elif hal.is_rt:
			if hal.is_kernelspace:
				actual_kernel = os.uname()[2]
				if hal.kernel_version == actual_kernel:
					is_realtime_capable = True
				else:
					self.warning_dialog(MESS_KERNEL_WRONG + '%s'%hal.kernel_version,True)
			else:
				is_realtime_capable = True
	except:
		print 'STEPCONF WARNING: check-for-realtime function failed - continuing anyways.'
		print sys.exc_info()
		return True

	if is_realtime_capable or self._p.debug:
		return True
	else:
		return False

#*********************
# General Axis methods and callbacks
#*********************
def update_axis_params(self, *args):
	self.update_axis_test()

def update_axis_test(self):
	axis = self._p.axis_under_test
	if axis is None:
		return

	self.halrun.write("""
		setp stepgen.0.maxaccel %(accel)f
		setp stepgen.0.maxvel %(vel)f
		setp steptest.0.jog-minus %(jogminus)s
		setp steptest.0.jog-plus %(jogplus)s
		setp steptest.0.run %(run)s
		setp steptest.0.amplitude %(amplitude)f
		setp steptest.0.maxvel %(vel)f
		setp steptest.0.dir %(dir)s
	""" % {
		'jogminus': self._p.jogminus,
		'jogplus': self._p.jogplus,
		'run': self.w.run.get_active(),
		'amplitude': self.w.testamplitude.get_value(),
		'accel': self.w.testacc.get_value(),
		'vel': self.w.testvel.get_value(),
		'dir': self.w.testdir.get_active(),
	})
	self.halrun.flush()


