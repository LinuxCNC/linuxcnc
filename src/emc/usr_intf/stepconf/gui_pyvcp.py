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
# GUI PYVCP
#************
import os
from stepconf.definitions import *

def create_pyvcp_hal_single(self):
	self._p.hal_postgui_list = []
	
	# net xStep stepgen.0.out => parport.0.pin-02-out parport.0.pin-08-out
	# X
	self._p.hal_postgui_list.append("# connect the X PyVCP buttons")
	self._p.hal_postgui_list.append("net my-jogxminus halui.axis.x.minus <= pyvcp.x-minus")
	self._p.hal_postgui_list.append("net my-jogxplus halui.axis.x.plus <= pyvcp.x-plus")
	self._p.hal_postgui_list.append("net my-x-null pyvcp.x-null => halui.mdi-command-%02d" % (self._p.halui_mdi_x_null) )
	self._p.hal_postgui_list.append("net my-x-zero pyvcp.x-zero => halui.mdi-command-%02d" % (self._p.halui_mdi_x_zero) )
	
	# Y
	if self.d.axes in(0, 1, 3):
		self._p.hal_postgui_list.append("# connect the Y PyVCP buttons")
		self._p.hal_postgui_list.append("net my-jogyminus halui.axis.y.minus <= pyvcp.y-minus")
		self._p.hal_postgui_list.append("net my-jogyplus halui.axis.y.plus <= pyvcp.y-plus")
		self._p.hal_postgui_list.append("net my-y-null pyvcp.y-null => halui.mdi-command-%02d" % (self._p.halui_mdi_y_null) )
		self._p.hal_postgui_list.append("net my-y-zero pyvcp.y-zero => halui.mdi-command-%02d" % (self._p.halui_mdi_y_zero) )

	# Z
	if self.d.axes in(0, 1, 2):
		self._p.hal_postgui_list.append("# connect the Z PyVCP buttons")
		self._p.hal_postgui_list.append("net my-jogzminus halui.axis.z.minus <= pyvcp.z-minus")
		self._p.hal_postgui_list.append("net my-jogzplus halui.axis.z.plus <= pyvcp.z-plus")
		self._p.hal_postgui_list.append("net my-z-null pyvcp.z-null => halui.mdi-command-%02d" % (self._p.halui_mdi_z_null) )
		self._p.hal_postgui_list.append("net my-z-zero pyvcp.z-zero => halui.mdi-command-%02d" % (self._p.halui_mdi_z_zero) )

	# A
	if self.d.axes == 1:
		self._p.hal_postgui_list.append("# connect the A PyVCP buttons")
		self._p.hal_postgui_list.append("net my-jogaminus halui.axis.a.minus <= pyvcp.a-minus")
		self._p.hal_postgui_list.append("net my-jogaplus halui.axis.a.plus <= pyvcp.a-plus")
		self._p.hal_postgui_list.append("net my-a-null pyvcp.a-null => halui.mdi-command-%02d" % (self._p.halui_mdi_a_null) )
		self._p.hal_postgui_list.append("net my-a-zero pyvcp.a-zero => halui.mdi-command-%02d" % (self._p.halui_mdi_a_zero) )
	# UV
	if self.d.axes == 3:
		self._p.hal_postgui_list.append("# connect the U PyVCP buttons")
		self._p.hal_postgui_list.append("net my-joguminus halui.axis.u.minus <= pyvcp.u-minus")
		self._p.hal_postgui_list.append("net my-joguplus halui.axis.u.plus <= pyvcp.u-plus")
		self._p.hal_postgui_list.append("net my-u-null pyvcp.u-null => halui.mdi-command-%02d" % (self._p.halui_mdi_u_null) )
		self._p.hal_postgui_list.append("net my-u-zero pyvcp.u-zero => halui.mdi-command-%02d" % (self._p.halui_mdi_u_zero) )

		self._p.hal_postgui_list.append("# connect the V PyVCP buttons")
		self._p.hal_postgui_list.append("net my-jogvminus halui.axis.v.minus <= pyvcp.v-minus")
		self._p.hal_postgui_list.append("net my-jogvplus halui.axis.v.plus <= pyvcp.v-plus")
		self._p.hal_postgui_list.append("net my-v-null pyvcp.v-null => halui.mdi-command-%02d" % (self._p.halui_mdi_v_null) )
		self._p.hal_postgui_list.append("net my-v-zero pyvcp.v-zero => halui.mdi-command-%02d" % (self._p.halui_mdi_v_zero) )

	# HOME ALL
	self._p.hal_postgui_list.append("# HOME-ALL")
	self._p.hal_postgui_list.append("net my-home-all halui.home-all <= pyvcp.home-all")

	# E-STOP
	self._p.hal_postgui_list.append("# ESTOP")
	self._p.hal_postgui_list.append("net power-btn and2.%d.in0 <= pyvcp.estop" % (self._p.and2_estop) )
	self._p.hal_postgui_list.append("net power-ok and2.%d.in1 not.%d.out" % (self._p.and2_estop, self._p.not_estop) )
	self._p.hal_postgui_list.append("net power-not not.%d.in <= halui.estop.is-activated" % (self._p.not_estop) )
	self._p.hal_postgui_list.append("net power-request toggle.%d.in <= and2.%d.out" % (self._p.toggle_estop, self._p.and2_estop) )
	self._p.hal_postgui_list.append("net power-toggle toggle2nist.%d.in <= toggle.%d.out" % (self._p.toggle2nist_estop, self._p.toggle_estop) )
	self._p.hal_postgui_list.append("net power-is-on toggle2nist.%d.is-on <= halui.machine.is-on" % (self._p.toggle2nist_estop) )
	self._p.hal_postgui_list.append("net power-on halui.machine.on <= toggle2nist.%d.on" % (self._p.toggle2nist_estop) )
	self._p.hal_postgui_list.append("net power-off halui.machine.off <= toggle2nist.%d.off" % (self._p.toggle2nist_estop) )
	self._p.hal_postgui_list.append("")

	# JOG
	self._p.hal_postgui_list.append("# connect the PyVCP jog speed slider")
	self._p.hal_postgui_list.append("net my-jogspeed halui.axis.jog-speed <= pyvcp.jog-speed-f")

	# Spindle speed
	if (self._p.has_spindle_speed_control or self._p.has_spindle_encoder):
		inputs = self.build_input_set()
		encoder = d_hal_input[PHA] in inputs
		if encoder:
			self._p.hal_postgui_list.append("# **** Setup of spindle speed display using pyvcp -START ****")
			self._p.hal_postgui_list.append("# **** Use ACTUAL spindle velocity from spindle encoder")
			self._p.hal_postgui_list.append("# **** spindle-velocity-feedback-rps bounces around so we filter it with lowpass")
			self._p.hal_postgui_list.append("# **** spindle-velocity-feedback-rps is signed so we use absolute component to remove sign") 
			self._p.hal_postgui_list.append("# **** ACTUAL velocity is in RPS not RPM so we scale it.")
			self._p.hal_postgui_list.append("")
			self._p.hal_postgui_list.append("setp scale.0.gain 60")
			self._p.hal_postgui_list.append("setp lowpass.0.gain %f")% self.d.spindlefiltergain
			self._p.hal_postgui_list.append("net spindle-velocity-feedback-rps               => lowpass.0.in")
			self._p.hal_postgui_list.append("net spindle-fb-filtered-rps      lowpass.0.out  => abs.0.in")
			self._p.hal_postgui_list.append("net spindle-fb-filtered-abs-rps  abs.0.out      => scale.0.in")
			self._p.hal_postgui_list.append("net spindle-fb-filtered-abs-rpm  scale.0.out    => pyvcp.spindle-speed")
			self._p.hal_postgui_list.append("")
			self._p.hal_postgui_list.append("# **** set up spindle at speed indicator ****")
			if self.d.usespindleatspeed:
				self.d.hal_gvcp_list.append("")
				self.d.hal_gvcp_list.append("net spindle-cmd-rps-abs             =>  near.%d.in1" % (self._p.near_spindle) )
				self.d.hal_gvcp_list.append("net spindle-velocity-feedback-rps   =>  near.%d.in2" % (self._p.near_spindle) )
				self.d.hal_gvcp_list.append("net spindle-at-speed                <=  near.%d.out" % (self._p.near_spindle) )
				self.d.hal_gvcp_list.append("setp near.%d.scale %f" % (self._p.near_spindle, self.spindlenearscale) )
			else:
				self._p.hal_postgui_list.append("# **** force spindle at speed indicator true because we chose no feedback ****")
				self._p.hal_postgui_list.append("")
				self._p.hal_postgui_list.append("sets spindle-at-speed true")
			self._p.hal_postgui_list.append("net spindle-at-speed       => pyvcp.spindle-at-speed-led")
		else:
			self._p.hal_postgui_list.append("# **** Use COMMANDED spindle velocity from LinuxCNC because no spindle encoder was specified")
			self._p.hal_postgui_list.append("")
			self._p.hal_postgui_list.append("net spindle-cmd-rpm-abs    => pyvcp.spindle-speed")
			self._p.hal_postgui_list.append("")
			self._p.hal_postgui_list.append("# **** force spindle at speed indicator true because we have no feedback ****")
			self._p.hal_postgui_list.append("")
			self._p.hal_postgui_list.append("net spindle-at-speed => pyvcp.spindle-at-speed-led")
			self._p.hal_postgui_list.append("sets spindle-at-speed true")

	#self.d.halui_list.append("G30.1")
	self._p.hal_postgui_list.append("net my-set-position pyvcp.set-position => halui.mdi-command-%02d" % (self._p.halui_mdi_set_position) )
	#self.d.halui_list.append("G30")
	self._p.hal_postgui_list.append("net my-goto-position pyvcp.goto-position => halui.mdi-command-%02d" % (self._p.halui_mdi_goto_position) )

	# TOOL LENGHT SENSOR
	inputs = self.build_input_set()
	if (d_hal_input[PROBE] in inputs):
		#self.d.halui_list.append("o<pyvcp_probe> call")
		self._p.hal_postgui_list.append("net my-probe pyvcp.probe => halui.mdi-command-%02d" % (self.d.halui_probe_tool_lenght) )

################################################
def create_pyvcp_hal_merge(self):
	# Use when we have pyvcp and gladevcp
	self._p.hal_postgui_list = []

	# Note:
	# I use the mux2 + logic or to select the jog for the pyvcp interface. Gladevcp is used by default.
	# X
	self._p.hal_postgui_list.append("# connect the X PyVCP buttons")
	self._p.hal_postgui_list.append("net my-jogxminus <= pyvcp.x-minus => or2.%d.in1 logic.%d.in-%02d" % (self._p.or2_jog_x_minus, self._p.logic_jog, self._p.logic_jog_x_minus))
	self._p.hal_postgui_list.append("net my-jogxplus <= pyvcp.x-plus => or2.%d.in1 logic.%d.in-%02d" % (self._p.or2_jog_x_plus, self._p.logic_jog, self._p.logic_jog_x_plus))
	self._p.hal_postgui_list.append("net my-x-null <= pyvcp.x-null => or2.%d.in1" % (self._p.or2_zero_x))
	self._p.hal_postgui_list.append("net my-x-zero <= pyvcp.x-zero => or2.%d.in1" % (self._p.or2_go_zero_x))
	
	# Y
	if self.d.axes in(0, 1, 3):
		self._p.hal_postgui_list.append("# connect the Y PyVCP buttons")
		self._p.hal_postgui_list.append("net my-jogyminus <= pyvcp.y-minus => or2.%d.in1 logic.%d.in-%02d" % (self._p.or2_jog_y_minus , self._p.logic_jog, self._p.logic_jog_y_minus))
		self._p.hal_postgui_list.append("net my-jogyplus <= pyvcp.y-plus => or2.%d.in1 logic.%d.in-%02d" % (self._p.or2_jog_y_plus, self._p.logic_jog, self._p.logic_jog_y_plus))
		self._p.hal_postgui_list.append("net my-y-null <= pyvcp.y-null => or2.%d.in1" % (self._p.or2_zero_y))
		self._p.hal_postgui_list.append("net my-y-zero <= pyvcp.y-zero => or2.%d.in1" % (self._p.or2_go_zero_y))

	# Z
	if self.d.axes in(0, 1, 2):
		self._p.hal_postgui_list.append("# connect the Z PyVCP buttons")
		self._p.hal_postgui_list.append("net my-jogzminus <= pyvcp.z-minus => or2.%d.in1 logic.%d.in-%02d" % (self._p.or2_jog_z_minus, self._p.logic_jog, self._p.logic_jog_z_minus))
		self._p.hal_postgui_list.append("net my-jogzplus <= pyvcp.z-plus => or2.%d.in1 logic.%d.in-%02d" % (self._p.or2_jog_z_plus, self._p.logic_jog, self._p.logic_jog_z_plus))
		self._p.hal_postgui_list.append("net my-z-null <= pyvcp.z-null => or2.%d.in1" % (self._p.or2_zero_z))
		self._p.hal_postgui_list.append("net my-z-zero <= pyvcp.z-zero => or2.%d.in1" % (self._p.or2_go_zero_z))

	# A
	if self.d.axes == 1:
		self._p.hal_postgui_list.append("# connect the A PyVCP buttons")
		self._p.hal_postgui_list.append("net my-jogaminus <= pyvcp.a-minus => or2.%d.in1 logic.%d.in-%02d" % (self._p.or2_jog_a_minus, self._p.logic_jog, self._p.logic_jog_a_minus))
		self._p.hal_postgui_list.append("net my-jogaplus <= pyvcp.a-plus => or2.%d.in1 logic.%d.in-%02d" % (self._p.or2_jog_a_plus, self._p.logic_jog, self._p.logic_jog_a_plus))
		self._p.hal_postgui_list.append("net my-a-null <= pyvcp.a-null => or2.%d.in1" % (self._p.or2_zero_a))
		self._p.hal_postgui_list.append("net my-a-zero <= pyvcp.a-zero => or2.%d.in1" % (self._p.or2_go_zero_a))
	# UV
	if self.d.axes == 3:
		self._p.hal_postgui_list.append("# connect the U PyVCP buttons")
		self._p.hal_postgui_list.append("net my-joguminus <= pyvcp.u-minus => or2.%d.in1 logic.%d.in-%02d" % (self._p.or2_jog_u_minus, self._p.logic_jog, self._p.logic_jog_u_minus))
		self._p.hal_postgui_list.append("net my-joguplus <= pyvcp.u-plus => or2.%d.in1 logic.%d.in-%02d" % (self._p.or2_jog_u_plus, self._p.logic_jog, self._p.logic_jog_u_plus))
		self._p.hal_postgui_list.append("net my-u-null <= pyvcp.u-null => or2.%d.in1" % (self._p.or2_zero_u))
		self._p.hal_postgui_list.append("net my-u-zero <= pyvcp.u-zero => or2.%d.in1" % (self._p.or2_go_zero_u))

		self._p.hal_postgui_list.append("# connect the V PyVCP buttons")
		self._p.hal_postgui_list.append("net my-jogvminus <= pyvcp.v-minus => or2.%d.in1 logic.%d.in-%02d" % (self._p.or2_jog_v_minus, self._p.logic_jog, self._p.logic_jog_v_minus))
		self._p.hal_postgui_list.append("net my-jogvplus <= pyvcp.v-plus => or2.%d.in1 logic.%d.in-%02d" % (self._p.or2_jog_v_plus, self._p.logic_jog, self._p.logic_jog_v_plus))
		self._p.hal_postgui_list.append("net my-v-null <= pyvcp.v-null => or2.%d.in1" % (self._p.or2_zero_v))
		self._p.hal_postgui_list.append("net my-v-zero <= pyvcp.v-zero => or2.%d.in1" % (self._p.or2_go_zero_v))

	# JOG
	self._p.hal_postgui_list.append("# connect the PyVCP jog speed slider")
	self._p.hal_postgui_list.append("net my-jogspeed-mux logic.%d.or => mux2.%d.sel" % (self._p.logic_jog, self._p.mux2_jog))
	self._p.hal_postgui_list.append("net my-jogspeed <= pyvcp.jog-speed-f => mux2.%d.in1" % (self._p.mux2_jog))

	# HOME ALL
	self._p.hal_postgui_list.append("# HOME ALL")
	self._p.hal_postgui_list.append("net my-home-all <= pyvcp.home-all => or2.%d.in1" % (self._p.or2_home_all))

	# E-STOP
	self._p.hal_postgui_list.append("# E-STOP")
	self._p.hal_postgui_list.append("net my-power-btn or2.%d.in1 <= pyvcp.estop" % (self._p.or2_estop))
	# Save position
	self._p.hal_postgui_list.append("net my-set-position <= pyvcp.set-position => or2.%d.in1" % (self._p.or2_set_position))
	self._p.hal_postgui_list.append("net my-goto-position <= pyvcp.goto-position => or2.%d.in1" % (self._p.or2_go_to_position))

	# TOOL LENGHT SENSOR
	inputs = self.build_input_set()
	if (d_hal_input[PROBE] in inputs):
		self._p.hal_postgui_list.append("net my-probe <= pyvcp.probe => or2.%d.in1" % (self._p.or2_probe))

	# Spindle speed
	if (self._p.has_spindle_speed_control or self._p.has_spindle_encoder):
		inputs = self.build_input_set()
		encoder = d_hal_input[PHA] in inputs
		if encoder:
			self._p.hal_postgui_list.append("# **** Setup of spindle speed display using pyvcp -START ****")
			self._p.hal_postgui_list.append("# **** Use ACTUAL spindle velocity from spindle encoder")
			self._p.hal_postgui_list.append("# **** spindle-velocity-feedback-rps bounces around so we filter it with lowpass")
			self._p.hal_postgui_list.append("# **** spindle-velocity-feedback-rps is signed so we use absolute component to remove sign") 
			self._p.hal_postgui_list.append("# **** ACTUAL velocity is in RPS not RPM so we scale it.")
			self._p.hal_postgui_list.append("")
			self._p.hal_postgui_list.append("setp scale.0.gain 60")
			self._p.hal_postgui_list.append("setp lowpass.0.gain %f")% self.d.spindlefiltergain
			self._p.hal_postgui_list.append("net spindle-velocity-feedback-rps               => lowpass.0.in")
			self._p.hal_postgui_list.append("net spindle-fb-filtered-rps      lowpass.0.out  => abs.0.in")
			self._p.hal_postgui_list.append("net spindle-fb-filtered-abs-rps  abs.0.out      => scale.0.in")
			self._p.hal_postgui_list.append("net spindle-fb-filtered-abs-rpm  scale.0.out    => pyvcp.spindle-speed")
			self._p.hal_postgui_list.append("")
			self._p.hal_postgui_list.append("# **** set up spindle at speed indicator ****")
			if self.d.usespindleatspeed:
				self._p.hal_postgui_list.append("")
				self._p.hal_postgui_list.append("net spindle-cmd-rps-abs             =>  near.0.in1")
				self._p.hal_postgui_list.append("net spindle-velocity-feedback-rps   =>  near.0.in2")
				self._p.hal_postgui_list.append("net spindle-at-speed                <=  near.0.out")
				self._p.hal_postgui_list.append("setp near.0.scale %f")% self.d.spindlenearscale
			else:
				self._p.hal_postgui_list.append("# **** force spindle at speed indicator true because we chose no feedback ****")
				self._p.hal_postgui_list.append("")
				self._p.hal_postgui_list.append("sets spindle-at-speed true")
			self._p.hal_postgui_list.append("net spindle-at-speed       => pyvcp.spindle-at-speed-led")
		else:
			self._p.hal_postgui_list.append("# **** Use COMMANDED spindle velocity from LinuxCNC because no spindle encoder was specified")
			self._p.hal_postgui_list.append("")
			self._p.hal_postgui_list.append("net spindle-cmd-rpm-abs    => pyvcp.spindle-speed")
			self._p.hal_postgui_list.append("")
			self._p.hal_postgui_list.append("# **** force spindle at speed indicator true because we have no feedback ****")
			self._p.hal_postgui_list.append("")
			self._p.hal_postgui_list.append("net spindle-at-speed => pyvcp.spindle-at-speed-led")
			self._p.hal_postgui_list.append("sets spindle-at-speed true")

################################################
def create_pyvcp_panel(self, filename):
	file = open(filename, "w")
	print >>file, ("""<?xml version="1.0"?>
<pyvcp>
<labelframe text="Jog Buttons">
	<font>("Helvetica",16)</font>
	<!-- the X jog buttons -->
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"x-plus"</halpin>
		<text>"X+"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"x-minus"</halpin>
		<text>"X-"</text>
		</button>
	</hbox>
""")
	if self.d.axes in(0, 1, 3):
		print >>file, ("""
	<!-- the Y jog buttons -->
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"y-plus"</halpin>
		<text>"Y+"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"y-minus"</halpin>
		<text>"Y-"</text>
		</button>
	</hbox>
""")
	if self.d.axes in(0, 1, 2):
		print >>file, ("""
	<!-- the Z jog buttons -->
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"z-plus"</halpin>
		<text>"Z+"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"z-minus"</halpin>
		<text>"Z-"</text>
		</button>
	</hbox>
""")
	if self.d.axes == 1:
		print >>file, ("""
	<!-- the A jog buttons -->
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"a-plus"</halpin>
		<text>"A+"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"a-minus"</halpin>
		<text>"A-"</text>
		</button>
	</hbox>
""")
	if self.d.axes == 3:
		print >>file, ("""
	<!-- the U jog buttons -->
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"u-plus"</halpin>
		<text>"U+"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"u-minus"</halpin>
		<text>"U-"</text>
		</button>
	</hbox>
	<!-- the V jog buttons -->
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"v-plus"</halpin>
		<text>"V+"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"v-minus"</halpin>
		<text>"V-"</text>
		</button>
	</hbox>
""")
	print >>file, ("""
	<!-- the jog speed slider -->
	<vbox>
		<relief>RAISED</relief>
		<bd>3</bd>
		<label>
		<text>"Jog Speed"</text>
		<font>("Helvetica",16)</font>
		</label>
		<scale>
		<font>("Helvetica",14)</font>
		<halpin>"jog-speed"</halpin>
		<resolution>1</resolution>
		<orient>HORIZONTAL</orient>
		<min_>0</min_>
""")
	print >>file, ("		<max_>%s</max_>" % self.general_maxspeed)
	print >>file, ("""
		</scale>
	</vbox>
""")
	print >>file, ("""
</labelframe>
<!-- nullpunkt -->
<labelframe text="G54 set zero">
	<font>("Helvetica",16)</font>
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"x-null"</halpin>
		<text>"X=0"</text>
		</button>
""")
	if self.d.axes in(0, 1, 3):
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"y-null"</halpin>
		<text>"Y=0"</text>
		</button>
""")
	if self.d.axes in(0, 1, 2):
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"z-null"</halpin>
		<text>"Z=0"</text>
		</button>
""")
	if self.d.axes == 1:
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"a-null"</halpin>
		<text>"A=0"</text>
		</button>
""")
	if self.d.axes == 3:
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"u-null"</halpin>
		<text>"U=0"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"v-null"</halpin>
		<text>"V=0"</text>
		</button>
""")
	print >>file, ("""
	</hbox>
</labelframe>
<!-- Go to zero -->
<labelframe text="Go to zero">
	<font>("Helvetica",16)</font>
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"x-zero"</halpin>
		<text>"-X-"</text>
		</button>
""")
	if self.d.axes in(0, 1, 3):
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"y-zero"</halpin>
		<text>"-Y-"</text>
		</button>
""")
	if self.d.axes in(0, 1, 2):
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"z-zero"</halpin>
		<text>"-Z-"</text>
		</button>
""")
	if self.d.axes == 1:
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"a-zero"</halpin>
		<text>"-A-"</text>
		</button>
""")
	if self.d.axes == 3:
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"u-zero"</halpin>
		<text>"-U-"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"v-zero"</halpin>
		<text>"-V-"</text>
		</button>
""")
	print >>file, ("""
	</hbox>
</labelframe>
""")
	# Check for spindle speed
	if (self._p.has_spindle_speed_control or self._p.has_spindle_encoder):
		print >>file, ("""
<vbox>
<relief>RIDGE</relief>
<bd>6</bd>
	<label>
			<text>"Spindle Speed:"</text>
			<font>("Helvetica",20)</font>
	</label>
	<bar>
			<halpin>"spindle-speed"</halpin>
			<max_>3000</max_>
	</bar>
	<label>
			<text>"Spindle-At-Speed:"</text>
			<font>("Helvetica",20)</font>
	</label>
	<hbox>
		<label>
				<text>"             "</text>
				<font>("Helvetica",20)</font>
		</label>
		<led>
			<halpin>"spindle-at-speed-led"</halpin>
			<size>30</size>
			<on_color>"green"</on_color>
			<off_color>"red"</off_color>
		</led>
	</hbox>
</vbox>
""")

	print >>file, ("""
<!-- Save/Set Position -->
<labelframe text="Save/Set Position">
<font>("Helvetica",16)</font>
	<hbox>
		<button>
		<!-- G30.1 -->
		<font>("Helvetica",12)</font>
		<width>8</width>
		<halpin>"set-position"</halpin>
		<text>"Save (G30.1)"</text>
		</button>
		<button>
		<!-- G30 -->
		<font>("Helvetica",12)</font>
		<width>8</width>
		<halpin>"goto-position"</halpin>
		<text>"Goto (G30.1)"</text>
		</button>
	</hbox>
</labelframe>
""")

	# Check for tool lenght sensor
	inputs = self.build_input_set()
	if (d_hal_input[PROBE] in inputs):
		print >>file, ("""
<!-- Tool length sensor -->
<labelframe text="Tool length sensor">
<font>("Helvetica",16)</font>
	<hbox>
		<button>
		<font>("Helvetica",12)</font>
		<width>8</width>
		<halpin>"probe"</halpin>
		<text>"Probe"</text>
		</button>
	</hbox>
</labelframe>
""")

	print >>file, ("""
<!-- GENERAL -->
<labelframe text="General">
<font>("Helvetica",16)</font>
	<hbox>
		<button>
		<font>("Helvetica",12)</font>
		<justify>CENTER</justify>
		<width>8</width>
		<halpin>"estop"</halpin>
		<text>"ESTOP"</text>
		</button>
		<button>
		<font>("Helvetica",12)</font>
		<justify>CENTER</justify>
		<width>8</width>
		<halpin>"home-all"</halpin>
		<text>"HOME ALL"</text>
		</button>
	</hbox>
</labelframe>
""")

	print >>file, ("""
</pyvcp>
""")
	file.close()

def create_pyvcp_custom(self,filename):
	file = open(filename, "w")
	print >>file, ("""<?xml version="1.0"?>
<pyvcp>
</pyvcp>
""")
	file.close()
