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
import errno
from stepconf.definitions import *

class Private_Data:
	def __init__(self):
		####################################################
		############# Prepare path #########################
		####################################################
		# Program path
		self.program_path = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
		# Config path
		self.config_path = os.path.expanduser("~/linuxcnc/configs")
		try:
			os.makedirs(self.config_path)
		except os.error, detail:
			if detail.errno != errno.EEXIST:
				raise

		self.datadir = os.path.join(self.program_path, "share", "linuxcnc","stepconf")
		self.main_datadir = os.path.join(self.program_path, "share", "linuxcnc")
		self.wizard = os.path.join(self.datadir, "linuxcnc-wizard.gif")
		if not os.path.isfile(self.wizard):
			self.wizard = os.path.join(self.main_datadir, "linuxcnc-wizard.gif")
		if not os.path.isfile(self.wizard):
			print "cannot find linuxcnc-wizard.gif, looked in %s and %s" % (self.datadir, self.main_datadir)
			sys.exit(1)

		self.distdir = os.path.join(self.program_path, "configs", "common")
		if not os.path.isdir(self.distdir):
			self.distdir = os.path.join(self.program_path, "share", "doc", "linuxcnc", "sample-configs", "common")
		if not os.path.isdir(self.distdir):
			self.distdir = os.path.join(self.program_path, "linuxcnc", "sample-configs", "common")
		if not os.path.isdir(self.distdir):
			self.distdir = os.path.join(self.program_path, "share", "doc", "linuxcnc", "examples", "sample-configs", "common")
		if not os.path.isdir(self.distdir):
			self.distdir = "/usr/share/doc/linuxcnc/examples/sample-configs/common"

		self.icondir = self.program_path
		self.linuxcncicon = os.path.join(self.icondir, "linuxcncicon.png")
		if not os.path.isfile(self.linuxcncicon):
			self.linuxcncicon = os.path.join("/etc/linuxcnc/linuxcnc-wizard.gif")
		if not os.path.isfile(self.linuxcncicon):
			self.linuxcncicon = os.path.join("/usr/share/linuxcnc/linuxcncicon.png")
		if not os.path.isfile(self.linuxcncicon):
			print "cannot find icon"
			sys.exit(1)

		self.debug = False
		self.in_pport_prepare = True
		self.recursive_block = False
		# For axis test
		self.jogminus = 0
		self.jogplus = 0
		self.axis_under_test = None
		
	# Boiler code
	def __getitem__(self, item):
		return getattr(self, item)
	def __setitem__(self, item, value):
		return setattr(self, item, value)

class Data:
	def __init__(self):
		#pw = pwd.getpwuid(os.getuid())
		self.createsymlink = True
		self.createshortcut = True
		self.sim_hardware = True
		self.sim_hardware = False
		self._lastconfigname= ""
		self._chooselastconfig = True
		self._preference_version = 1.0
	
		self.md5sums = []
	
		self.machinename = _("my-mill")
		self.axes = 0 # XYZ
		self.units = MM # mm
		self.drivertype = "Other"
		self.steptime = 5000
		self.stepspace = 5000
		self.dirhold = 20000 
		self.dirsetup = 20000
		self.latency = 15000
		self.period = 25000
		
		self.global_preset = 0
		self.lparport = None
		self.ioaddr = "0"
		self.ioaddr2 = "1"
		self.pp2_direction = 0 # output
		self.ioaddr3 = "2"
		self.pp3_direction = 0 # output
		self.number_pports = 1
	
		self.halui = 0
		self.halui_list = []
		self.halui_custom = 0
		self.halui_list_custom = []
	
		self.hal_postgui = 0
		self.hal_postgui_list = []
		self.hal_postgui_custom = 0
		self.hal_postgui_list_custom = []
		self.hal_guioption_list = []
	
		self.manualtoolchange = 1
		self.customhal = 1 # include custom hal file
	
		# HALUI MDI_COMMAND
		self.halui_mdi_x_null = -1
		self.halui_mdi_x_zero = -1
		self.halui_mdi_y_null = -1
		self.halui_mdi_y_zero = -1
		self.halui_mdi_z_null = -1
		self.halui_mdi_z_zero = -1
		self.halui_mdi_a_null = -1
		self.halui_mdi_a_zero = -1
		self.halui_mdi_u_null = -1
		self.halui_mdi_u_zero = -1
		self.halui_mdi_v_null = -1
		self.halui_mdi_v_zero = -1
		self.halui_mdi_set_position = -1
		self.halui_mdi_goto_position = -1
		self.halui_probe_tool_lenght = -1
	
		# gui
		self.mix_gladevcp_pyvcp = False
		self.maxor2 = 0
		# pyvcp data
		self.pyvcp = True # default include
		self.pyvcptype = PYVCP_DEFAULT # include default pyvcp gui
		self.pyvcpname = "blank.xml"
		self.pyvcphaltype = 0 # no HAL connections specified
		
		# gladevcp data
		self.gladevcp = False # not included
		self.gladevcptype = GLADEVCP_DEFAULT # include default gladevcp gui
		self.gladesample = True
		self.gladeexists = False
		self.spindlespeedbar = True
		self.spindleatspeed = True
		self.maxspeeddisplay = 1000
		self.zerox = False
		self.zeroy = False
		self.zeroz = False
		self.zeroa = False
		self.autotouchz = False
		self.gladevcphaluicmds = 0 # not used
		self.centerembededgvcp = False
		self.sideembededgvcp = True
		self.gladevcpname = "blank.ui"
	
		# Tool change
		self.manual_tool_change = False
		
		# Position of probe switch
		self.probe_x_pos = 10
		self.probe_y_pos = 10
		self.probe_z_pos = 100
		self.probe_sensor_height = 40.0 # mm
	
		self.classicladder = 0 # not included
		self.tempexists = 0 # not present
		self.laddername = "custom.clp"
		self.modbus = 0
		self.ladderhaltype = 0 # no HAL connections specified
		self.ladderconnect = 1 # HAL connections allowed
	
		self.select_axis = True
		self.select_gmoccapy = False
	
		self.pin1inv = False
		self.pin2inv = False
		self.pin3inv = False
		self.pin4inv = False
		self.pin5inv = False
		self.pin6inv = False
		self.pin7inv = False
		self.pin8inv = False
		self.pin9inv = False
		self.pin10inv = False
		self.pin11inv = False
		self.pin12inv = False
		self.pin13inv = False
		self.pin14inv = False
		self.pin15inv = False
		self.pin16inv = False
		self.pin17inv = False
	
		self.pin1 = d_hal_output[ESTOP]
		self.pin2 = d_hal_output[XSTEP]
		self.pin3 = d_hal_output[XDIR]
		self.pin4 = d_hal_output[YSTEP]
		self.pin5 = d_hal_output[YDIR]
		self.pin6 = d_hal_output[ZSTEP]
		self.pin7 = d_hal_output[ZDIR]
		self.pin8 = d_hal_output[ASTEP]
		self.pin9 = d_hal_output[ADIR]
		self.pin14 = d_hal_output[CW]
		self.pin16 = d_hal_output[PWM]
		self.pin17 = d_hal_output[AMP]
	
		self.pin10 = d_hal_input[UNUSED_INPUT]
		self.pin11 = d_hal_input[UNUSED_INPUT]
		self.pin12 = d_hal_input[UNUSED_INPUT]
		self.pin13 = d_hal_input[UNUSED_INPUT]
		self.pin15 = d_hal_input[UNUSED_INPUT]
		# pp1 preset
		self.pport1_preset = 0
	
		#   port 2
		for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
			p = 'pp2_pin%d' % pin
			self[p] = d_hal_output[UNUSED_OUTPUT]
			p = 'pp2_pin%dinv' % pin
			self[p] = 0
		for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
			p = 'pp2_pin%d_in' % pin
			self[p] = d_hal_input[UNUSED_INPUT]
			p = 'pp2_pin%d_in_inv' % pin
			self[p] = 0
	
		for i in ('x','y','z','a', 'u','v'):
			 self[i+'steprev'] = 200
			 self[i+'microstep'] = 2
			 self[i+'pulleynum'] = 1
			 self[i+'pulleyden'] = 1
			 self[i+'leadscrew'] = 20
			 self[i+'maxvel'] = 0
			 self[i+'maxacc'] = 0
	
			 self[i+'homepos'] = 0
			 self[i+'minlim'] =  0
			 self[i+'maxlim'] =  0
			 self[i+'homesw'] =  0
			 self[i+'homevel'] = 0
			 self[i+'latchdir'] = 0
			 self[i+'scale'] = 0
	
			 # Varibles for test axis
			 self[i+'testmaxvel'] = None
			 self[i+'testmaxacc'] = None
			 # Preset
			 self[i+'preset'] = 0

		self.asteprev = 200
		self.amicrostep = 2
		self.apulleynum = 1
		self.apulleyden = 1
		self.aleadscrew = 360
		self.amaxvel = 360
		self.amaxacc = 1200
	
		self.ahomepos = 0
		self.aminlim = -9999
		self.amaxlim =  9999
		self.ahomesw =  0
		self.ahomevel = .05
		self.alatchdir = 0
		self.ascale = 0
	
		self.spindlecarrier = 100
		self.spindlecpr = 100
		self.spindlespeed1 = 100
		self.spindlespeed2 = 800
		self.spindlepwm1 = .2
		self.spindlepwm2 = .8
		self.spindlefiltergain = .01
		self.spindlenearscale = 1.5
		self.usespindleatspeed = False
	
		self.digitsin = 15
		self.digitsout = 15
		self.s32in = 10
		self.s32out = 10
		self.floatsin = 10
		self.floatsout = 10
		self.createsymlink = 1
		self.createshortcut = 1

	# Boiler code
	def __getitem__(self, item):
		return getattr(self, item)
	def __setitem__(self, item, value):
		return setattr(self, item, value)
