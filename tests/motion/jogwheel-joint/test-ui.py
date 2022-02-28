#!/usr/bin/env python3

import linuxcnc
import hal
import time
import sys
import os
import math


def wait_for_joint_to_stop(joint_number):
    pos_pin = 'joint-%d-position' % joint_number
    start_time = time.time()
    timeout = 2.0
    prev_pos = h[pos_pin]
    while (time.time() - start_time) < timeout:
        time.sleep(0.1)
        new_pos = h[pos_pin]
        if new_pos == prev_pos:
            return
        prev_pos = new_pos
    print("Error: joint didn't stop jogging!")
    print("joint %d is at %.6f %.6f seconds after reaching target (prev_pos=%.6f)" % (joint_number, h[pos_pin], timeout, prev_pos))
    sys.exit(1)


def close_enough(a, b, epsilon=0.000001):
    return math.fabs(a - b) < epsilon


def jog_joint(joint_number, counts=1, scale=0.001):
    timeout = 5.0

    start_pos = 3*[0]
    for j in range(0,3):
        start_pos[j] = h['joint-%d-position' % j]

    target = h['joint-%d-position' % joint_number] + (counts * scale)

    h['joint-%d-jog-scale' % joint_number] = scale
    h['joint-%d-jog-enable' % joint_number] = 1
    h['joint-%d-jog-counts' % joint_number] += counts

    start_time = time.time()
    while not close_enough(h['joint-%d-position' % joint_number], target) and (time.time() - start_time < timeout):
        #print "joint %d is at %.9f" % (joint_number, h['joint-%d-position' % joint_number])
        time.sleep(0.010)

    h['joint-%d-jog-enable' % joint_number] = 0

    print("joint jogged from %.6f to %.6f (%d counts at scale %.6f)" % (start_pos[joint_number], h['joint-%d-position' % joint_number], counts, scale))

    success = True
    for j in range(0,3):
        pin_name = 'joint-%d-position' % j
        if j == joint_number:
            if not close_enough(h[pin_name], target):
                print("joint %d didn't get to target (start=%.6f, target=%.6f, got to %.6f)" % (joint_number, start_pos[joint_number], target, h['joint-%d-position' % joint_number]))
                success = False
        else:
            if h[pin_name] != start_pos[j]:
                print("joint %d moved from %.6f to %.6f but should not have!" % (j, start_pos[j], h[pin_name]))
                success = False

    wait_for_joint_to_stop(joint_number)

    if not success:
        sys.exit(1)


#
# set up pins
# shell out to halcmd to make nets to halui and motion
#

h = hal.component("test-ui")

h.newpin("joint-0-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-0-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("joint-0-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("joint-0-position", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-1-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-1-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("joint-1-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("joint-1-position", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-2-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-2-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("joint-2-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("joint-2-position", hal.HAL_FLOAT, hal.HAL_IN)

h.ready() # mark the component as 'ready'

os.system("halcmd source ./postgui.hal")


#
# connect to LinuxCNC
#

c = linuxcnc.command()
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MANUAL)


#
# run the test
#
# These jog_joint() functions will exit with a return value of 1 if
# something goes wrong.
#

jog_joint(0, counts=1, scale=0.001)
jog_joint(1, counts=10, scale=-0.025)
jog_joint(2, counts=-100, scale=0.100)

sys.exit(0)

