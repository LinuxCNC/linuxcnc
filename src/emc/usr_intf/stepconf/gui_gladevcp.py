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
# GUI GLADEVCP
#************
import os
import time
from stepconf.definitions import *

def create_gladevcp_hal_single(self):
	# Use when we have only pyvcp interface
	# Currently unused
	self._p.hal_gvcp_list = []

	# X
	self._p.hal_gvcp_list.append("# connect the X Gladevcp buttons")
	self._p.hal_gvcp_list.append("net gvcp-jogxminus halui.axis.x.minus <= gladevcp.jog_x_minus")
	self._p.hal_gvcp_list.append("net gvcp-jogxplus halui.axis.x.plus <= gladevcp.jog_x_plus")
	self._p.hal_gvcp_list.append("net gvcp-x-null gladevcp.zero_x => halui.mdi-command-%02d" % (self._p.halui_mdi_x_null) )
	self._p.hal_gvcp_list.append("net gvcp-x-zero gladevcp.go_zero_x => halui.mdi-command-%02d" % (self._p.halui_mdi_x_zero) )

	# Y
	if self.d.axes in(0, 1, 3):
		self._p.hal_gvcp_list.append("# connect the Y Gladevcp buttons")
		self._p.hal_gvcp_list.append("net gvcp-jogyminus halui.axis.y.minus <= gladevcp.jog_y_minus")
		self._p.hal_gvcp_list.append("net gvcp-jogyplus halui.axis.y.plus <= gladevcp.jog_y_plus")
		self._p.hal_gvcp_list.append("net gvcp-y-null gladevcp.zero_y => halui.mdi-command-%02d" % (self._p.halui_mdi_y_null) )
		self._p.hal_gvcp_list.append("net gvcp-y-zero gladevcp.go_zero_y => halui.mdi-command-%02d" % (self._p.halui_mdi_y_zero) )
	# Z
	if self.d.axes in(0, 1, 2):
		self._p.hal_gvcp_list.append("# connect the Z Gladevcp buttons")
		self._p.hal_gvcp_list.append("net gvcp-jogzminus halui.axis.z.minus <= gladevcp.jog_z_minus")
		self._p.hal_gvcp_list.append("net gvcp-jogzplus halui.axis.z.plus <= gladevcp.jog_z_plus")
		self._p.hal_gvcp_list.append("net gvcp-z-null gladevcp.zero_z => halui.mdi-command-%02d" % (self._p.halui_mdi_z_null) )
		self._p.hal_gvcp_list.append("net gvcp-z-zero gladevcp.go_zero_z => halui.mdi-command-%02d" % (self._p.halui_mdi_z_zero) )
	# A
	if self.d.axes == 1:
		self._p.hal_gvcp_list.append("# connect the A Gladevcp buttons")
		self._p.hal_gvcp_list.append("net gvcp-jogaminus halui.axis.a.minus <= gladevcp.jog_a_minus")
		self._p.hal_gvcp_list.append("net gvcp-jogaplus halui.axis.a.plus <= gladevcp.jog_a_plus")
		self._p.hal_gvcp_list.append("net gvcp-a-null gladevcp.zero_a => halui.mdi-command-%02d" % (self._p.halui_mdi_a_null) )
		self._p.hal_gvcp_list.append("net gvcp-a-zero gladevcp.go_zero_a => halui.mdi-command-%02d" % (self._p.halui_mdi_a_zero) )
	# UV
	if self.d.axes == 3:
		self._p.hal_gvcp_list.append("# connect the U Gladevcp buttons")
		self._p.hal_gvcp_list.append("net gvcp-joguminus halui.axis.u.minus <= gladevcp.jog_u_minus")
		self._p.hal_gvcp_list.append("net gvcp-joguplus halui.axis.u.plus <= gladevcp.jog_u_plus")
		self._p.hal_gvcp_list.append("net gvcp-u-null gladevcp.zero_u => halui.mdi-command-%02d" % (self._p.halui_mdi_u_null) )
		self._p.hal_gvcp_list.append("net gvcp-u-zero gladevcp.go_zero_u => halui.mdi-command-%02d" % (self._p.halui_mdi_u_zero) )

		self._p.hal_gvcp_list.append("# connect the V Gladevcp buttons")
		self._p.hal_gvcp_list.append("net gvcp-jogvminus halui.axis.v.minus <= gladevcp.jog_v_minus")
		self._p.hal_gvcp_list.append("net gvcp-jogvplus halui.axis.v.plus <= gladevcp.jog_v_plus")
		self._p.hal_gvcp_list.append("net gvcp-v-null gladevcp.zero_v => halui.mdi-command-%02d" % (self._p.halui_mdi_v_null) )
		self._p.hal_gvcp_list.append("net gvcp-v-zero gladevcp.go_zero_v => halui.mdi-command-%02d" % (self._p.halui_mdi_v_zero) )

	# HOME ALL
	self._p.hal_gvcp_list.append("# HOME-ALL")
	self._p.hal_gvcp_list.append("net gvcp-home-all halui.home-all <= gladevcp.home_all")

	# E-STOP
	self._p.hal_gvcp_list.append("# ESTOP")
	self._p.hal_gvcp_list.append("net power-btn and2.0.in0 <= gladevcp.estop")
	self._p.hal_gvcp_list.append("net power-ok and2.0.in1 not.0.out")
	self._p.hal_gvcp_list.append("net power-not not.0.in <= halui.estop.is-activated")
	self._p.hal_gvcp_list.append("net power-request toggle.0.in <= and2.0.out")
	self._p.hal_gvcp_list.append("net power-toggle toggle2nist.0.in <= toggle.0.out")
	self._p.hal_gvcp_list.append("net power-is-on toggle2nist.0.is-on <= halui.machine.is-on")
	self._p.hal_gvcp_list.append("net power-on halui.machine.on <= toggle2nist.0.on")
	self._p.hal_gvcp_list.append("net power-off halui.machine.off <= toggle2nist.0.off")
	self._p.hal_gvcp_list.append("")
	
	# JOG
	self._p.hal_gvcp_list.append("# connect the gladevcp jog speed slider")
	self._p.hal_gvcp_list.append("net gvcp-jogspeed halui.axis.jog-speed <= gladevcp.scale_jog_speed")

	# Spindle speed
	if (self._p.has_spindle_speed_control or self._p.has_spindle_encoder):
		self._p.hal_gvcp_list.append("# connect the Gladevcp spindle speed slider")
		self._p.hal_gvcp_list.append("net gvcp-spindlespeed <= gladevcp.scale_spindle_speed => mux2.%d.in1" % (self._p.mux2_spindle_speed))

		inputs = self.build_input_set()
		encoder = d_hal_input[PHA] in inputs
		if encoder:
			self._p.hal_gvcp_list.append("# **** Setup of spindle speed display using gladevcp -START ****")
			self._p.hal_gvcp_list.append("# **** Use ACTUAL spindle velocity from spindle encoder")
			self._p.hal_gvcp_list.append("# **** spindle-velocity-feedback-rps bounces around so we filter it with lowpass")
			self._p.hal_gvcp_list.append("# **** spindle-velocity-feedback-rps is signed so we use absolute component to remove sign") 
			self._p.hal_gvcp_list.append("# **** ACTUAL velocity is in RPS not RPM so we scale it.")
			self._p.hal_gvcp_list.append("")
			self._p.hal_gvcp_list.append("setp scale.0.gain 60")
			self._p.hal_gvcp_list.append("setp lowpass.0.gain %f")% self.spindlefiltergain
			self._p.hal_gvcp_list.append("net spindle-velocity-feedback-rps               => lowpass.0.in")
			self._p.hal_gvcp_list.append("net spindle-fb-filtered-rps      lowpass.0.out  => abs.0.in")
			self._p.hal_gvcp_list.append("net spindle-fb-filtered-abs-rps  abs.0.out      => scale.0.in")
			self._p.hal_gvcp_list.append("net spindle-fb-filtered-abs-rpm  scale.0.out    => gladevcp.bar_spindle_speed")
			self._p.hal_gvcp_list.append("")
			self._p.hal_gvcp_list.append("# **** set up spindle at speed indicator ****")
			if self.d.usespindleatspeed:
				self._p.hal_gvcp_list.append("")
				self._p.hal_gvcp_list.append("net spindle-cmd-rps-abs             =>  near.%d.in1" % (self._p.near_spindle) )
				self._p.hal_gvcp_list.append("net spindle-velocity-feedback-rps   =>  near.%d.in2" % (self._p.near_spindle) )
				self._p.hal_gvcp_list.append("net spindle-at-speed                <=  near.%d.out" % (self._p.near_spindle) )
				self._p.hal_gvcp_list.append("setp near.%d.scale %f" % (self._p.near_spindle, self.spindlenearscale) )
			else:
				self._p.hal_gvcp_list.append("# **** force spindle at speed indicator true because we chose no feedback ****")
				self._p.hal_gvcp_list.append("")
				self._p.hal_gvcp_list.append("sets spindle-at-speed true")
			self._p.hal_gvcp_list.append("net spindle-at-speed       => gladevcp.spindle_at_speed_led")
		else:
			self._p.hal_gvcp_list.append("# **** Use COMMANDED spindle velocity from LinuxCNC because no spindle encoder was specified")
			self._p.hal_gvcp_list.append("")
			self._p.hal_gvcp_list.append("net spindle-cmd-rpm-abs    => gladevcp.bar_spindle_speed")
			self._p.hal_gvcp_list.append("")
			self._p.hal_gvcp_list.append("# **** force spindle at speed indicator true because we have no feedback ****")
			self._p.hal_gvcp_list.append("")
			self._p.hal_gvcp_list.append("net spindle-at-speed => gladevcp.spindle_at_speed_led")
			self._p.hal_gvcp_list.append("sets spindle-at-speed true")

	# Save position
	self._p.hal_gvcp_list.append("net gvcp-set-position gladevcp.set_position => halui.mdi-command-%02d" % (self._p.halui_mdi_set_position) )
	self._p.hal_gvcp_list.append("net gvcp-goto-position gladevcp.go_to_position => halui.mdi-command-%02d" % (self._p.halui_mdi_goto_position) )

	# TOOL LENGHT SENSOR
	inputs = self.build_input_set()
	if (d_hal_input[PROBE] in inputs):
		self._p.hal_gvcp_list.append("net gvcp-probe gladevcp.probe => halui.mdi-command-%02d" % (self.d.halui_probe_tool_lenght) )

################################################
def create_gladevcp_hal_merge(self):
	# Use when we have pyvcp and gladevcp
	self._p.hal_gvcp_list = []

	# X
	self._p.hal_gvcp_list.append("# connect the X Gladevcp buttons")
	self._p.hal_gvcp_list.append("net gvcp-jogxminus <= gladevcp.jog_x_minus => or2.%d.in0" % (self._p.or2_jog_x_minus))
	self._p.hal_gvcp_list.append("net gvcp-jogxplus <= gladevcp.jog_x_plus => or2.%d.in0" % (self._p.or2_jog_x_plus))
	self._p.hal_gvcp_list.append("net gvcp-x-null <= gladevcp.zero_x => or2.%d.in0" % (self._p.or2_zero_x))
	self._p.hal_gvcp_list.append("net gvcp-x-zero <= gladevcp.go_zero_x => or2.%d.in0" % (self._p.or2_go_zero_x))
	
	# Y
	if self.d.axes in(0, 1, 3):
		self._p.hal_gvcp_list.append("# connect the Y Gladevcp buttons")
		self._p.hal_gvcp_list.append("net gvcp-jogyminus <= gladevcp.jog_y_minus => or2.%d.in0" % (self._p.or2_jog_y_minus))
		self._p.hal_gvcp_list.append("net gvcp-jogyplus <= gladevcp.jog_y_plus => or2.%d.in0" % (self._p.or2_jog_y_plus))
		self._p.hal_gvcp_list.append("net gvcp-y-null <= gladevcp.zero_y => or2.%d.in0" % (self._p.or2_zero_y))
		self._p.hal_gvcp_list.append("net gvcp-y-zero <= gladevcp.go_zero_y => or2.%d.in0" % (self._p.or2_go_zero_y))
	# Z
	if self.d.axes in(0, 1, 2):
		self._p.hal_gvcp_list.append("# connect the Z Gladevcp buttons")
		self._p.hal_gvcp_list.append("net gvcp-jogzminus <= gladevcp.jog_z_minus => or2.%d.in0" % (self._p.or2_jog_z_minus))
		self._p.hal_gvcp_list.append("net gvcp-jogzplus <= gladevcp.jog_z_plus => or2.%d.in0" % (self._p.or2_jog_z_plus))
		self._p.hal_gvcp_list.append("net gvcp-z-null <= gladevcp.zero_z => or2.%d.in0" % (self._p.or2_zero_z))
		self._p.hal_gvcp_list.append("net gvcp-z-zero <= gladevcp.go_zero_z => or2.%d.in0" % (self._p.or2_go_zero_z))
	# A
	if self.d.axes == 1:
		self._p.hal_gvcp_list.append("# connect the A Gladevcp buttons")
		self._p.hal_gvcp_list.append("net gvcp-jogaminus <= gladevcp.jog_a_minus => or2.%d.in0" % (self._p.or2_jog_a_minus))
		self._p.hal_gvcp_list.append("net gvcp-jogaplus <= gladevcp.jog_a_plus => or2.%d.in0" % (self._p.or2_jog_a_plus))
		self._p.hal_gvcp_list.append("net gvcp-a-null <= gladevcp.zero_a => or2.%d.in0" % (self._p.or2_zero_a))
		self._p.hal_gvcp_list.append("net gvcp-a-zero <= gladevcp.go_zero_a => or2.%d.in0" % (self._p.or2_go_zero_a))
	# UV
	if self.d.axes == 3:
		self._p.hal_gvcp_list.append("# connect the U Gladevcp buttons")
		self._p.hal_gvcp_list.append("net gvcp-joguminus <= gladevcp.jog_u_minus => or2.%d.in0" % (self._p.or2_jog_u_minus))
		self._p.hal_gvcp_list.append("net gvcp-joguplus <= gladevcp.jog_u_plus => or2.%d.in0" % (self._p.or2_jog_u_plus))
		self._p.hal_gvcp_list.append("net gvcp-u-null <= gladevcp.zero_u =>  or2.%d.in0" % (self._p.or2_zero_u))
		self._p.hal_gvcp_list.append("net gvcp-u-zero <= gladevcp.go_zero_u => or2.%d.in0" % (self._p.or2_go_zero_u))

		self._p.hal_gvcp_list.append("# connect the V Gladevcp buttons")
		self._p.hal_gvcp_list.append("net gvcp-jogvminus <= gladevcp.jog_v_minus => or2.%d.in0" % (self._p.or2_jog_v_minus))
		self._p.hal_gvcp_list.append("net gvcp-jogvplus <= gladevcp.jog_v_plus => or2.%d.in0" % (self._p.or2_jog_v_plus))
		self._p.hal_gvcp_list.append("net gvcp-v-null <= gladevcp.zero_v => or2.%d.in0" % (self._p.or2_zero_v))
		self._p.hal_gvcp_list.append("net gvcp-v-zero <= gladevcp.go_zero_v => or2.%d.in0" % (self._p.or2_go_zero_v))


	# HOME ALL
	self._p.hal_gvcp_list.append("net gvcp-home-all <= gladevcp.home_all => or2.%d.in0" % (self._p.or2_home_all))

	# E-STOP
	self._p.hal_gvcp_list.append("# E-STOP")
	self._p.hal_gvcp_list.append("net gvcp-power-btn or2.%d.in0 <= gladevcp.estop" % (self._p.or2_estop))

	# JOG
	self._p.hal_gvcp_list.append("# connect the Gladevcp jog speed slider")
	self._p.hal_gvcp_list.append("net gvcp-jogspeed <= gladevcp.scale_jog_speed => mux2.%d.in0" % (self._p.mux2_jog))

	# Save position
	self._p.hal_gvcp_list.append("net gvcp-set-position <= gladevcp.set_position => or2.%d.in0" % (self._p.or2_set_position))
	self._p.hal_gvcp_list.append("net gvcp-goto-position <= gladevcp.go_to_position  => or2.%d.in0" % (self._p.or2_go_to_position))

	# TOOL LENGHT SENSOR
	inputs = self.build_input_set()
	if (d_hal_input[PROBE] in inputs):
		self._p.hal_gvcp_list.append("net gvcp-probe  <= gladevcp.probe => or2.%d.in0" % (self._p.or2_probe))

	# TODO
	"""
	# Spindle speed
	if (self._p.has_spindle_speed_control or self._p.has_spindle_encoder):
		inputs = self.build_input_set()
		encoder = d_hal_input[PHA] in inputs
		if encoder:
			self._p.hal_gvcp_list.append("# **** Setup of spindle speed display using gladevcp -START ****")
			self._p.hal_gvcp_list.append("# **** Use ACTUAL spindle velocity from spindle encoder")
			self._p.hal_gvcp_list.append("# **** spindle-velocity-feedback-rps bounces around so we filter it with lowpass")
			self._p.hal_gvcp_list.append("# **** spindle-velocity-feedback-rps is signed so we use absolute component to remove sign") 
			self._p.hal_gvcp_list.append("# **** ACTUAL velocity is in RPS not RPM so we scale it.")
			self._p.hal_gvcp_list.append("")
			self._p.hal_gvcp_list.append("setp scale.0.gain 60")
			self._p.hal_gvcp_list.append("setp lowpass.0.gain %f")% self.d.spindlefiltergain
			self._p.hal_gvcp_list.append("net spindle-velocity-feedback-rps               => lowpass.0.in")
			self._p.hal_gvcp_list.append("net spindle-fb-filtered-rps      lowpass.0.out  => abs.0.in")
			self._p.hal_gvcp_list.append("net spindle-fb-filtered-abs-rps  abs.0.out      => scale.0.in")
			self._p.hal_gvcp_list.append("net spindle-fb-filtered-abs-rpm  scale.0.out    => gladevcp.bar_spindle_speed")
			self._p.hal_gvcp_list.append("")
			self._p.hal_gvcp_list.append("# **** set up spindle at speed indicator ****")
			if self.d.usespindleatspeed:
				self._p.hal_gvcp_list.append("")
				self._p.hal_gvcp_list.append("net spindle-cmd-rps-abs             =>  near.0.in1")
				self._p.hal_gvcp_list.append("net spindle-velocity-feedback-rps   =>  near.0.in2")
				self._p.hal_gvcp_list.append("net spindle-at-speed                <=  near.0.out")
				self._p.hal_gvcp_list.append("setp near.0.scale %f")% self.d.spindlenearscale
			else:
				self._p.hal_gvcp_list.append("# **** force spindle at speed indicator true because we chose no feedback ****")
				self._p.hal_gvcp_list.append("")
				self._p.hal_gvcp_list.append("sets spindle-at-speed true")
			self._p.hal_gvcp_list.append("net spindle-at-speed       => gladevcp.spindle_at_speed_led")
		else:
			self._p.hal_gvcp_list.append("# **** Use COMMANDED spindle velocity from LinuxCNC because no spindle encoder was specified")
			self._p.hal_gvcp_list.append("")
			self._p.hal_gvcp_list.append("net spindle-cmd-rpm-abs    => gladevcp.bar_spindle_speed")
			self._p.hal_gvcp_list.append("")
			self._p.hal_gvcp_list.append("# **** force spindle at speed indicator true because we have no feedback ****")
			self._p.hal_gvcp_list.append("")
			self._p.hal_gvcp_list.append("net spindle-at-speed => gladevcp.spindle_at_speed_led")
			self._p.hal_gvcp_list.append("sets spindle-at-speed true")
	"""

################################################
def create_gladevcp_panel(self,filename):
	gladevcpfile = open(filename, "w")
	print >>gladevcpfile, ("""<?xml version="1.0" encoding="UTF-8"?>
<interface>
  <!-- interface-requires gladevcp 0.0 -->
  <requires lib="gtk+" version="2.16"/>
  <!-- interface-naming-policy project-wide -->""")

	# Add GtkAdjustment for jog speed
	print >>gladevcpfile, ("""  <object class="GtkAdjustment" id="adj_jog_speed">""")
	print >>gladevcpfile, ("""    <property name="upper">%(maxrpm)d</property>"""%{'maxrpm':self.general_maxspeed })
	print >>gladevcpfile, ("""    <property name="step_increment">1</property>
    <property name="page_increment">10</property>
  </object>""")

	if (self._p.has_spindle_speed_control or self._p.has_spindle_encoder):
		# Add GtkAdjustment for jog spindle speed
		print >>gladevcpfile, ("""  <object class="GtkAdjustment" id="adj_spindle_speed">""")
		print >>gladevcpfile, ("""    <property name="upper">%(maxrpm)d</property>"""%{'maxrpm':self.d.spindlespeed2 })
		print >>gladevcpfile, ("""    <property name="step_increment">1</property>
    <property name="page_increment">10</property>
  </object>""")

	# Main Windows
	print >>gladevcpfile, ("""  <object class="GtkWindow" id="window1">
    <property name="can_focus">False</property>
    <child>
      <object class="GtkVBox" id="box1">
        <property name="visible">True</property>
        <property name="can_focus">False</property>
        <!-- destroy signal must be here, not in window1. I don't know why... -->
        <signal name="destroy" handler="on_destroy"/>""")

	# child position on box1
	position=0
	# JOG BUTTONS
	self.add_gladevcp_jog_buttons(gladevcpfile, position)
	position=position +1

	# JOG SPEED
	self.add_gladevcp_jog_speed(gladevcpfile, position)
	position=position +1

	# SET ZERO
	self.add_gladevcp_set_zero(gladevcpfile, position)
	position=position +1

	# GO TO ZERO
	self.add_gladevcp_go_to_zero(gladevcpfile, position)
	position=position +1

	# Check for spindle speed
	if (self._p.has_spindle_speed_control or self._p.has_spindle_encoder):
		self.add_gladevcp_spindle(gladevcpfile, position)
		position=position +1

	# Save goto position G30.1 G30
	self.add_gladevcp_g30(gladevcpfile, position)
	position=position +1

	# Check for tool lenght sensor
	inputs = self.build_input_set()
	if (d_hal_input[PROBE] in inputs):
		self.add_gladevcp_tool_lenght_sensor(gladevcpfile, position)
		position=position +1

	# GENERAL
	self.add_gladevcp_general(gladevcpfile, position)
	position=position +1

	# Close xml
	print >>gladevcpfile, ("""      </object>
    </child>
  </object>
</interface>""")
	gladevcpfile.close()

################################################
def add_gladevcp_jog_buttons(self, gladevcpfile, position):
	# Add jog buttons
	print >>gladevcpfile, ("""        <child>
          <object class="GtkVBox" id="box2">
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <child>
              <object class="GtkLabel" id="label_jog_buttons">
                <property name="visible">True</property>
                <property name="can_focus">False</property>
                <property name="label" translatable="yes">Jog Buttons</property>
                <attributes>
                  <attribute name="font-desc" value="Sans 14"/>
                </attributes>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">True</property>
                <property name="position">0</property>
              </packing>
            </child>
            <child>
              <object class="GtkTable" id="grid1">
                <property name="visible">True</property>
                <property name="can_focus">False</property>""")
	# Select the right number of row
	if self.d.axes == XYZ:
		print >>gladevcpfile, ("""                <property name="n_rows">3</property>""")
	elif self.d.axes == XYZA:
		print >>gladevcpfile, ("""                <property name="n_rows">4</property>""")
	elif self.d.axes == XZ:
		print >>gladevcpfile, ("""                <property name="n_rows">2</property>""")
	elif self.d.axes == XYUV:
		print >>gladevcpfile, ("""                <property name="n_rows">4</property>""")
	else:
		print >>gladevcpfile, ("""                <property name="n_rows">3</property>""")

	print >>gladevcpfile, ("""                <property name="n_columns">2</property>
                <!-- JOG X -->
                <child>
                  <object class="HAL_Button" id="jog_x_plus">
                    <property name="label" translatable="yes">X+</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                    <signal name="pressed" handler="on_jog_x_plus_press"/>
                  </object>
                  <packing>
                    <property name="left_attach">0</property>
                    <property name="top_attach">0</property>
                    <!-- <property name="width">1</property>
                    <property name="height">1</property> -->
                  </packing>
                </child>
                <child>
                  <object class="HAL_Button" id="jog_x_minus">
                    <property name="label" translatable="yes">X-</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                    <signal name="pressed" handler="on_jog_x_minus_press"/>
                  </object>
                  <packing>
                    <property name="left_attach">1</property>
                    <property name="top_attach">0</property>
                    <!-- <property name="width">1</property>
                    <property name="height">1</property> -->
                  </packing>
                </child>""")
	# Y
	if self.d.axes in(XYZ, XYZA, XYUV):
		print >>gladevcpfile, ("""
                <!-- JOG Y -->
                <child>
                  <object class="HAL_Button" id="jog_y_plus">
                    <property name="label" translatable="yes">Y+</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                    <signal name="pressed" handler="on_jog_y_plus_press"/>
                  </object>
                  <packing>
                    <property name="left_attach">0</property>
                    <property name="top_attach">1</property>
                    <!-- <property name="width">1</property>
                    <property name="height">1</property> -->
                  </packing>
                </child>
                <child>
                  <object class="HAL_Button" id="jog_y_minus">
                    <property name="label" translatable="yes">Y-</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                    <signal name="pressed" handler="on_jog_y_minus_press"/>
                  </object>
                  <packing>
                    <property name="left_attach">1</property>
                    <property name="top_attach">1</property>
                    <!-- <property name="width">1</property>
                    <property name="height">1</property> -->
                  </packing>
                </child>""")
	# Z
	if self.d.axes in(XYZ, XYZA, XZ):
		print >>gladevcpfile, ("""
                <!-- JOG Z -->
                <child>
                  <object class="HAL_Button" id="jog_z_plus">
                    <property name="label" translatable="yes">Z+</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                    <signal name="pressed" handler="on_jog_z_plus_press"/>
                  </object>
                  <packing>
                    <property name="left_attach">0</property>""")
		if (self.d.axes == XZ):
			print >>gladevcpfile, ("""					<property name="top_attach">1</property>""")
		else:
			print >>gladevcpfile, ("""					<property name="top_attach">2</property>""")
		print >>gladevcpfile, ("""
                    <!-- <property name="width">1</property>
                    <property name="height">1</property> -->
                  </packing>
                </child>
                <child>
                  <object class="HAL_Button" id="jog_z_minus">
                    <property name="label" translatable="yes">Z-</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                    <signal name="pressed" handler="on_jog_z_minus_press"/>
                  </object>
                  <packing>
                    <property name="left_attach">1</property>""")
		if (self.d.axes == XZ):
			print >>gladevcpfile, ("""					<property name="top_attach">1</property>""")
		else:
			print >>gladevcpfile, ("""					<property name="top_attach">2</property>""")
		print >>gladevcpfile, ("""
                    <!-- <property name="width">1</property>
                    <property name="height">1</property> -->
                  </packing>
                </child>""")
	# A
	if self.d.axes == XYZA:
		print >>gladevcpfile, ("""
                <!-- JOG A -->
                <child>
                  <object class="HAL_Button" id="jog_a_plus">
                    <property name="label" translatable="yes">A+</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                    <signal name="pressed" handler="on_jog_a_plus_press"/>
                  </object>
                  <packing>
                    <property name="left_attach">0</property>
                    <property name="top_attach">3</property>
                    <!-- <property name="width">1</property>
                    <property name="height">1</property> -->
                  </packing>
                </child>
                <child>
                  <object class="HAL_Button" id="jog_a_minus">
                    <property name="label" translatable="yes">A-</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                    <signal name="pressed" handler="on_jog_a_minus_press"/>
                  </object>
                  <packing>
                    <property name="left_attach">1</property>
                    <property name="top_attach">3</property>
                    <!-- <property name="width">1</property>
                    <property name="height">1</property> -->
                  </packing>
                </child>""")
	# UV
	if self.d.axes == XYUV:
		print >>gladevcpfile, ("""
                <!-- JOG U and V -->
                <child>
                  <object class="HAL_Button" id="jog_u_plus">
                    <property name="label" translatable="yes">U+</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                    <signal name="pressed" handler="on_jog_u_plus_press"/>
                  </object>
                  <packing>
                    <property name="left_attach">0</property>
                    <property name="top_attach">2</property>
                    <!-- <property name="width">1</property>
                    <property name="height">1</property> -->
                  </packing>
                </child>
                <child>
                  <object class="HAL_Button" id="jog_u_minus">
                    <property name="label" translatable="yes">U-</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                    <signal name="pressed" handler="on_jog_u_minus_press"/>
                  </object>
                  <packing>
                    <property name="left_attach">1</property>
                    <property name="top_attach">2</property>
                    <!-- <property name="width">1</property>
                    <property name="height">1</property> -->
                  </packing>
                </child>
                <child>
                  <object class="HAL_Button" id="jog_v_plus">
                    <property name="label" translatable="yes">V+</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                    <signal name="pressed" handler="on_jog_v_plus_press"/>
                  </object>
                  <packing>
                    <property name="left_attach">0</property>
                    <property name="top_attach">3</property>
                    <!-- <property name="width">1</property>
                    <property name="height">1</property> -->
                  </packing>
                </child>
                <child>
                  <object class="HAL_Button" id="jog_v_minus">
                    <property name="label" translatable="yes">V-</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                    <signal name="pressed" handler="on_jog_v_minus_press"/>
                  </object>
                  <packing>
                    <property name="left_attach">1</property>
                    <property name="top_attach">3</property>
                    <!-- <property name="width">1</property>
                    <property name="height">1</property> -->
                  </packing>
                </child>""")
	print >>gladevcpfile, ("""
              </object>
              <packing>
                <property name="expand">True</property>
                <property name="fill">True</property>
                <property name="position">1</property>
              </packing>
            </child>
          </object>
          <packing>
            <property name="expand">True</property>
            <property name="fill">True</property>""")
	print >>gladevcpfile, ("""            <property name="position">%(position)d</property>"""%{'position':position })
	print >>gladevcpfile, ("""          </packing>
        </child>""")

################################################
def add_gladevcp_jog_speed(self, gladevcpfile, position):
	print >>gladevcpfile, ("""
        <!-- JOG SPEED -->
        <child>
          <object class="GtkVBox" id="box3">
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="orientation">vertical</property>
            <child>
              <object class="GtkLabel" id="label_jog_speed">
                <property name="visible">True</property>
                <property name="can_focus">False</property>
                <property name="label" translatable="yes">Jog Speed</property>
                <attributes>
                  <attribute name="font-desc" value="Sans 14"/>
                </attributes>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">True</property>
                <property name="position">0</property>
              </packing>
            </child>
            <child>
              <object class="HAL_HScale" id="scale_jog_speed">
                <property name="visible">True</property>
                <property name="can_focus">True</property>
                <property name="adjustment">adj_jog_speed</property>
                <property name="round_digits">1</property>
                <property name="digits">0</property>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">True</property>
                <property name="position">1</property>
              </packing>
            </child>
          </object>
          <packing>
            <property name="expand">True</property>
            <property name="fill">False</property>
            <property name="padding">10</property>""")
	print >>gladevcpfile, ("""            <property name="position">%(position)d</property>"""%{'position':position })
	print >>gladevcpfile, ("""          </packing>
        </child>""")

################################################
def add_gladevcp_set_zero(self, gladevcpfile, position):
	print >>gladevcpfile, ("""
        <child>
          <object class="GtkVBox" id="box4">
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="orientation">vertical</property>
            <child>
              <object class="GtkLabel" id="label_set_zero">
                <property name="visible">True</property>
                <property name="can_focus">False</property>
                <property name="label" translatable="yes">G54 Set Zero</property>
                <attributes>
                  <attribute name="font-desc" value="Sans 14"/>
                </attributes>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">True</property>
                <property name="position">0</property>
              </packing>
            </child>
            <child>
              <object class="GtkHBox" id="box5">
                <property name="visible">True</property>
                <property name="can_focus">False</property>
                <!-- ZERO X -->
                <child>
                  <object class="HAL_Button" id="zero_x">
                    <property name="label" translatable="yes">X=0</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>
                    <property name="position">0</property>
                  </packing>
                </child>""")
	# Y
	if self.d.axes in(XYZ, XYZA, XYUV):
		print >>gladevcpfile, ("""
                <!-- ZERO Y -->
                <child>
                  <object class="HAL_Button" id="zero_y">
                    <property name="label" translatable="yes">Y=0</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>
                    <property name="position">1</property>
                  </packing>
                </child>""")
	# Z
	if self.d.axes in(XYZ, XYZA, XZ):
		print >>gladevcpfile, ("""
                <!-- ZERO Z -->
                <child>
                  <object class="HAL_Button" id="zero_z">
                    <property name="label" translatable="yes">Z=0</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>""")
		if (self.d.axes == XZ):
			print >>gladevcpfile, ("""                    <property name="position">1</property>""")
		else:
			print >>gladevcpfile, ("""                    <property name="position">2</property>""")
		print >>gladevcpfile, ("""
                  </packing>
                </child>""")
	# A
	if self.d.axes == XYZA:
		print >>gladevcpfile, ("""
                <!-- ZERO A -->
                <child>
                  <object class="HAL_Button" id="zero_a">
                    <property name="label" translatable="yes">A=0</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>
                    <property name="position">3</property>
                  </packing>
                </child>""")
	# UV
	if self.d.axes == XYUV:
		print >>gladevcpfile, ("""
                <!-- ZERO U and V -->
                <child>
                  <object class="HAL_Button" id="zero_u">
                    <property name="label" translatable="yes">U=0</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>
                    <property name="position">2</property>
                  </packing>
                </child>
                <child>
                  <object class="HAL_Button" id="zero_v">
                    <property name="label" translatable="yes">V=0</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>
                    <property name="position">3</property>
                  </packing>
                </child>""")
	print >>gladevcpfile, ("""
			</object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">True</property>
                <property name="position">1</property>
              </packing>
            </child>
          </object>
          <packing>
            <property name="expand">False</property>
            <property name="fill">True</property>
            <property name="padding">10</property>""")
	print >>gladevcpfile, ("""            <property name="position">%(position)d</property>"""%{'position':position })
	print >>gladevcpfile, ("""          </packing>
        </child>""")

################################################
def add_gladevcp_go_to_zero(self, gladevcpfile, position):
	print >>gladevcpfile, ("""
        <child>
          <object class="GtkVBox" id="box_go_to_zero">
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="orientation">vertical</property>
            <child>
              <object class="GtkLabel" id="label_go_to_zero">
                <property name="visible">True</property>
                <property name="can_focus">False</property>
                <property name="label" translatable="yes">Go To Zero</property>
                <attributes>
                  <attribute name="font-desc" value="Sans 14"/>
                </attributes>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">True</property>
                <property name="position">0</property>
              </packing>
            </child>
            <child>
              <object class="GtkHBox" id="box7">
                <property name="visible">True</property>
                <property name="can_focus">False</property>
                <!-- GO TO ZERO X -->
                <child>
                  <object class="HAL_Button" id="go_zero_x">
                    <property name="label" translatable="yes">-X-</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>
                    <property name="position">0</property>
                  </packing>
                </child>""")
	# Y
	if self.d.axes in(XYZ, XYZA, XYUV):
		print >>gladevcpfile, ("""
                <!-- GO TO ZERO Y -->
                <child>
                  <object class="HAL_Button" id="go_zero_y">
                    <property name="label" translatable="yes">-Y-</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>
                    <property name="position">1</property>
                  </packing>
                </child>""")
	# Z
	if self.d.axes in(XYZ, XYZA, XZ):
		print >>gladevcpfile, ("""
                <!-- GO TO ZERO Z -->
                <child>
                  <object class="HAL_Button" id="go_zero_z">
                    <property name="label" translatable="yes">-Z-</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>""")
		if (self.d.axes == XZ):
			print >>gladevcpfile, ("""					<property name="position">1</property>""")
		else:
			print >>gladevcpfile, ("""					<property name="position">2</property>""")
		print >>gladevcpfile, ("""
                  </packing>
                </child>""")
	# A
	if self.d.axes == XYZA:
		print >>gladevcpfile, ("""
                <!-- GO TO ZERO A -->
                <child>
                  <object class="HAL_Button" id="go_zero_a">
                    <property name="label" translatable="yes">-A-</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>
                    <property name="position">3</property>
                  </packing>
                </child>""")
	# UV
	if self.d.axes == XYUV:
		print >>gladevcpfile, ("""
                <!-- GO TO ZERO U and V -->
                <child>
                  <object class="HAL_Button" id="go_zero_u">
                    <property name="label" translatable="yes">-U-</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>
                    <property name="position">2</property>
                  </packing>
                </child>
                <child>
                  <object class="HAL_Button" id="go_zero_v">
                    <property name="label" translatable="yes">-V-</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>
                    <property name="position">3</property>
                  </packing>
                </child>""")
	print >>gladevcpfile, ("""
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">True</property>
                <property name="position">1</property>
              </packing>
            </child>
          </object>
          <packing>
            <property name="expand">False</property>
            <property name="fill">True</property>
            <property name="padding">10</property>""")
	print >>gladevcpfile, ("""            <property name="position">%(position)d</property>"""%{'position':position })
	print >>gladevcpfile, ("""          </packing>
        </child>""")

################################################
def add_gladevcp_spindle(self, gladevcpfile, position):
	print >>gladevcpfile, ("""
        <!-- SPINDLE SPEED -->
        <child>
          <object class="GtkVBox" id="box8">
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="orientation">vertical</property>
            <child>
              <object class="GtkLabel" id="label_spindle">
                <property name="visible">True</property>
                <property name="can_focus">False</property>
                <property name="label" translatable="yes">Spindle</property>
                <attributes>
                  <attribute name="font-desc" value="Sans 14"/>
                </attributes>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">True</property>
                <property name="position">0</property>
              </packing>
            </child>
            <child>
              <object class="HAL_HBar" id="bar_spindle_speed">
                <property name="visible">True</property>
                <property name="bg_color">#bebebebebebe</property>
                <property name="z1_border">0.85000002384185791</property>
                <property name="z0_border">0.69999998807907104</property>
                <property name="force_height">20</property>
                <property name="max">100</property>
                <property name="z1_color">#ffffffff0000</property>
                <property name="z2_color">#ffff00000000</property>
                <property name="z0_color">#0000ffff0000</property>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">True</property>
                <property name="position">1</property>
              </packing>
            </child>
            <child>
              <object class="HAL_HScale" id="scale_spindle_speed">
                <property name="visible">True</property>
                <property name="can_focus">True</property>
                <property name="adjustment">adj_spindle_speed</property>
                <property name="round_digits">1</property>
                <property name="digits">0</property>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">True</property>
                <property name="position">2</property>
              </packing>
            </child>
            <child>
              <object class="HAL_LED" id="spindle_at_speed_led">
                 <property name="visible">True</property>
                 <property name="pick_color_on">#f88096020000</property>
                 <property name="pick_color_off">#00002724ffff</property>
               </object>
               <packing>
                <property name="expand">False</property>
                <property name="fill">True</property>
                <property name="position">3</property>
               </packing>
            </child>
          </object>
          <packing>
            <property name="expand">False</property>
            <property name="fill">True</property>
            <property name="padding">10</property>""")
	print >>gladevcpfile, ("""            <property name="position">%(position)d</property>"""%{'position':position })
	print >>gladevcpfile, ("""          </packing>
        </child>""")

################################################
def add_gladevcp_g30(self, gladevcpfile, position):
	print >>gladevcpfile, ("""
		<!-- G30.1 G30 -->
		<child>
		  <object class="GtkVBox" id="vbox1">
			<property name="visible">True</property>
			<property name="can_focus">False</property>
			<property name="orientation">vertical</property>
			<child>
			  <object class="GtkLabel" id="label7">
				<property name="visible">True</property>
				<property name="can_focus">False</property>
				<property name="label" translatable="yes">Save/Goto Position</property>
                <attributes>
                  <attribute name="font-desc" value="Sans 14"/>
                </attributes>
			  </object>
			  <packing>
				<property name="expand">False</property>
				<property name="fill">True</property>
				<property name="position">0</property>
			  </packing>
			</child>
			<child>
			  <object class="GtkHBox" id="box10">
				<property name="visible">True</property>
				<property name="can_focus">False</property>
				<child>
				  <object class="HAL_Button" id="set_position">
					<property name="label" translatable="yes">Save Pos.(G30.1)</property>
					<property name="visible">True</property>
					<property name="can_focus">True</property>
					<property name="receives_default">True</property>
					<property name="use_action_appearance">False</property>
				  </object>
				  <packing>
					<property name="expand">True</property>
					<property name="fill">True</property>
					<property name="position">0</property>
				  </packing>
				</child>
				<child>
				  <object class="HAL_Button" id="go_to_position">
					<property name="label" translatable="yes">Goto Pos.(G30)</property>
					<property name="visible">True</property>
					<property name="can_focus">True</property>
					<property name="receives_default">True</property>
					<property name="use_action_appearance">False</property>
				  </object>
				  <packing>
					<property name="expand">True</property>
					<property name="fill">True</property>
					<property name="position">1</property>
				  </packing>
				</child>
			  </object>
			  <packing>
				<property name="expand">False</property>
				<property name="fill">True</property>
				<property name="position">1</property>
			  </packing>
			</child>
		  </object>
		  <packing>
			<property name="expand">False</property>
			<property name="fill">True</property>
			<property name="padding">10</property>""")
	print >>gladevcpfile, ("""            <property name="position">%(position)d</property>"""%{'position':position })
	print >>gladevcpfile, ("""          </packing>
        </child>""")

################################################
def add_gladevcp_tool_lenght_sensor(self, gladevcpfile, position):
	print >>gladevcpfile, ("""
        <!-- TOOL LENGHT SENSOR -->
        <child>
          <object class="GtkVBox" id="box_probe">
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <child>
			  <object class="GtkLabel" id="label_tool_lenght_sensor">
				<property name="visible">True</property>
				<property name="can_focus">False</property>
				<property name="label" translatable="yes">Tool Lenght Sensor</property>
                <attributes>
                  <attribute name="font-desc" value="Sans 14"/>
                </attributes>
			  </object>
			  <packing>
				<property name="expand">False</property>
				<property name="fill">True</property>
				<property name="position">0</property>
			  </packing>
            </child>
            <child>
              <object class="HAL_Button" id="probe">
                <property name="label" translatable="yes">Probe</property>
                <property name="visible">True</property>
                <property name="can_focus">True</property>
                <property name="receives_default">True</property>
                <property name="use_action_appearance">False</property>
              </object>
              <packing>
                <property name="expand">True</property>
                <property name="fill">True</property>
                <property name="position">1</property>
              </packing>
            </child>
          </object>
          <packing>
            <property name="expand">False</property>
            <property name="fill">True</property>
            <property name="padding">10</property>""")
	print >>gladevcpfile, ("""            <property name="position">%(position)d</property>"""%{'position':position })
	print >>gladevcpfile, ("""          </packing>
        </child>""")
################################################
def add_gladevcp_general(self, gladevcpfile, position):
	print >>gladevcpfile, ("""
		<!-- GENERAL -->
        <child>
          <object class="GtkVBox" id="box_general">
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <child>
			  <object class="GtkLabel" id="lbl_general">
				<property name="visible">True</property>
				<property name="can_focus">False</property>
				<property name="label" translatable="yes">General</property>
                <attributes>
                  <attribute name="font-desc" value="Sans 14"/>
                </attributes>
			  </object>
			  <packing>
				<property name="expand">False</property>
				<property name="fill">True</property>
				<property name="position">0</property>
			  </packing>
            </child>""")
	# ESTOP
	print >>gladevcpfile, ("""
        <child>
              <object class="GtkHBox" id="hbox_general">
                <property name="visible">True</property>
                <property name="can_focus">False</property>
                <!-- ESTOP -->
                <child>
                  <object class="HAL_Button" id="estop">
                    <property name="label" translatable="yes">ESTOP</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>
                    <property name="position">0</property>
                  </packing>
                </child>
                <!-- HOME ALL -->
                <child>
                  <object class="HAL_Button" id="home_all">
                    <property name="label" translatable="yes">HOME ALL</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="use_action_appearance">False</property>
                  </object>
                  <packing>
                    <property name="expand">True</property>
                    <property name="fill">True</property>
                    <property name="position">1</property>
                  </packing>
                </child>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">True</property>
                <property name="position">1</property>
              </packing>
            </child>
          </object>
          <packing>
            <property name="expand">False</property>
            <property name="fill">True</property>
            <property name="padding">10</property>""")
	print >>gladevcpfile, ("""            <property name="position">%(position)d</property>"""%{'position':position })
	print >>gladevcpfile, ("""          </packing>
        </child>""")

################################################
################################################
def create_gladevcp_py(self, filename):
	# Do not use gladevcp.py as filename.
	file = open(filename, "w")
	print >>file, ("""#!/usr/bin/env python
# -*- coding: utf-8 -*-
""")

	print >>file, _("# Generated by stepconf %(version)s at %(currenttime)s") % {'version':STEPCONF_VERSION, 'currenttime':time.asctime()}
	print >>file, _("# If you make changes to this file, they will be").encode('utf-8')
	print >>file, _("# overwritten when you run stepconf again").encode('utf-8')

	print >>file, ("""
import gtk
import hal
import glib
import time
import sys

class HandlerClass:
	'''
	class with gladevcp callback handlers
	'''
	def __init__(self, halcomp,builder,useropts):
		'''
		Handler classes are instantiated in the following state:
		- the widget tree is created, but not yet realized (no toplevel window.show() executed yet)
		- the halcomp HAL component is set up and the widhget tree's HAL pins have already been added to it
		- it is safe to add more hal pins because halcomp.ready() has not yet been called at this point.

		after all handlers are instantiated in command line and get_handlers() order, callbacks will be
		connected with connect_signals()/signal_autoconnect()

		The builder may be either of libglade or GtkBuilder type depending on the glade file format.
		'''

		self.halcomp = halcomp
		self.builder = builder

	def on_destroy(self,obj,data=None):
		print("Closed")
		sys.exit(0)

	def on_unix_signal(self,signum,stack_frame):
		print "on_unix_signal(): signal %d received, saving state" % (signum)
		self.ini.save_state(self)

	def on_jog_x_plus_press(self,widget,data=None):
		print "on_button_press called"
	def on_jog_x_minus_press(self,widget,data=None):
		print "on_button_press called"

	def on_jog_y_plus_press(self,widget,data=None):
		print "on_button_press called"
	def on_jog_y_minus_press(self,widget,data=None):
		print "on_button_press called"

	def on_jog_z_plus_press(self,widget,data=None):
		print "on_button_press called"
	def on_jog_z_minus_press(self,widget,data=None):
		print "on_button_press called"

	def on_jog_a_plus_press(self,widget,data=None):
		print "on_button_press called"
	def on_jog_a_minus_press(self,widget,data=None):
		print "on_button_press called"

	def on_jog_u_plus_press(self,widget,data=None):
		print "on_button_press called"
	def on_jog_u_minus_press(self,widget,data=None):
		print "on_button_press called"

	def on_jog_v_plus_press(self,widget,data=None):
		print "on_button_press called"
	def on_jog_v_minus_press(self,widget,data=None):
		print "on_button_press called"


def get_handlers(halcomp,builder,useropts):
	'''
	this function is called by gladevcp at import time (when this module is passed with '-u <modname>.py')

	return a list of object instances whose methods should be connected as callback handlers
	any method whose name does not begin with an underscore ('_') is a  callback candidate

	the 'get_handlers' name is reserved - gladevcp expects it, so do not change
	'''
	return [HandlerClass(halcomp,builder,useropts)]


""")
	file.close()

def create_gladevcp_custom(self,filename):
	file = open(filename, "w")
	print >>file, ("""<?xml version="1.0" encoding="UTF-8"?>
<interface>
  <!-- interface-requires gladevcp 0.0 -->
  <requires lib="gtk+" version="2.16"/>
  <!-- interface-naming-policy project-wide -->
  <object class="GtkWindow" id="window1">
    <property name="can_focus">False</property>
    <child>
    </child>
  </object>
</interface>""")
	file.close()

