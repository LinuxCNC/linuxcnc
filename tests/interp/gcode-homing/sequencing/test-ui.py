#!/usr/bin/env python3

"""
Regression test for the G28.2 Pn task-level sequencing behavior:

 1. G28.2 Pn's FREE-mode dip (needed because do_homing() only advances in
    free mode) must be invisible at the task level -- task.mode should be
    unaffected by it.
 2. A G28.2 Pn naming a joint that does not exist on the machine must be
    rejected without leaving task.mode stuck (regression test for a bug
    where an outright-rejected home left the FREE-mode dip unrestored,
    which made task.mode read as MANUAL forever).
"""

import linuxcnc
import hal

import sys
import time

h = hal.component("python-ui")
h.ready()

c = linuxcnc.command()
s = linuxcnc.stat()


def poll():
    s.poll()


def wait_idle(timeout=5.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        poll()
        if s.exec_state == linuxcnc.EXEC_DONE and s.interp_state == linuxcnc.INTERP_IDLE:
            return True
        time.sleep(0.01)
    return False


def wait_homed(expected, timeout=5.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        poll()
        if list(s.homed[:3]) == expected:
            return True
        time.sleep(0.01)
    return False


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def near(a, b, tol=0.001):
    return abs(a - b) < tol


c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(0)
c.home(1)
c.home(2)
if not wait_homed([1, 1, 1]):
    fail("initial home-all did not home all joints: {}".format(list(s.homed[:3])))

c.mode(linuxcnc.MODE_MDI)
time.sleep(0.2)
poll()
mode_before = s.task_mode

# Machine is still fully homed here, so this MDI G28.2 P1 is a redundant
# re-home -- the NO_FORCE_HOMING gate doesn't apply (all_homed() is true
# throughout), so this exercises the plain mode-dip-and-restore path.
c.mdi("G28.2 P1")
if not wait_idle():
    fail("redundant G28.2 P1 did not complete")
poll()
if s.task_mode != mode_before:
    fail("task_mode changed across a redundant G28.2 Pn: {} -> {}".format(mode_before, s.task_mode))
print("PASS: G28.2 Pn's mode dip is invisible at the task level")

# The machine is fully homed, so this passes the NO_FORCE_HOMING gate and
# reaches the actual Pn validation, which must reject joint 99 without
# leaving task_mode stuck (regression test for the fix in 65447329e9).
c.mdi("G28.2 P99")
if not wait_idle():
    fail("invalid-joint G28.2 P99 did not settle")
poll()
if s.task_mode != mode_before:
    fail("task_mode after invalid Pn is {}, expected {} (MDI)".format(s.task_mode, mode_before))

c.mdi("G0 X2")
if not wait_idle():
    fail("recovery MDI command after invalid Pn did not settle")
poll()
if not near(s.position[0], 2.0):
    fail("MDI command after an invalid Pn was rejected -- task_mode stuck (X stayed at {})".format(s.position[0]))
print("PASS: an invalid Pn does not leave task_mode stuck")

print("done! it all worked")
sys.exit(0)
