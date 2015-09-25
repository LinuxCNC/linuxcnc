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


def introspect(h):
    os.system("halcmd show pin python-ui")
    os.system("halcmd show sig")


def wait_for_joint_to_stop(joint_number):
    print "waiting for joint", joint_number, "to stop"
    s.poll()
    start_time = time.time()
    timeout = 2.0
    prev_pos = s.position[joint_number]
    while (time.time() - start_time) < timeout:
        time.sleep(0.1)
        s.poll()
        new_pos = s.position[joint_number]
        if new_pos == prev_pos:
            return
        prev_pos = new_pos
    print "Error: joint", joint_number, "didn't stop jogging!"
    print "joint %d is at %.3f %.3f seconds after reaching target (prev_pos=%.3f)" % (joint_number, s.position[joint_number], timeout, prev_pos)
    sys.exit(1)


def jog_joint(joint_number, target):
    s.poll()
    start_pos = s.position

    print "jogging joint %d from %.3f to %.3f" % (joint_number, start_pos[joint_number], target)

    if s.position[joint_number] < target:
        vel = 5
        done = lambda pos: pos > target
    else:
        vel = -5
        done = lambda pos: pos < target

    c.jog(linuxcnc.JOG_CONTINUOUS, joint_number, vel)

    start = time.time()
    while not done(s.position[joint_number]) and ((time.time() - start) < timeout):
        time.sleep(0.1)
        s.poll()

    c.jog(linuxcnc.JOG_STOP, joint_number)

    if not done(s.position[joint_number]):
        print "failed to jog joint %d to %.3f" % (joint_number, target)
        print "timed out at %.3f after %.3f seconds" % (s.position[joint_number], timeout)
        introspect(h)
        sys.exit(1)

    print "    jogged joint %d past target %.3f" % (joint_number, target)

    wait_for_joint_to_stop(joint_number);

    success = True
    for i in range(0, 9):
        if i == joint_number:
            continue;
        if start_pos[i] != s.position[i]:
            print "joint %d moved from %.3f to %.3f but shouldnt have!" % (i, start_pos[i], s.position[i])
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


def wait_for_joint_to_stop_at(joint, target):
    timeout = 10.0
    tolerance = 0.0001

    s.poll()
    start = time.time()

    while ((time.time() - start) < timeout):
        prev_pos = s.position[joint]
        s.poll()
        vel = s.position[joint] - prev_pos
        error = math.fabs(s.position[joint] - target)
        if (error < tolerance) and (vel == 0):
            print "joint %d stopped at %.3f" % (joint, target)
            return
        time.sleep(0.1)
    print "timeout waiting for joint %d to stop at %.3f (pos=%.3f, vel=%.3f)" % (joint, target, s.position[joint], vel)
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

jog_joint(0, -0.5)
jog_joint(0, 0.0)

jog_joint(2, -0.5)
jog_joint(2, 0.0)


#
# do some MDI commands
#

c.mode(linuxcnc.MODE_MDI)

gcode = 'g0 x2 z2'
print "running gcode:", gcode
c.mdi(gcode)
c.wait_complete()
wait_for_joint_to_stop_at(0, 2);
wait_for_joint_to_stop_at(2, 2);

gcode = 'g0 x-2'
print "running gcode:", gcode
c.mdi(gcode)
c.wait_complete()
wait_for_joint_to_stop_at(0, -2);
wait_for_joint_to_stop_at(2, 2);

gcode = 'g0 z-2'
print "running gcode:", gcode
c.mdi(gcode)
c.wait_complete()
wait_for_joint_to_stop_at(0, -2);
wait_for_joint_to_stop_at(2, -2);

sys.exit(0)

