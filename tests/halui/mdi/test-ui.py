#!/usr/bin/env python3

import linuxcnc
import hal
import time
import sys
import os
import math

program_start = time.time()

def log(msg):
    delta_t = time.time() - program_start;
    print("%.3f: %s" % (delta_t, msg))
    sys.stdout.flush()


def introspect():
    os.system("halcmd show pin halui")
    os.system("halcmd show pin python-ui")
    os.system("halcmd show sig")


def wait_for_joint_to_stop_at(joint, target):
    timeout = 10.0
    tolerance = 0.0001

    start = time.time()

    curr_pos = 0;
    while ((time.time() - start) < timeout):
        prev_pos = curr_pos
        curr_pos = h['joint-%d-position' % joint]
        vel = curr_pos - prev_pos
        error = math.fabs(curr_pos - target)
        if (error < tolerance) and (vel == 0):
            log("joint %d stopped at %.3f" % (joint, target))
            return
        time.sleep(0.1)
    log("timeout waiting for joint %d to stop at %.3f (pos=%.3f, vel=%.3f)" % (joint, target, curr_pos, vel))
    sys.exit(1)


def wait_for_task_mode(target):
    timeout = 10.0
    start = time.time()

    while ((time.time() - start) < timeout):
        s.poll()
        if s.task_mode == target:
            return
        time.sleep(0.1)

    log("timeout waiting for task mode to get to  %d (it's %d)" % (target, s.task_mode))
    sys.exit(1)


def wait_for_halui_mode(pin_name):
    timeout = 10.0

    start = time.time()

    while ((time.time() - start) < timeout):
        if h[pin_name]:
            print("halui reports mode {}".format(pin_name))
            return
        time.sleep(0.1)
    print("timeout waiting for halui to report mode {}".format(pin_name))
    sys.exit(1)



#
# set up pins
# shell out to halcmd to make nets to halui and motion
#

h = hal.component("python-ui")

h.newpin("mdi-0", hal.Type.BOOL, hal.Dir.OUT)
h.newpin("mdi-1", hal.Type.BOOL, hal.Dir.OUT)
h.newpin("mdi-2", hal.Type.BOOL, hal.Dir.OUT)
h.newpin("mdi-3", hal.Type.BOOL, hal.Dir.OUT)

h.newpin("joint-0-position", hal.Type.REAL, hal.Dir.IN)
h.newpin("joint-1-position", hal.Type.REAL, hal.Dir.IN)
h.newpin("joint-2-position", hal.Type.REAL, hal.Dir.IN)

h.newpin("is-manual", hal.Type.BOOL, hal.Dir.IN)
h.newpin("is-auto", hal.Type.BOOL, hal.Dir.IN)
h.newpin("is-mdi", hal.Type.BOOL, hal.Dir.IN)

h.ready() # mark the component as 'ready'

os.system("halcmd source ./postgui.hal")


#
# connect to LinuxCNC
#

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete()


#
# run the test
#
# These functions will exit with a return value of 1 if something goes
# wrong.
#

# The task mode cannot be set: halui's MDI commands run from whatever mode
# a UI asked for, and task is back in Manual (idle) when they finish.
# c.mode() only sets this process's view (what s.task_mode reports while
# task is idle), so task's own mode is read from the halui pins.

def mdi_then_manual(n, x, y, z):
    log("running MDI command %d" % n)
    h['mdi-%d' % n] = 1
    wait_for_joint_to_stop_at(0, x);
    wait_for_joint_to_stop_at(1, y);
    wait_for_joint_to_stop_at(2, z);
    h['mdi-%d' % n] = 0
    wait_for_halui_mode('is-manual')

log("asking for Manual mode")
c.mode(linuxcnc.MODE_MANUAL)
wait_for_halui_mode('is-manual')
mdi_then_manual(0, -1, 0, 0)

log("asking for Auto mode, which only sets this process's view")
c.mode(linuxcnc.MODE_AUTO)
c.wait_complete()
wait_for_task_mode(linuxcnc.MODE_AUTO)
wait_for_halui_mode('is-manual')
mdi_then_manual(1, 1, 0, 0)

log("asking for MDI mode, which only sets this process's view")
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()
wait_for_task_mode(linuxcnc.MODE_MDI)
wait_for_halui_mode('is-manual')
mdi_then_manual(2, 1, 2, 0)
mdi_then_manual(3, 1, 2, 3)
mdi_then_manual(0, -1, 0, 0)


sys.exit(0)
