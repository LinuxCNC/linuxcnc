#!/usr/bin/env python3

import linuxcnc
import hal

import math
import time
import sys
import subprocess
import os
import signal
import glob
import re

def wait_for_linuxcnc_startup(status, timeout=10.0):

    """Poll the Status buffer waiting for it to look initialized,
    rather than just allocated (all-zero).  Returns on success, throws
    RuntimeError on failure."""

    start_time = time.time()
    while time.time() - start_time < timeout:
        status.poll()
        if (status.angular_units == 0.0) \
            or (status.axis_mask == 0) \
            or (status.cycle_time == 0.0) \
            or (status.exec_state != linuxcnc.EXEC_DONE) \
            or (status.interp_state != linuxcnc.INTERP_IDLE) \
            or (status.inpos == False) \
            or (status.linear_units == 0.0) \
            or (status.max_acceleration == 0.0) \
            or (status.max_velocity == 0.0) \
            or (status.program_units == 0.0) \
            or (status.rapidrate == 0.0) \
            or (status.state != linuxcnc.RCS_DONE) \
            or (status.task_state != linuxcnc.STATE_ESTOP):
            time.sleep(0.1)
        else:
            # looks good
            return

    # timeout, throw an exception
    raise RuntimeError("Timeout")


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()
h = hal.component("dummy")
h.ready()

# Wait for LinuxCNC to initialize itself so the Status buffer stabilizes.
wait_for_linuxcnc_startup(s)

# Because the kinematics is non-trivial, a homing is needed.
# HOME_ABSOLUTE_ENCODER = 1 is used in the ini file.
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(0)
c.home(1)
c.home(2)
c.wait_complete()

def absdelta(a, b):
    '''Maximum absolute difference between components of two coordinate points'''
    return max(abs(a[i] - b[i]) for i in range(len(a)))

def test_coordinate(target, expected):
    '''Command LinuxCNC to go to 'target' position,
    and check that actual joint positions given by inverse kinematics
    match 'expected'. Also check that feedback position given by forward
    kinematics matches the 'target' point.
    '''
    c.mode(linuxcnc.MODE_MDI)
    c.mdi('G0 X%0.9f Y%0.9f Z%0.9f F1000' % target)
    c.wait_complete()

    # Read out resulting joint positions to verify kinematics is working correctly.
    # The delay seems to be needed for trajectory to fully settle, otherwise there is
    # about 1e-5 error between target and actual position.
    time.sleep(0.05)
    s.poll()
    joints = s.joint_position[:3]
    joints_feedback = s.joint_actual_position[:3]
    coord_feedback = s.actual_position[:3]

    print("Target location %s, joints %s, feedback %s" % (target, joints, coord_feedback))

    # Accuracy limit is set to 1e-9 here.
    # For practical purposes, a numerical accuracy of 1e-6 would be perfectly acceptable.
    # Current implementation using doubles achieves about 1e-14 precision.
    maxdelta = 1e-9

    if absdelta(joints, expected) > maxdelta:
        raise RuntimeError("Error in inverse kinematics: coordinates %s should give joint positions %s, got %s" %
                           (target, expected, joints))

    if absdelta(coord_feedback, target) > maxdelta:
        raise RuntimeError("Error in forward kinematics: coordinates %s, feedback joints %s and coord %s" %
                           (target, joints_feedback, coord_feedback))

# Test the default identity transform
test_coordinate((0, 0, 0), (0, 0, 0))
test_coordinate((10, 20, 30), (10, 20, 30))
test_coordinate((11, 12, 13), (11, 12, 13))

# Test with axis scaling
hal.set_p('matrixkins.C_xx', "0.99")
hal.set_p('matrixkins.C_yy', "1.01")
hal.set_p('matrixkins.C_zz', "1.05")
test_coordinate((0, 0, 0), (0, 0, 0))
test_coordinate((10, 20, 30), (10 * 0.99, 20 * 1.01, 30 * 1.05))
test_coordinate((11, 12, 13), (11 * 0.99, 12 * 1.01, 13 * 1.05))
hal.set_p('matrixkins.C_xx', "1.00")
hal.set_p('matrixkins.C_yy', "1.00")
hal.set_p('matrixkins.C_zz', "1.00")

# Test with X axis skews
hal.set_p('matrixkins.C_xy', "0.01")
hal.set_p('matrixkins.C_xz', "0.03")
test_coordinate((0, 0, 0), (0, 0, 0))
test_coordinate((10, 20, 30), (10 + 20 * 0.01 + 30 * 0.03, 20, 30))
test_coordinate((11, 12, 13), (11 + 12 * 0.01 + 13 * 0.03, 12, 13))
hal.set_p('matrixkins.C_xy', "0.0")
hal.set_p('matrixkins.C_xz', "0.0")

# Test with Y axis skews
hal.set_p('matrixkins.C_yx', "0.01")
hal.set_p('matrixkins.C_yz', "0.03")
test_coordinate((0, 0, 0), (0, 0, 0))
test_coordinate((10, 20, 30), (10, 20 + 10 * 0.01 + 30 * 0.03, 30))
test_coordinate((11, 12, 13), (11, 12 + 11 * 0.01 + 13 * 0.03, 13))
hal.set_p('matrixkins.C_yx', "0.0")
hal.set_p('matrixkins.C_yz', "0.0")

# Test with Z axis skews
hal.set_p('matrixkins.C_zx', "0.01")
hal.set_p('matrixkins.C_zy', "0.03")
test_coordinate((0, 0, 0), (0, 0, 0))
test_coordinate((10, 20, 30), (10, 20, 30 + 0.01 * 10 + 0.03 * 20))
test_coordinate((11, 12, 13), (11, 12, 13 + 0.01 * 11 + 0.03 * 12))
hal.set_p('matrixkins.C_zx', "0.0")
hal.set_p('matrixkins.C_zy', "0.0")

sys.exit(0)

