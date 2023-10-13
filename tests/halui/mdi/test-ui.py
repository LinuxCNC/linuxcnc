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

h.newpin("mdi-0", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("mdi-1", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("mdi-2", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("mdi-3", hal.HAL_BIT, hal.HAL_OUT)

h.newpin("joint-0-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-1-position", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("joint-2-position", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("is-manual", hal.HAL_BIT, hal.HAL_IN)
h.newpin("is-auto", hal.HAL_BIT, hal.HAL_IN)
h.newpin("is-mdi", hal.HAL_BIT, hal.HAL_IN)

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

log("setting mode to Manual")
c.mode(linuxcnc.MODE_MANUAL)
wait_for_halui_mode('is-manual')
log("running MDI command 0")
h['mdi-0'] = 1
wait_for_joint_to_stop_at(0, -1);
wait_for_joint_to_stop_at(1, 0);
wait_for_joint_to_stop_at(2, 0);
h['mdi-0'] = 0
wait_for_task_mode(linuxcnc.MODE_MANUAL)
wait_for_halui_mode('is-manual')

log("setting mode to Auto")
c.mode(linuxcnc.MODE_AUTO)
wait_for_halui_mode('is-auto')
log("running MDI command 1")
h['mdi-1'] = 1
wait_for_joint_to_stop_at(0, 1);
wait_for_joint_to_stop_at(1, 0);
wait_for_joint_to_stop_at(2, 0);
h['mdi-1'] = 0
wait_for_task_mode(linuxcnc.MODE_AUTO)
wait_for_halui_mode('is-auto')

log("setting mode to MDI")
c.mode(linuxcnc.MODE_MDI)
wait_for_halui_mode('is-mdi')
log("running MDI command 2")
h['mdi-2'] = 1
wait_for_joint_to_stop_at(0, 1);
wait_for_joint_to_stop_at(1, 2);
wait_for_joint_to_stop_at(2, 0);
h['mdi-2'] = 0
s.poll()
wait_for_task_mode(linuxcnc.MODE_MDI)
wait_for_halui_mode('is-mdi')

log("running MDI command 3")
h['mdi-3'] = 1
wait_for_joint_to_stop_at(0, 1);
wait_for_joint_to_stop_at(1, 2);
wait_for_joint_to_stop_at(2, 3);
h['mdi-3'] = 0
wait_for_task_mode(linuxcnc.MODE_MDI)
wait_for_halui_mode('is-mdi')

log("running MDI command 0")
h['mdi-0'] = 1
wait_for_joint_to_stop_at(0, -1);
wait_for_joint_to_stop_at(1, 0);
wait_for_joint_to_stop_at(2, 0);
h['mdi-0'] = 0
wait_for_task_mode(linuxcnc.MODE_MDI)
wait_for_halui_mode('is-mdi')


sys.exit(0)

