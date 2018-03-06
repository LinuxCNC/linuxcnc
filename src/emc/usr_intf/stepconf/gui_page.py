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
