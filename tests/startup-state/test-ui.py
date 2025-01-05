#!/usr/bin/env python3

import linuxcnc
import linuxcnc_util
import hal

import math
import time
import sys
import subprocess
import os
import signal
import glob
import re


def print_status(status):

    """Prints all the fields of the status buffer.  Does not call
    poll()."""

    print("acceleration: {}".format(status.acceleration))
    print("active_queue: {}".format(status.active_queue))
    print("actual_position: {}".format(status.actual_position))
    print("adaptive_feed_enabled: {}".format(status.adaptive_feed_enabled))
    print("ain: {}".format(status.ain))
    print("angular_units: {}".format(status.angular_units))
    print("aout: {}".format(status.aout))
    print("axis_mask: {}".format(status.axis_mask))
    print("block_delete: {}".format(status.block_delete))
    print("call_level: {}".format(status.call_level))
    print("command: {}".format(status.command))
    print("current_line: {}".format(status.current_line))
    print("current_vel: {}".format(status.current_vel))
    print("cycle_time: {}".format(status.cycle_time))
    print("debug: {}".format(status.debug))
    print("delay_left: {}".format(status.delay_left))
    print("din: {}".format(status.din))
    print("distance_to_go: {}".format(status.distance_to_go))
    print("dout: {}".format(status.dout))
    print("dtg: {}".format(status.dtg))
    print("echo_serial_number: {}".format(status.echo_serial_number))
    print("enabled: {}".format(status.enabled))
    print("estop: {}".format(status.estop))
    print("exec_state: {}".format(status.exec_state))
    print("feed_hold_enabled: {}".format(status.feed_hold_enabled))
    print("feed_override_enabled: {}".format(status.feed_override_enabled))
    print("feedrate: {}".format(status.feedrate))
    print("file: {}".format(status.file))
    print("flood: {}".format(status.flood))
    print("g5x_index: {}".format(status.g5x_index))
    print("g5x_offset: {}".format(status.g5x_offset))
    print("g92_offset: {}".format(status.g92_offset))
    print("gcodes: {}".format(status.gcodes))
    print("homed: {}".format(status.homed))
    print("id: {}".format(status.motion_id))
    print("inpos: {}".format(status.inpos))
    print("input_timeout: {}".format(status.input_timeout))
    print("interp_state: {}".format(status.interp_state))
    print("interpreter_errcode: {}".format(status.interpreter_errcode))
    print("joint: {}".format(status.joint))
    print("joint_actual_position: {}".format(status.joint_actual_position))
    print("joint_position: {}".format(status.joint_position))
    print("joints: {}".format(status.joints))
    print("kinematics_type: {}".format(status.kinematics_type))
    print("limit: {}".format(status.limit))
    print("linear_units: {}".format(status.linear_units))
    print("max_acceleration: {}".format(status.max_acceleration))
    print("max_velocity: {}".format(status.max_velocity))
    print("mcodes: {}".format(status.mcodes))
    print("mist: {}".format(status.mist))
    print("motion_line: {}".format(status.motion_line))
    print("motion_mode: {}".format(status.motion_mode))
    print("motion_type: {}".format(status.motion_type))
    print("optional_stop: {}".format(status.optional_stop))
    print("paused: {}".format(status.paused))
    print("pocket_prepped: {}".format(status.pocket_prepped))
    print("position: {}".format(status.position))
    print("probe_tripped: {}".format(status.probe_tripped))
    print("probe_val: {}".format(status.probe_val))
    print("probed_position: {}".format(status.probed_position))
    print("probing: {}".format(status.probing))
    print("program_units: {}".format(status.program_units))
    print("queue: {}".format(status.queue))
    print("queue_full: {}".format(status.queue_full))
    print("queued_mdi_commands: {}".format(status.queued_mdi_commands))
    print("rapidrate: {}".format(status.rapidrate))
    print("read_line: {}".format(status.read_line))
    print("rotation_xy: {}".format(status.rotation_xy))
    print("settings: {}".format(status.settings))
    print("spindle_brake: {}".format(status.spindle[0]['brake']))
    print("spindle_direction: {}".format(status.spindle[0]['direction']))
    print("spindle_enabled: {}".format(status.spindle[0]['enabled']))
    print("spindle_override_enabled: {}".format(status.spindle[0]['override_enabled']))
    print("spindle_speed: {}".format(status.spindle[0]['speed']))
    print("spindlerate: {}".format(status.spindle[0]['override']))
    print("state: {}".format(status.state))
    print("task_mode: {}".format(status.task_mode))
    print("task_paused: {}".format(status.task_paused))
    print("task_state: {}".format(status.task_state))
    print("tool_in_spindle: {}".format(status.tool_in_spindle))
    print("tool_offset: {}".format(status.tool_offset))
    print("tool_table: {}".format(status.tool_table))
    print("velocity: {}".format(status.velocity))
    sys.stdout.flush()


def assert_joint_zeroed(joint):
    assert(joint['ferror_current'] == 0.0)
    assert(joint['max_position_limit'] == 1.0)
    assert(joint['max_ferror'] == 1.0)
    assert(joint['inpos'] == 1)
    assert(joint['ferror_highmark'] == 0.0)
    assert(joint['jointType'] == 1)
    assert(joint['units'] == 1.0)
    assert(joint['input'] == 0.0)
    assert(joint['min_soft_limit'] == 0)
    assert(joint['min_hard_limit'] == 0)
    assert(joint['homing'] == 0)
    assert(joint['min_ferror'] == 1.0)
    assert(joint['max_hard_limit'] == 0)
    assert(joint['output'] == 0.0)
    assert(joint['backlash'] == 0.0)
    assert(joint['fault'] == 0)
    assert(joint['enabled'] == 0)
    assert(joint['max_soft_limit'] == 0)
    assert(joint['override_limits'] == 0)
    assert(joint['homed'] == 0)
    assert(joint['min_position_limit'] == -1.0)
    assert(joint['velocity'] == 0.0)


def assert_axis_zeroed(axis):
    assert(axis['min_position_limit'] == 0.0)
    assert(axis['velocity'] == 0.0)
    assert(axis['max_position_limit'] == 0.0)


def assert_joint_initialized(joint):
    assert(joint['ferror_current'] == 0.0)
    assert(joint['max_position_limit'] == 40.0)
    assert(joint['max_ferror'] == 0.05)
    assert(joint['inpos'] == 1)
    assert(joint['ferror_highmark'] == 0.0)
    assert(joint['jointType'] == 1)
    assert(joint['units'] == 0.03937007874015748)
    assert(joint['input'] == 0.0)
    assert(joint['min_soft_limit'] == 0)
    assert(joint['min_hard_limit'] == 0)
    assert(joint['homing'] == 0)
    assert(joint['min_ferror'] == 0.01)
    assert(joint['max_hard_limit'] == 0)
    assert(joint['output'] == 0.0)
    assert(joint['backlash'] == 0.0)
    assert(joint['fault'] == 0)
    assert(joint['enabled'] == 0)
    assert(joint['max_soft_limit'] == 0)
    assert(joint['override_limits'] == 0)
    assert(joint['homed'] == 0)
    assert(joint['min_position_limit'] == -40.0)
    assert(joint['velocity'] == 0.0)


def assert_axis_initialized(axis):
    assert(axis['min_position_limit'] == -40.0)
    assert(axis['velocity'] == 0.0)
    assert(axis['max_position_limit'] == 40.0)


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

l = linuxcnc_util.LinuxCNC()
# Wait for LinuxCNC to initialize itself so the Status buffer stabilizes.
l.wait_for_linuxcnc_startup()

print("status at boot up")
s.poll()
print_status(s)

# FIXME: in 2.6.12, acceleration at startup is initialized to 1e99 for some reason
#assert(s.acceleration == 0.0)

assert(s.active_queue == 0)
assert(s.actual_position == (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))
assert(s.adaptive_feed_enabled == False)
assert(s.ain == (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))
assert(s.angular_units == 1.0)
assert(s.aout == (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))
assert_axis_initialized(s.axis[0])
assert_axis_initialized(s.axis[1])
assert_axis_initialized(s.axis[2])
assert_axis_zeroed(s.axis[3])
assert_axis_zeroed(s.axis[4])
assert_axis_zeroed(s.axis[5])
assert_axis_zeroed(s.axis[6])
assert_axis_zeroed(s.axis[7])
assert_axis_zeroed(s.axis[8])
assert(s.axis_mask == 0x7)

assert(s.block_delete == True)

assert(s.command == "")
assert(s.current_line == 0)
assert(s.current_vel == 0.0)
assert(s.cycle_time > 0.0)

assert(s.debug == 0)
assert(s.delay_left == 0.0)
assert(s.din == (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
assert(s.distance_to_go == 0.0)
assert(s.dout == (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
assert(s.dtg == (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))

assert(s.echo_serial_number == 0)
assert(s.enabled == False)
assert(s.estop == 1)
assert(s.exec_state == linuxcnc.EXEC_DONE)

assert(s.feed_hold_enabled == True)
assert(s.feed_override_enabled == True)
assert(s.feedrate == 1.0)
assert(s.file == "")
assert(s.flood == 0)

assert(s.g5x_index == 1)

# mmm, floats
assert(math.fabs(s.g5x_offset[0] - 0.125) < 0.0000001)
assert(math.fabs(s.g5x_offset[1] - 0.250) < 0.0000001)
assert(math.fabs(s.g5x_offset[2] - 0.500) < 0.0000001)
assert(s.g5x_offset[3:] == (0.0, 0.0, 0.0, 0.0, 0.0, 0.0))

assert(math.fabs(s.g92_offset[0] - 1.000) < 0.0000001)
assert(math.fabs(s.g92_offset[1] - 2.000) < 0.0000001)
assert(math.fabs(s.g92_offset[2] - 4.000) < 0.0000001)
assert(s.g92_offset[3:] == (0.0, 0.0, 0.0, 0.0, 0.0, 0.0))

assert(s.gcodes == (0, 800, -1, 170, 400, 200, 900, 940, 540, 490, 990, 640, -1, 970, 911, 80, 923))

assert(not (1 in s.homed))
assert(s.motion_id == 0)
assert(s.inpos == True)
assert(s.input_timeout == False)
assert(s.interp_state == linuxcnc.INTERP_IDLE)
assert(s.interpreter_errcode == 0)

assert_joint_initialized(s.joint[0])
assert_joint_initialized(s.joint[1])
assert_joint_initialized(s.joint[2])
assert_joint_zeroed(s.joint[3])
assert_joint_zeroed(s.joint[4])
assert_joint_zeroed(s.joint[5])
assert_joint_zeroed(s.joint[6])
assert_joint_zeroed(s.joint[7])
assert_joint_zeroed(s.joint[8])
for i in range(len(s.joint_actual_position)):
    assert(s.joint_actual_position[i] == 0.0)
for i in range(len(s.joint_position)):
    assert(s.joint_position[i] == 0.0)
assert(s.joints == 3)

assert(s.kinematics_type == 1)

assert(not (1 in s.limit))
assert(math.fabs(s.linear_units - 0.0393700787402) < 0.0000001)

# FIXME: in master (663c914) this is initialized to 1e99
#assert(math.fabs(s.max_acceleration - 123.45) < 0.0000001)

assert(math.fabs(s.max_velocity - 45.67) < 0.0000001)
assert(s.mcodes == (0, -1, 5, -1, 9, -1, 48, -1, 53, -1))
assert(s.mist == 0)
assert(s.motion_line == 0)
assert(s.motion_mode == linuxcnc.TRAJ_MODE_FREE)
assert(s.motion_type == 0)

assert(s.optional_stop == True)

assert(s.paused == False)
assert(s.pocket_prepped == -1)
assert(s.position == (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))
assert(s.probe_tripped == False)
assert(s.probe_val == 0)
assert(s.probed_position == (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))
assert(s.probing == False)
assert(s.program_units == 1)

assert(s.queue == 0)
assert(s.queue_full == False)
assert(s.queued_mdi_commands == 0)

assert(s.rapidrate == 1.0)
assert(s.read_line == 0)
assert(s.rotation_xy == 0.0)

assert(s.settings == (0.0, 0.0, 0.0, 0.001, 0.0))
assert(s.spindle[0]['brake'] == 1)
assert(s.spindle[0]['direction'] == 0)
assert(s.spindle[0]['enabled'] == 0)
assert(s.spindle[0]['override_enabled'] == True)
assert(s.spindle[0]['speed'] == 0.0)
assert(s.spindle[0]['override'] == 1.0)

assert(s.state == linuxcnc.STATE_ESTOP)

assert(s.task_mode == linuxcnc.MODE_MANUAL)
assert(s.task_paused == 0)
assert(s.task_state == linuxcnc.STATE_ESTOP)
assert(s.tool_in_spindle == 0)
assert(s.tool_offset == (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0))

assert(s.velocity == 0.0)

sys.exit(0)

