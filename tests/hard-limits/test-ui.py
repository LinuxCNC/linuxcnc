#!/usr/bin/env python

import linuxcnc
import hal

import time
import sys
import subprocess
import os
import signal
import glob
import re


def print_status(status):
    status.poll()
    print "status.axis[0]:", status.axis[0]
    print "status.axis[1]:", status.axis[1]
    print "status.joint[0]:", status.joint[0]
    print "status.joint[1]:", status.joint[1]
    print "status.current_vel:", status.current_vel
    print "status.echo_serial_number:", status.echo_serial_number
    print "status.enabled:", status.enabled
    print "status.estop:", status.estop
    print "status.exec_state:", status.exec_state
    print "status.inpos:", status.inpos
    print "status.interp_state:", status.interp_state
    print "status.interpreter_errcode:", status.interpreter_errcode
    print "status.limit:", status.limit
    print "status.motion_mode:", status.motion_mode
    print "status.motion_type:", status.motion_type
    print "status.position:", status.position
    print "status.state:", status.state
    print "status.task_mode:", status.task_mode
    print "status.task_state:", status.task_state
    print "status.velocity:", status.velocity
    sys.stdout.flush()


def assert_wait_complete(command):
    r = command.wait_complete()
    print "wait_complete() returns", r
    assert((r == linuxcnc.RCS_DONE) or (r == linuxcnc.RCS_ERROR))


#
# connect to HAL
#

comp = hal.component("test-ui")
comp.newpin("x-neg-lim-sw", hal.HAL_BIT, hal.HAL_OUT)
comp.ready()

hal.connect('test-ui.x-neg-lim-sw', 'x-neg-lim-sw')


#
# connect to LinuxCNC
#

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


#
# Come out of E-stop, turn the machine on, switch to Manual mode, and home.
#

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MANUAL)
c.home(0)
c.home(1)
c.home(2)
c.wait_complete()

# wait for homing to complete
start_time = time.time()
s.poll()
all_homed = s.homed[0]+s.homed[1]+s.homed[2]
while (all_homed is not 3) and (time.time() - start_time < 5):
    time.sleep(0.100)
    s.poll()
    all_homed = s.homed[0]+s.homed[1]+s.homed[2]

if all_homed is not 3:
    print "failed to home"
    print "s.homed:", s.homed
    sys.exit(1)

c.teleop_enable(0)


#
# run the test: start a jog on X, then trip a limit switch
#

# jog arguments are: (jog_type, joint_flag, axis, velocity)
c.jog(linuxcnc.JOG_CONTINUOUS, 1, 0, -0.1)

# verify that we're starting to move
s.poll()
old_x = s.position[0]
start_time = time.time()
while (old_x == s.position[0]) and (time.time() - start_time < 5):
    time.sleep(0.1)
    s.poll()

if old_x == s.position[0]:
    print "no jog movement"
    sys.exit(1)

print "x started moving (%.6f to %.6f)" % (old_x, s.position[0])
print_status(s)

# verify that Status reflects the situation
assert(s.joint[0]['min_soft_limit'] == False)
assert(s.joint[0]['min_hard_limit'] == False)
assert(s.joint[0]['max_soft_limit'] == False)
assert(s.joint[0]['max_hard_limit'] == False)
assert(s.joint[0]['inpos'] == False)
assert(s.joint[0]['enabled'] == True)

assert(not (1 in s.limit))
assert(s.inpos == False)
assert(s.enabled == True)

# trip the limit switch
comp['x-neg-lim-sw'] = True

# let linuxcnc react to the limit switch
expected_error = 'joint 0 on limit switch error'
start_time = time.time()
while (time.time() - start_time < 5):
    error = e.poll()
    if error != None:
        if error[1] == expected_error:
            break
        else:
            print "linuxcnc sent other error %d: %s" % (error[0], error[1])
    time.sleep(0.1)

if error == None or error[1] != expected_error:
    print "no limit switch error from LinuxCNC"
    sys.exit(1)

print "linuxcnc sent error %d: %s" % (error[0], error[1])
print_status(s)

# verify that we're stopping
s.poll()
start_time = time.time()
while (s.joint[0]['velocity'] != 0.0) and (time.time() - start_time < 5):
    time.sleep(0.1)
    s.poll()

if s.joint[0]['velocity'] != 0.0:
    print "limit switch didn't stop movement"
    sys.exit(1)

print "x stopped moving (pos=%.6f, vel=%.6f)" % (s.position[0], s.joint[0]['velocity'])
print_status(s)

# verify that Status reflects the situation
assert(s.joint[0]['min_soft_limit'] == False)
assert(s.joint[0]['min_hard_limit'] == True)
assert(s.joint[0]['max_soft_limit'] == False)
assert(s.joint[0]['max_hard_limit'] == False)
assert(s.joint[0]['inpos'] == True)
assert(s.joint[0]['enabled'] == False)

assert(s.limit[0] == 1)
assert(s.inpos == True)
assert(s.enabled == False)

# turn the machine back on with Override Limits enabled
c.override_limits()
time.sleep(1)
s.poll()
print_status(s)
print "command.serial:", c.serial
# this fails in 2.6.12 due to the stat RCS message having a status of
# RCS_EXEC...  as if though the override_limits command didnt set status
# back to RCS_DONE when it finished.
# assert_wait_complete(c)

c.state(linuxcnc.STATE_ON)
assert_wait_complete(c)

# verify that Status reflects the new situation
s.poll()
assert(s.joint[0]['min_soft_limit'] == False)
assert(s.joint[0]['min_hard_limit'] == True)
assert(s.joint[0]['max_soft_limit'] == False)
assert(s.joint[0]['max_hard_limit'] == False)
assert(s.joint[0]['inpos'] == True)
assert(s.joint[0]['enabled'] == True)

assert(s.limit[0] == 1)
assert(s.inpos == True)
assert(s.enabled == True)

# jog X in the positive direction, off the negative limit switch
c.jog(linuxcnc.JOG_CONTINUOUS, 1, 0, 1)

# verify that we're starting to move
s.poll()
old_x = s.position[0]
start_time = time.time()
while (old_x == s.position[0]) and (time.time() - start_time < 5):
    time.sleep(0.1)
    s.poll()

if old_x == s.position[0]:
    print "no jog movement"
    sys.exit(1)

print "x started moving (%.6f to %.6f)" % (old_x, s.position[0])
print_status(s)

# un-trip the limit switch
comp['x-neg-lim-sw'] = False

# let linuxcnc react to the limit switch untripping
start_time = time.time()
while (time.time() - start_time < 5):
    s.poll()
    if (s.joint[0]['min_hard_limit'] == False) and (s.limit[0] == 0):
        break
    time.sleep(0.1)

# verify that Status reflects the new situation
assert(s.joint[0]['min_soft_limit'] == False)
assert(s.joint[0]['min_hard_limit'] == False)
assert(s.joint[0]['max_soft_limit'] == False)
assert(s.joint[0]['max_hard_limit'] == False)
assert(s.joint[0]['inpos'] == False)
assert(s.joint[0]['enabled'] == True)

assert(s.limit[0] == 0)
assert(s.inpos == False)
assert(s.enabled == True)

# stop the jog
c.jog(linuxcnc.JOG_STOP, 1, 0)

# verify that we're stopping
s.poll()
old_x = s.position[0]
start_time = time.time()
while (old_x != s.position[0]) and (time.time() - start_time < 5):
    time.sleep(0.1)
    s.poll()

if old_x != s.position[0]:
    print "JOG_STOP didn't stop movement"
    sys.exit(1)

print "x stopped moving (%.6f)" % s.position[0]
print_status(s)

# verify that Status reflects the new situation
assert(s.joint[0]['min_soft_limit'] == False)
assert(s.joint[0]['min_hard_limit'] == False)
assert(s.joint[0]['max_soft_limit'] == False)
assert(s.joint[0]['max_hard_limit'] == False)
# FIXME: another bug
#assert(s.joint[0]['inpos'] == True)
assert(s.joint[0]['enabled'] == True)

assert(s.limit[0] == 0)
# FIXME: another bug
#assert(s.inpos == True)
assert(s.enabled == True)

# success!
sys.exit(0)
