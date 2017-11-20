#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#    This is stepconf, a graphical configuration editor for LinuxCNC
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
# GUI PAGE
#************
import os
from stepconf.definitions import *

def gui_page_prepare(self):
	self.w.gui_select_axis.set_active(self.d.select_axis)
	self.w.gui_select_gmoccapy.set_active(self.d.select_gmoccapy)
	self.w.ladderconnect.set_active(self.d.ladderconnect)
	
	# Gladevcp
	self.w.gui_gladevcp.set_active(self.d.gladevcp)
	self.w.centerembededgvcp.set_active(self.d.centerembededgvcp)
	self.w.sideembededgvcp.set_active(self.d.sideembededgvcp)
	if(self.d.gladevcptype == GLADEVCP_DEFAULT):
		self.w.gui_rdo_default_gladevcp.set_active(True)
	elif(self.d.gladevcptype == GLADEVCP_CUSTOM):
		self.w.gui_rdo_custom_gladevcp.set_active(True)
	elif(self.d.gladevcptype == GLADEVCP_NONE):
		self.w.gui_rdo_none_gladevcp.set_active(True)
	
	# Pyvcp
	self.w.gui_pyvcp.set_active(self.d.pyvcp)
	if(self.d.pyvcptype == PYVCP_DEFAULT):
		self.w.gui_rdo_default_pyvcp.set_active(True)
	elif(self.d.pyvcptype == PYVCP_CUSTOM):
		self.w.gui_rdo_custom_pyvcp.set_active(True)
	elif(self.d.pyvcptype == PYVCP_NONE):
		self.w.gui_rdo_none_pyvcp.set_active(True)

	#self.on_gui_pyvcp_toggled()

	# Get max speed (speed / minutes)
	if self.d.axes == 0: maxspeed = max(self.d.xmaxvel, self.d.ymaxvel, self.d.zmaxvel)* 60 # X Y Z
	elif self.d.axes == 1: maxspeed = max(self.d.xmaxvel, self.d.ymaxvel, self.d.zmaxvel)* 60 # X Y Z A (A is too fast, excluded)
	elif self.d.axes == 2: maxspeed = max(self.d.xmaxvel, self.d.zmaxvel)* 60 # X Z
	elif self.d.axes == 3: maxspeed = max(self.d.xmaxvel, self.d.ymaxvel, self.d.umaxvel, self.d.vmaxvel)* 60 # X Y U V
	self.general_maxspeed = maxspeed

def gui_page_finish(self):
	self.d.select_axis = self.w.gui_select_axis.get_active()
	self.d.select_gmoccapy = self.w.gui_select_gmoccapy.get_active()
	self.d.pyvcp = self.w.gui_pyvcp.get_active()
	self.d.gladevcp = self.w.gui_gladevcp.get_active()

	# Gladevcp
	if self.d.gladevcp == True:
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
	if self.d.pyvcp == True:
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

	# Halui connections
	if(self.d.pyvcp or self.d.gladevcp):
		# Check if at least one default panel is selected
		if((self.d.pyvcptype == PYVCP_DEFAULT) or (self.d.gladevcptype == GLADEVCP_DEFAULT)): # default panel
			# MDI_COMMAND
			self.create_halui_mdi()
			self.d.halui = 1 # enable writing mdi_commands

	
	# Check if we have both gladevcp and pyvcp
	if (self.d.gladevcp == True and self.d.gladevcptype == GLADEVCP_DEFAULT) and (self.d.pyvcp == True and self.d.pyvcptype == PYVCP_DEFAULT):
		self.d.mix_gladevcp_pyvcp = True
		# Prepare hal components like or2, mux2, logic, etc.
		self.prepare_hal_components_for_merge()
		# Create hal files for merge
		self.create_gladevcp_hal_merge() # gladevcp
		self.create_pyvcp_hal_merge() # pyvcp
		self.create_guimerge_hal() # for merging pyvcp and gladevcp
	else:
		self.d.mix_gladevcp_pyvcp = False
		# Prepare hal components like or2, mux2, logic, etc.
		self.prepare_hal_components_for_single()
		# Single hal files
		if(self.d.gladevcp == True and self.d.gladevcptype == PYVCP_DEFAULT):
			self.create_gladevcp_hal_single() # gladevcp
		if(self.d.pyvcp == True and self.d.pyvcptype == PYVCP_DEFAULT):
			self.create_pyvcp_hal_single() # pyvcp

	self.d.centerembededgvcp = self.w.centerembededgvcp.get_active()
	self.d.sideembededgvcp = self.w.sideembededgvcp.get_active()   

def create_halui_mdi(self):
	# X
	self._p.halui_list.append(MDI_G54X0)
	self._p.halui_mdi_x_null = len(self._p.halui_list) -1
	self._p.halui_list.append("G0 x0 F1000")
	self._p.halui_mdi_x_zero = len(self._p.halui_list) -1

	# Y
	if self.d.axes in(0, 1, 3):
		self._p.halui_list.append(MDI_G54Y0)
		self._p.halui_mdi_y_null = len(self._p.halui_list) -1
		self._p.halui_list.append("G0 y0 F1000")
		self._p.halui_mdi_y_zero = len(self._p.halui_list) -1
	# Z
	if self.d.axes in(0, 1, 2):
		self._p.halui_list.append(MDI_G54Z0)
		self._p.halui_mdi_z_null = len(self._p.halui_list) -1
		self._p.halui_list.append("G0 z0 F400")
		self._p.halui_mdi_z_zero = len(self._p.halui_list) -1
	# A
	if self.d.axes == 1:
		self._p.halui_list.append(MDI_G54A0)
		self._p.halui_mdi_a_null = len(self._p.halui_list) -1
		self._p.halui_list.append("G0 a0 F1000")
		self._p.halui_mdi_a_zero = len(self._p.halui_list) -1
	# UV
	if self.d.axes == 3:
		self._p.halui_list.append(MDI_G54U0)
		self._p.halui_mdi_u_null = len(self._p.halui_list) -1
		self._p.halui_list.append("G0 u0 F1000")
		self._p.halui_mdi_u_zero = len(self._p.halui_list) -1

		self._p.halui_list.append(MDI_G54V0)
		self._p.halui_mdi_v_null = len(self._p.halui_list) -1
		self._p.halui_list.append("G0 v0 F1000")
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


def prepare_hal_components_for_merge(self):
	#####################
	### OR2
	OR = self._p.maxor2
	# X
	self._p.or2_jog_x_minus = OR
	OR += 1
	self._p.or2_jog_x_plus = OR
	OR += 1
	self._p.or2_zero_x = OR
	OR += 1
	self._p.or2_go_zero_x = OR
	OR += 1
	
	# Y
	if self.d.axes in(0, 1, 3):
		self._p.or2_jog_y_minus = OR
		OR += 1
		self._p.or2_jog_y_plus = OR
		OR += 1
		self._p.or2_zero_y = OR
		OR += 1
		self._p.or2_go_zero_y = OR
		OR += 1
	# Z
	if self.d.axes in(0, 1, 2):
		self._p.or2_jog_z_minus = OR
		OR += 1
		self._p.or2_jog_z_plus = OR
		OR += 1
		self._p.or2_zero_z = OR
		OR += 1
		self._p.or2_go_zero_z = OR
		OR += 1
	# A
	if self.d.axes == 1:
		self._p.or2_jog_a_minus = OR
		OR += 1
		self._p.or2_jog_a_plus = OR
		OR += 1
		self._p.or2_zero_a = OR
		OR += 1
		self._p.or2_go_zero_a = OR
		OR += 1
	# UV
	if self.d.axes == 3:
		self._p.or2_jog_u_minus = OR
		OR += 1
		self._p.or2_jog_u_plus = OR
		OR += 1
		self._p.or2_zero_u = OR
		OR += 1
		self._p.or2_go_zero_u = OR
		OR += 1

		self._p.or2_jog_v_minus = OR
		OR += 1
		self._p.or2_jog_v_plus = OR
		OR += 1
		self._p.or2_zero_v = OR
		OR += 1
		self._p.or2_go_zero_v = OR
		OR += 1

	# HOME ALL
	self._p.or2_home_all = OR
	OR += 1

	# E-STOP
	self._p.or2_estop = OR
	OR += 1

	# JOG

	# Save position
	self._p.or2_set_position = OR
	OR += 1
	self._p.or2_go_to_position = OR
	OR += 1

	# TOOL LENGHT SENSOR
	inputs = self.build_input_set()
	if (d_hal_input[PROBE] in inputs):
		self._p.or2_probe = OR
		OR += 1

	# or2 start from 0, so I don't remove last OR += 1
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
	### MUX2
	MUX2 = self._p.maxmux2
	# E-STOP
	self._p.mux2_jog = MUX2
	MUX2 += 1
	# SPINDLE SPEED
	self._p.mux2_spindle_speed = MUX2
	MUX2 += 1
	self._p.maxmux2 = MUX2

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
	### LOGIC
	LOGIC = self._p.maxlogic
	# JOG
	self._p.logic_jog = LOGIC
	LOGIC += 1

	# logic.x.in for jog
	LOGICIN = 0
	# E-STOP
	self._p.logic_jog_x_minus = LOGICIN
	LOGICIN += 1
	self._p.logic_jog_x_plus = LOGICIN
	LOGICIN += 1

	# Y
	if self.d.axes in(0, 1, 3):
		self._p.logic_jog_y_minus = LOGICIN
		LOGICIN += 1
		self._p.logic_jog_y_plus = LOGICIN
		LOGICIN += 1

	# Z
	if self.d.axes in(0, 1, 2):
		self._p.logic_jog_z_minus = LOGICIN
		LOGICIN += 1
		self._p.logic_jog_z_plus = LOGICIN
		LOGICIN += 1

	# A
	if self.d.axes == 1:
		self._p.logic_jog_a_minus = LOGICIN
		LOGICIN += 1
		self._p.logic_jog_a_plus = LOGICIN
		LOGICIN += 1

	# UV
	if self.d.axes == 3:
		self._p.logic_jog_u_minus = LOGICIN
		LOGICIN += 1
		self._p.logic_jog_u_plus = LOGICIN
		LOGICIN += 1

		self._p.logic_jog_v_minus = LOGICIN
		LOGICIN += 1
		self._p.logic_jog_v_plus = LOGICIN
		LOGICIN += 1
		
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

def create_guimerge_hal(self):
	self._p.hal_guimerge_list = []

	# X
	self._p.hal_guimerge_list.append("# X AXES")
	self._p.hal_guimerge_list.append("net jogxminus <= or2.%d.out => halui.axis.x.minus" % (self._p.or2_jog_x_minus))
	self._p.hal_guimerge_list.append("net jogxplus <= or2.%d.out => halui.axis.x.plus" % (self._p.or2_jog_x_plus))
	self._p.hal_guimerge_list.append("net x-null <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_zero_x, self._p.halui_mdi_x_null) )
	self._p.hal_guimerge_list.append("net x-zero <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_go_zero_x, self._p.halui_mdi_x_zero) )

	# Y
	if self.d.axes in(0, 1, 3):
		self._p.hal_guimerge_list.append("# Y AXES")
		self._p.hal_guimerge_list.append("net jogyminus <= or2.%d.out => halui.axis.y.minus" % (self._p.or2_jog_y_minus))
		self._p.hal_guimerge_list.append("net jogyplus <= or2.%d.out => halui.axis.y.plus" % (self._p.or2_jog_y_plus))
		self._p.hal_guimerge_list.append("net y-null <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_zero_y , self._p.halui_mdi_y_null) )
		self._p.hal_guimerge_list.append("net y-zero <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_go_zero_y, self._p.halui_mdi_y_zero) )
	# Z
	if self.d.axes in(0, 1, 2):
		self._p.hal_guimerge_list.append("# Z AXES")
		self._p.hal_guimerge_list.append("net jogzminus <= or2.%d.out => halui.axis.z.minus" % (self._p.or2_jog_z_minus))
		self._p.hal_guimerge_list.append("net jogzplus <= or2.%d.out => halui.axis.z.plus" % (self._p.or2_jog_z_plus))
		self._p.hal_guimerge_list.append("net z-null <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_zero_z, self._p.halui_mdi_z_null) )
		self._p.hal_guimerge_list.append("net z-zero <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_go_zero_z, self._p.halui_mdi_z_zero) )
	# A
	if self.d.axes == 1:
		self._p.hal_guimerge_list.append("# A AXES")
		self._p.hal_guimerge_list.append("net jogaminus <= or2.%d.out => halui.axis.a.minus" % (self._p.or2_jog_a_minus))
		self._p.hal_guimerge_list.append("net jogaplus <= or2.%d.out => halui.axis.a.plus" % (self._p.or2_jog_a_plus))
		self._p.hal_guimerge_list.append("net a-null <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_zero_a , self._p.halui_mdi_a_null) )
		self._p.hal_guimerge_list.append("net a-zero <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_go_zero_a, self._p.halui_mdi_a_zero) )
	# UV
	if self.d.axes == 3:
		self._p.hal_guimerge_list.append("# U AXES")
		self._p.hal_guimerge_list.append("net joguminus <= or2.%d.out => halui.axis.u.minus" % (self._p.or2_jog_u_minus))
		self._p.hal_guimerge_list.append("net joguplus <= or2.%d.out => halui.axis.u.plus" % (self._p.or2_jog_u_plus))
		self._p.hal_guimerge_list.append("net u-null <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_zero_u, self._p.halui_mdi_u_null) )
		self._p.hal_guimerge_list.append("net u-zero <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_go_zero_u, self._p.halui_mdi_u_zero) )

		self._p.hal_guimerge_list.append("# V AXES")
		self._p.hal_guimerge_list.append("net jogvminus <= or2.%d.out => halui.axis.v.minus" % (self._p.or2_jog_v_minus))
		self._p.hal_guimerge_list.append("net jogvplus <= or2.%d.out => halui.axis.v.plus" % (self._p.or2_jog_v_plus))
		self._p.hal_guimerge_list.append("net v-null <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_zero_v, self._p.halui_mdi_v_null) )
		self._p.hal_guimerge_list.append("net v-zero <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_go_zero_v, self._p.halui_mdi_v_zero) )

	# JOG
	self._p.hal_guimerge_list.append("# connect the PyVCP jog speed slider")
	self._p.hal_guimerge_list.append("net jogspeed <= mux2.0.out => halui.axis.jog-speed")

	# select between normal speed and orient speed
	# based on ori-enable (M119) command
	#net ori-enable => mux2.0.sel
	# output of mux is desired spindle RPM
	#net spindle-rpm-out mux2.0.out => scale.0.in
	#net spindle-speed-DAC scale.0.out => hm2_[HOSTMOT2](BOARD).0.pwmgen.03.value

	# HOME ALL
	self._p.hal_guimerge_list.append("# HOME-ALL")
	self._p.hal_guimerge_list.append("net home-all <= or2.%d.out => halui.home-all" % (self._p.or2_home_all))

	#POWER on/off
	self._p.hal_guimerge_list.append("# E-STOP")
	self._p.hal_guimerge_list.append("net power-btn or2.%d.out => and2.%d.in0" % (self._p.or2_estop, AND))
	self._p.hal_guimerge_list.append("net power-ok and2.%d.in1 not.0.out" % (AND))
	self._p.hal_guimerge_list.append("net power-not not.0.in <= halui.estop.is-activated")
	self._p.hal_guimerge_list.append("net power-request toggle.0.in <= and2.%d.out" % (AND))
	self._p.hal_guimerge_list.append("net power-toggle toggle2nist.0.in <= toggle.0.out")
	self._p.hal_guimerge_list.append("net power-is-on toggle2nist.0.is-on <= halui.machine.is-on")
	self._p.hal_guimerge_list.append("net power-on halui.machine.on <= toggle2nist.0.on")
	self._p.hal_guimerge_list.append("net power-off halui.machine.off <= toggle2nist.0.off")

	# SET/GOTO POSITION
	self._p.hal_guimerge_list.append("# SET GOTO POSITION")
	self._p.hal_guimerge_list.append("net set-position <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_set_position, self._p.halui_mdi_set_position) )
	self._p.hal_guimerge_list.append("net goto-position <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_go_to_position, self._p.halui_mdi_goto_position) )

	# TOOL LENGHT SENSOR
	self._p.hal_guimerge_list.append("# TOOL LENGHT SENSOR")
	inputs = self.build_input_set()
	if (d_hal_input[PROBE] in inputs):
		self._p.hal_guimerge_list.append("net probe <= or2.%d.out => halui.mdi-command-%02d" % (self._p.or2_probe, self.d.halui_probe_tool_lenght) )
	
	"""
	# Spindle speed
	if (self._p.has_spindle_speed_control or self._p.has_spindle_encoder):
		inputs = self.build_input_set()
		encoder = d_hal_input[PHA] in inputs
		if encoder:
			self._p.hal_guimerge_list.append("# **** Setup of spindle speed display using pyvcp -START ****")
			self._p.hal_guimerge_list.append("# **** Use ACTUAL spindle velocity from spindle encoder")
			self._p.hal_guimerge_list.append("# **** spindle-velocity-feedback-rps bounces around so we filter it with lowpass")
			self._p.hal_guimerge_list.append("# **** spindle-velocity-feedback-rps is signed so we use absolute component to remove sign") 
			self._p.hal_guimerge_list.append("# **** ACTUAL velocity is in RPS not RPM so we scale it.")
			self._p.hal_guimerge_list.append("")
			self._p.hal_guimerge_list.append("setp scale.0.gain 60")
			self._p.hal_guimerge_list.append("setp lowpass.0.gain %f")% self.d.spindlefiltergain
			self._p.hal_guimerge_list.append("net spindle-velocity-feedback-rps               => lowpass.0.in")
			self._p.hal_guimerge_list.append("net spindle-fb-filtered-rps      lowpass.0.out  => abs.0.in")
			self._p.hal_guimerge_list.append("net spindle-fb-filtered-abs-rps  abs.0.out      => scale.0.in")
			self._p.hal_guimerge_list.append("net spindle-fb-filtered-abs-rpm  scale.0.out    => pyvcp.spindle-speed")
			self._p.hal_guimerge_list.append("")
			self._p.hal_guimerge_list.append("# **** set up spindle at speed indicator ****")
			if self.d.usespindleatspeed:
				self._p.hal_guimerge_list.append("")
				self._p.hal_guimerge_list.append("net spindle-cmd-rps-abs             =>  near.0.in1")
				self._p.hal_guimerge_list.append("net spindle-velocity-feedback-rps   =>  near.0.in2")
				self._p.hal_guimerge_list.append("net spindle-at-speed                <=  near.0.out")
				self._p.hal_guimerge_list.append("setp near.0.scale %f")% self.d.spindlenearscale
			else:
				self._p.hal_guimerge_list.append("# **** force spindle at speed indicator true because we chose no feedback ****")
				self._p.hal_guimerge_list.append("")
				self._p.hal_guimerge_list.append("sets spindle-at-speed true")
			self._p.hal_guimerge_list.append("net spindle-at-speed       => pyvcp.spindle-at-speed-led")
		else:
			self._p.hal_guimerge_list.append("# **** Use COMMANDED spindle velocity from LinuxCNC because no spindle encoder was specified")
			self._p.hal_guimerge_list.append("")
			self._p.hal_guimerge_list.append("net spindle-cmd-rpm-abs    => pyvcp.spindle-speed")
			self._p.hal_guimerge_list.append("")
			self._p.hal_guimerge_list.append("# **** force spindle at speed indicator true because we have no feedback ****")
			self._p.hal_guimerge_list.append("")
			self._p.hal_guimerge_list.append("net spindle-at-speed => pyvcp.spindle-at-speed-led")
			self._p.hal_guimerge_list.append("sets spindle-at-speed true")

	"""


def on_gui_gladevcp_toggled(self,*args):
	i= self.w.gui_gladevcp.get_active()
	self.w.gui_rdo_default_gladevcp.set_sensitive(i)
	self.w.gui_rdo_custom_gladevcp.set_sensitive(i)
	self.w.gui_rdo_none_gladevcp.set_sensitive(i)
	self.w.gui_gladevcp_box.set_sensitive(i) 

def on_gui_pyvcp_toggled(self,*args):
	i= self.w.gui_pyvcp.get_active()
	self.w.gui_rdo_default_pyvcp.set_sensitive(i)
	self.w.gui_rdo_custom_pyvcp.set_sensitive(i)
	self.w.gui_rdo_none_pyvcp.set_sensitive(i)
	self.w.gui_pyvcp_box.set_sensitive(i)

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
