#!/usr/bin/env python3

import linuxcnc
import linuxcnc_util
import hal
import time
import sys
import os
import math


def close_enough(a, b, epsilon=0.000001):
    return math.fabs(a - b) < epsilon


def jog_axis(axis_letter, counts=1, scale=0.001):
    timeout = 5.0

    start_pos = { }
    for a in 'xyz':
        start_pos[a] = h['axis-%c-position' % a]

    target = h['axis-%c-position' % axis_letter] + (counts * scale)

    h['axis-%c-jog-scale' % axis_letter] = scale
    h['axis-%c-jog-enable' % axis_letter] = 1
    h['axis-%c-jog-counts' % axis_letter] += counts

    start_time = time.time()
    while not close_enough(h['axis-%c-position' % axis_letter], target) and (time.time() - start_time < timeout):
        #print "axis %c is at %.9f" % (axis_letter, h['axis-%c-position' % axis_letter])
        time.sleep(0.010)

    h['axis-%c-jog-enable' % axis_letter] = 0

    print("axis %c jogged from %.6f to %.6f (%d counts at scale %.6f)" % (axis_letter, start_pos[axis_letter], h['axis-%c-position' % axis_letter], counts, scale))

    success = True
    for a in 'xyz':
        pin_name = 'axis-%c-position' % a
        if a == axis_letter:
            if not close_enough(h[pin_name], target):
                print("axis %c didn't get to target (start=%.6f, target=%.6f, got to %.6f)" % (axis_letter, start_pos[axis_letter], target, h['axis-%c-position' % axis_letter]))
                success = False
        else:
            if h[pin_name] != start_pos[a]:
                print("axis %c moved from %.6f to %.6f but should not have!" % (a, start_pos[a], h[pin_name]))
                success = False

    l.wait_for_axis_to_stop(axis_letter)

    if not success:
        sys.exit(1)


#
# set up pins
# shell out to halcmd to make nets to halui and motion
#

h = hal.component("test-ui")

h.newpin("axis-x-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("axis-x-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("axis-x-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("axis-x-position", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("axis-y-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("axis-y-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("axis-y-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("axis-y-position", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("axis-z-jog-enable", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("axis-z-jog-counts", hal.HAL_S32, hal.HAL_OUT)
h.newpin("axis-z-jog-scale", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("axis-z-position", hal.HAL_FLOAT, hal.HAL_IN)

h.ready() # mark the component as 'ready'

os.system("halcmd source ./postgui.hal")


#
# connect to LinuxCNC
#

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)


# must home to use Teleop mode

c.home(-1)
c.wait_complete()

l.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])

c.mode(linuxcnc.MODE_MANUAL)


#
# run the test
#
# These jog_joint() functions will exit with a return value of 1 if
# something goes wrong.
#

jog_axis('x', counts=1, scale=0.001)
jog_axis('y', counts=10, scale=-0.025)
jog_axis('z', counts=-100, scale=0.100)

sys.exit(0)

