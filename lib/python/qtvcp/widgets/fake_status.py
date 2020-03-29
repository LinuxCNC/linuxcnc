#!/usr/bin/python
# -*- encoding: utf-8 -*-
#
#    Copyright 2020 Chris Morley
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
# This program is used to fake the status of linuxcnc in python fot the graphics display
# in qtvcp. Now in designer it shows a fake display of an xyz machine.
# you could probably extends this to update some attributes and fake tool movement on a demo display
#
# most entries are default, a few, for axes and joints, to make an XYZ machine

import linuxcnc
class fakeStatus():
    def __init__(self):

        self.acceleration = 0.0
        self.active_queue = 0
        self.actual_position = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        self.adaptive_feed_enabled = False
        self.ain = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        self.angular_units = 0
        self.aout = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        self.axes = 3
        self.axis = ({'min_position_limit': 0.0, 'velocity': 0.0, 'max_position_limit': 0.0}, {'min_position_limit': 0.0, 'velocity': 0.0, 'max_position_limit': 0.0}, {'min_position_limit': 0.0, 'velocity': 0.0, 'max_position_limit': 0.0}, {'min_position_limit': 0.0, 'velocity': 0.0, 'max_position_limit': 0.0}, {'min_position_limit': 0.0, 'velocity': 0.0, 'max_position_limit': 0.0}, {'min_position_limit': 0.0, 'velocity': 0.0, 'max_position_limit': 0.0}, {'min_position_limit': 0.0, 'velocity': 0.0, 'max_position_limit': 0.0}, {'min_position_limit': 0.0, 'velocity': 0.0, 'max_position_limit': 0.0}, {'min_position_limit': 0.0, 'velocity': 0.0, 'max_position_limit': 0.0})
        self.axis_mask = 7
        self.block_delete = False
        self.call_level = 0
        self.command = ''
        self.current_line = 0
        self.current_vel = 0.0
        self.cycle_time = 0.0
        self.debug = 0
        self.delay_left = 0.0
        self.din = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        self.distance_to_go = 0.0
        self.dout = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        self.dtg = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        self.echo_serial_number = 0
        self.enabled = False
        self.estop = 0
        self.exec_state = 0
        self.feed_hold_enabled = False
        self.feed_override_enabled = False
        self.feedrate = 0.0
        self.file = ''
        self.flood = 0
        self.g5x_index = 0
        self.g5x_offset = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        self.g92_offset = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        self.gcodes = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        self.homed = (0, 0, 0, 0, 0, 0, 0, 0, 0)
        self.id = 0
        self.inpos = False
        self.input_timeout = False
        self.interp_state = 0
        self.interpreter_errcode = 0
        self.joint = ({'ferror_current': 0.0, 'max_position_limit': 0.0, 'max_ferror': 0.0, 'inpos': 0, 'ferror_highmark': 0.0, 'jointType': 0, 'units': 0.0, 'input': 0.0, 'min_soft_limit': 0, 'min_hard_limit': 0, 'homing': 0, 'min_ferror': 0.0, 'max_hard_limit': 0, 'output': 0.0, 'backlash': 0.0, 'fault': 0, 'enabled': 0, 'max_soft_limit': 0, 'override_limits': 0, 'homed': 0, 'min_position_limit': 0.0, 'velocity': 0.0}, {'ferror_current': 0.0, 'max_position_limit': 0.0, 'max_ferror': 0.0, 'inpos': 0, 'ferror_highmark': 0.0, 'jointType': 0, 'units': 0.0, 'input': 0.0, 'min_soft_limit': 0, 'min_hard_limit': 0, 'homing': 0, 'min_ferror': 0.0, 'max_hard_limit': 0, 'output': 0.0, 'backlash': 0.0, 'fault': 0, 'enabled': 0, 'max_soft_limit': 0, 'override_limits': 0, 'homed': 0, 'min_position_limit': 0.0, 'velocity': 0.0}, {'ferror_current': 0.0, 'max_position_limit': 0.0, 'max_ferror': 0.0, 'inpos': 0, 'ferror_highmark': 0.0, 'jointType': 0, 'units': 0.0, 'input': 0.0, 'min_soft_limit': 0, 'min_hard_limit': 0, 'homing': 0, 'min_ferror': 0.0, 'max_hard_limit': 0, 'output': 0.0, 'backlash': 0.0, 'fault': 0, 'enabled': 0, 'max_soft_limit': 0, 'override_limits': 0, 'homed': 0, 'min_position_limit': 0.0, 'velocity': 0.0}, {'ferror_current': 0.0, 'max_position_limit': 0.0, 'max_ferror': 0.0, 'inpos': 0, 'ferror_highmark': 0.0, 'jointType': 0, 'units': 0.0, 'input': 0.0, 'min_soft_limit': 0, 'min_hard_limit': 0, 'homing': 0, 'min_ferror': 0.0, 'max_hard_limit': 0, 'output': 0.0, 'backlash': 0.0, 'fault': 0, 'enabled': 0, 'max_soft_limit': 0, 'override_limits': 0, 'homed': 0, 'min_position_limit': 0.0, 'velocity': 0.0}, {'ferror_current': 0.0, 'max_position_limit': 0.0, 'max_ferror': 0.0, 'inpos': 0, 'ferror_highmark': 0.0, 'jointType': 0, 'units': 0.0, 'input': 0.0, 'min_soft_limit': 0, 'min_hard_limit': 0, 'homing': 0, 'min_ferror': 0.0, 'max_hard_limit': 0, 'output': 0.0, 'backlash': 0.0, 'fault': 0, 'enabled': 0, 'max_soft_limit': 0, 'override_limits': 0, 'homed': 0, 'min_position_limit': 0.0, 'velocity': 0.0}, {'ferror_current': 0.0, 'max_position_limit': 0.0, 'max_ferror': 0.0, 'inpos': 0, 'ferror_highmark': 0.0, 'jointType': 0, 'units': 0.0, 'input': 0.0, 'min_soft_limit': 0, 'min_hard_limit': 0, 'homing': 0, 'min_ferror': 0.0, 'max_hard_limit': 0, 'output': 0.0, 'backlash': 0.0, 'fault': 0, 'enabled': 0, 'max_soft_limit': 0, 'override_limits': 0, 'homed': 0, 'min_position_limit': 0.0, 'velocity': 0.0}, {'ferror_current': 0.0, 'max_position_limit': 0.0, 'max_ferror': 0.0, 'inpos': 0, 'ferror_highmark': 0.0, 'jointType': 0, 'units': 0.0, 'input': 0.0, 'min_soft_limit': 0, 'min_hard_limit': 0, 'homing': 0, 'min_ferror': 0.0, 'max_hard_limit': 0, 'output': 0.0, 'backlash': 0.0, 'fault': 0, 'enabled': 0, 'max_soft_limit': 0, 'override_limits': 0, 'homed': 0, 'min_position_limit': 0.0, 'velocity': 0.0}, {'ferror_current': 0.0, 'max_position_limit': 0.0, 'max_ferror': 0.0, 'inpos': 0, 'ferror_highmark': 0.0, 'jointType': 0, 'units': 0.0, 'input': 0.0, 'min_soft_limit': 0, 'min_hard_limit': 0, 'homing': 0, 'min_ferror': 0.0, 'max_hard_limit': 0, 'output': 0.0, 'backlash': 0.0, 'fault': 0, 'enabled': 0, 'max_soft_limit': 0, 'override_limits': 0, 'homed': 0, 'min_position_limit': 0.0, 'velocity': 0.0}, {'ferror_current': 0.0, 'max_position_limit': 0.0, 'max_ferror': 0.0, 'inpos': 0, 'ferror_highmark': 0.0, 'jointType': 0, 'units': 0.0, 'input': 0.0, 'min_soft_limit': 0, 'min_hard_limit': 0, 'homing': 0, 'min_ferror': 0.0, 'max_hard_limit': 0, 'output': 0.0, 'backlash': 0.0, 'fault': 0, 'enabled': 0, 'max_soft_limit': 0, 'override_limits': 0, 'homed': 0, 'min_position_limit': 0.0, 'velocity': 0.0})
        self.joint_actual_position = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        self.joint_position = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        self.joints = 3
        self.kinematics_type = 1
        self.limit = (0, 0, 0, 0, 0, 0, 0, 0, 0)
        self.linear_units = 0.0
        self.lube = 0
        self.lube_level = 0
        self.max_acceleration = 0.0
        self.max_velocity = 0.0
        self.mcodes = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        self.mist = 0
        self.motion_line = 0
        self.motion_mode = 0
        self.motion_type = 0
        self.optional_stop = False
        self.paused = False
        self.pocket_prepped = 0
        self.position = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        self.probe_tripped = False
        self.probe_val = 0
        self.probed_position = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        self.probing = False
        self.program_units = 0
        self.queue = 0
        self.queue_full = False
        self.queued_mdi_commands = 0
        self.rapidrate = 0.0
        self.read_line = 0
        self.rotation_xy = 0.0
        self.settings = (0.0, 0.0, 0.0)
        self.spindle = ({'direction': 0L, 'orient_state': 0L, 'enabled': 0L, 'override_enabled': False, 'brake': 0L, 'homed': False, 'override': 0.0, 'speed': 0.0, 'orient_fault': 0L}, {'direction': 0L, 'orient_state': 0L, 'enabled': 0L, 'override_enabled': False, 'brake': 0L, 'homed': False, 'override': 0.0, 'speed': 0.0, 'orient_fault': 0L}, {'direction': 0L, 'orient_state': 0L, 'enabled': 0L, 'override_enabled': False, 'brake': 0L, 'homed': False, 'override': 0.0, 'speed': 0.0, 'orient_fault': 0L}, {'direction': 0L, 'orient_state': 0L, 'enabled': 0L, 'override_enabled': False, 'brake': 0L, 'homed': False, 'override': 0.0, 'speed': 0.0, 'orient_fault': 0L}, {'direction': 0L, 'orient_state': 0L, 'enabled': 0L, 'override_enabled': False, 'brake': 0L, 'homed': False, 'override': 0.0, 'speed': 0.0, 'orient_fault': 0L}, {'direction': 0L, 'orient_state': 0L, 'enabled': 0L, 'override_enabled': False, 'brake': 0L, 'homed': False, 'override': 0.0, 'speed': 0.0, 'orient_fault': 0L}, {'direction': 0L, 'orient_state': 0L, 'enabled': 0L, 'override_enabled': False, 'brake': 0L, 'homed': False, 'override': 0.0, 'speed': 0.0, 'orient_fault': 0L}, {'direction': 0L, 'orient_state': 0L, 'enabled': 0L, 'override_enabled': False, 'brake': 0L, 'homed': False, 'override': 0.0, 'speed': 0.0, 'orient_fault': 0L})
        self.spindles = 0
        self.state = 0
        self.task_mode = 0
        self.task_paused = 0
        self.task_state = 0
        self.tool_in_spindle = 0
        self.tool_offset = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        self.tool_table = ()
        self.velocity = 0.0

    def poll(self):
        return True
