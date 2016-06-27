#!/usr/bin/env python

import linuxcnc
import hal

import time
import sys
import os
import math

# this is how long we wait for linuxcnc to do our bidding
timeout = 5.0


# unbuffer stdout
sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)


def wait_for_axis_to_stop(axis_letter):
    axis_letter = axis_letter.lower()
    axis_index = 'xyzabcuvw'.index(axis_letter)
    print "waiting for axis", axis_letter, "to stop"
    s.poll()
    start_time = time.time()
    timeout = 2.0
    prev_pos = s.position[axis_index]
    while (time.time() - start_time) < timeout:
        time.sleep(0.1)
        s.poll()
        new_pos = s.position[axis_index]
        if new_pos == prev_pos:
            return
        prev_pos = new_pos
    print "Error: axis %s didn't stop jogging!" % axis_letter
    print "axis %s is at %.3f %.3f seconds after reaching target (prev_pos=%.3f)" % (axis_letter, s.position[axis_index], timeout, prev_pos)
    sys.exit(1)

def jog_axis(axis_letter, target):
    s.poll()
    axis_letter = axis_letter.lower()
    axis_index = 'xyzabcuvw'.index(axis_letter)
    start_pos = s.position

    print "jogging axis %s from %.3f to %.3f" % (axis_letter, start_pos[axis_index], target)

    if s.position[axis_index] < target:
        vel = 5
        done = lambda pos: pos > target
    else:
        vel = -5
        done = lambda pos: pos < target

    # the 0 here means "jog axis, not joint"
    c.jog(linuxcnc.JOG_CONTINUOUS, 0, axis_index, vel)

    start = time.time()
    while not done(s.position[axis_index]) and ((time.time() - start) < timeout):
        time.sleep(0.1)
        s.poll()

    # the 0 here means "jog axis, not joint"
    c.jog(linuxcnc.JOG_STOP, 0, axis_index)

    if not done(s.position[axis_index]):
        print "failed to jog axis %s to %.3f" % (axis_letter, target)
        print "timed out at %.3f after %.3f seconds" % (s.position[axis_index], timeout)
        sys.exit(1)

    print "    jogged axis %d past target %.3f" % (axis_index, target)

    wait_for_axis_to_stop(axis_letter);

    success = True
    for i in range(0, 9):
        if i == axis_index:
            continue;
        if start_pos[i] != s.position[i]:
            print "axis %s moved from %.3f to %.3f but shouldnt have!" % ('xyzabcuvw'[i], start_pos[i], s.position[i])
            success = False
    if not success:
        sys.exit(1)


def all_joints_homed(joints):
    s.poll()
    for i in range(0,9):
        if joints[i] and not s.homed[i]:
            return False
    return True


def wait_for_home(joints):
    print "homing..."
    start_time = time.time()
    timeout = 10.0
    while (time.time() - start_time) < timeout:
        if all_joints_homed(joints):
            return
        time.sleep(0.1)

    s.poll()
    print "timeout waiting for homing to complete"
    print "s.homed:", s.homed
    print "s.position:", s.position
    sys.exit(1)


def wait_for_axis_to_stop_at(axis_letter, target):
    axis_letter = axis_letter.lower()
    axis_index = 'xyzabcuvw'.index(axis_letter)
    timeout = 10.0
    tolerance = 0.0001

    s.poll()
    start = time.time()

    while ((time.time() - start) < timeout):
        prev_pos = s.position[axis_index]
        s.poll()
        vel = s.position[axis_index] - prev_pos
        error = math.fabs(s.position[axis_index] - target)
        if (error < tolerance) and (vel == 0):
            print "axis %s stopped at %.3f" % (axis_letter, target)
            return
        time.sleep(0.1)
    print "timeout waiting for axis %s to stop at %.3f (pos=%.3f, vel=%.3f)" % (axis_letter, target, s.position[axis_index], vel)
    sys.exit(1)



#
# connect to LinuxCNC
#

c = linuxcnc.command()
e = linuxcnc.error_channel()
s = linuxcnc.stat()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MANUAL)   
c.home(-1)   
wait_for_home(joints=[1,0,1,0,0,0,0,0,0])


#
# do some jogs in manual mode
#

jog_axis('x', -0.5)
jog_axis('x', 0.0)

jog_axis('z', -0.5)
jog_axis('z', 0.0)


#
# do some MDI commands
#

c.mode(linuxcnc.MODE_MDI)

gcode = 'g0 x2 z2'
print "running gcode:", gcode
c.mdi(gcode)
c.wait_complete()
wait_for_axis_to_stop_at('x', 2);
wait_for_axis_to_stop_at('z', 2);

gcode = 'g0 x-2'
print "running gcode:", gcode
c.mdi(gcode)
c.wait_complete()
wait_for_axis_to_stop_at('x', -2);
wait_for_axis_to_stop_at('z', 2);

gcode = 'g0 z-2'
print "running gcode:", gcode
c.mdi(gcode)
c.wait_complete()
wait_for_axis_to_stop_at('x', -2);
wait_for_axis_to_stop_at('z', -2);

sys.exit(0)

