#!/usr/bin/env python

from linuxcnccontrol import LinuxcncControl
from linuxcnccontrol import LinuxcncError

import linuxcnc
import hal
import time
import sys
import os


# this is how long we wait for linuxcnc to do our bidding
timeout = 1.0


def introspect(h):
    #print "joint.0.select =", h['joint-0-select']
    #print "joint.0.selected =", h['joint-0-selected']
    #print "joint.0.position =", h['joint-0-position']
    os.system("halcmd show pin halui")
    os.system("halcmd show pin python-ui")
    os.system("halcmd show sig")


def select_joint(name):
    print "    selecting", name

    h[name + '-select'] = 1
    start = time.time()
    while (h[name + '-selected'] == 0) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[name + '-selected'] == 0:
        print "failed to select", name, "in halui"
        introspect(h)
        sys.exit(1)

    h[name + '-select'] = 0


def jog_minus(name, target):
    start_position = h[name + '-position']
    print "    jogging", name, "negative: to %.3f" % (target)

    h['jog-selected-minus'] = 1

    start = time.time()
    while (h[name + '-position'] > target) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[name + '-position'] > target:
        print name, "failed to jog", name, "to", target
        introspect(h)
        return False

    h['jog-selected-minus'] = 0

    print "    jogged %s negative past target %.3f" % (name, target)

    return True


def jog_plus(name, target):
    start_position = h[name + '-position']
    print "    jogging %s positive: to %.3f" % (name, target)

    h['jog-selected-plus'] = 1

    start = time.time()
    while (h[name + '-position'] < target) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[name + '-position'] < target:
        print name, "failed to jog", name, "to", target
        introspect(h)
        return False

    h['jog-selected-plus'] = 0

    print "    jogged %s positive past target %.3f)" % (name, target)

    return True


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
    print "Error: joint didn't stop jogging!"
    sys.exit(1)


def jog_joint(joint_number, target):
    success = True

    joint = []
    for j in range(0,3):
        joint.append(h['joint-%d-position' % j])

    name = 'joint-%d' % joint_number

    print "jogging", name, "to", target
    select_joint(name)

    if h[name + '-position'] > target:
        jog_minus(name, target)
    else:
        jog_plus(name, target)

    for j in range(0,3):
        pin_name = 'joint-%d-position' % j
        if j == joint_number:
            if joint[j] == h[pin_name]:
                print "joint", str(j), "didn't move but should have!"
                success = False
        else:
            if joint[j] != h[pin_name]:
                print "joint", str(j), "moved from %.3f to %.3f but shouldnt have!" % (joint[j], h[pin_name])
                success = False

    wait_for_joint_to_stop(joint_number)

    if not success:
        sys.exit(1)


#
# set up pins
# shell out to halcmd to make nets to halui and motion
#

h = hal.component("python-ui")

h.newpin("joint-0-select", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-0-selected", hal.HAL_BIT, hal.HAL_IN)
h.newpin("joint-0-position", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-1-select", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-1-selected", hal.HAL_BIT, hal.HAL_IN)
h.newpin("joint-1-position", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("joint-2-select", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("joint-2-selected", hal.HAL_BIT, hal.HAL_IN)
h.newpin("joint-2-position", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("jog-selected-minus", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("jog-selected-plus", hal.HAL_BIT, hal.HAL_OUT)

h.ready() # mark the component as 'ready'

os.system("halcmd source ./postgui.hal")


#
# connect to LinuxCNC
#

e = LinuxcncControl()
e.set_state(linuxcnc.STATE_ESTOP_RESET)
e.set_state(linuxcnc.STATE_ON)
e.set_mode(linuxcnc.MODE_MANUAL)


#
# run the test
#
# These jog_joint() functions will exit with a return value of 1 if
# something goes wrong.
#

jog_joint(0, -0.5)
jog_joint(0, 0.0)

jog_joint(1, -0.5)
jog_joint(1, 0.0)

jog_joint(2, -0.5)
jog_joint(2, 0.0)


sys.exit(0)

