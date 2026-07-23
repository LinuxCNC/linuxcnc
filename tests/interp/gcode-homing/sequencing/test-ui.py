#!/usr/bin/env python3

"""
Regression test for the G28.2/G28.3 Pn task-level sequencing behavior:

 1. G28.2 Pn's FREE-mode dip (needed because do_homing() only advances in
    free mode) must be invisible at the task level -- task.mode should be
    unaffected by it.
 2. With NO_FORCE_HOMING unset (default, force homing required), a G28.3 Pn
    that leaves the machine not fully homed must trip the pre-existing
    NO_FORCE_HOMING gate (checked at commit 2a5eabf581, re-applied at this
    sync point by commit 3d238dbb46) and block further MDI commands, exactly
    as if never homed -- note this fires on the *current* homed state, not
    on whether this particular G28.3 changed anything, so even a redundant
    G28.3 Pn on an already-unhomed joint trips it as long as the machine
    isn't fully homed. Recovery has to go through the classic direct
    joint-home NML call (c.home(), the same one the GUI's Home button uses)
    rather than MDI text, since that gate has no exemption for a homing
    command that is itself submitted as MDI text.
 3. A G28.2/G28.3 Pn naming a joint that does not exist on the machine must
    be rejected without leaving task.mode stuck (regression test for a bug
    where an outright-rejected home/unhome left the FREE-mode dip
    unrestored, which made task.mode read as MANUAL forever).
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

# This G28.3 P1 leaves the machine not fully homed -- with the default
# NO_FORCE_HOMING=0, that must trip the gate: task_mode no longer reads the
# MDI/AUTO choice (determineMode() derives MANUAL from the traj mode being
# left at FREE), and further MDI motion is blocked.
c.mdi("G28.3 P1")
if not wait_idle():
    fail("G28.3 P1 did not settle")
poll()
if list(s.homed[:3]) == [1, 1, 1]:
    fail("G28.3 P1 did not actually unhome joint 1")
if s.task_mode == mode_before:
    fail("NO_FORCE_HOMING gate did not trip after G28.3 Pn left the machine not fully homed (task_mode still {})".format(s.task_mode))
print("PASS: NO_FORCE_HOMING gate trips after G28.3 Pn leaves the machine not fully homed")

c.mdi("G0 X1")
if not wait_idle():
    fail("gate-check MDI command did not settle")
poll()
if not near(s.position[0], 0.0):
    fail("NO_FORCE_HOMING gate did not block MDI after G28.3 Pn left the machine not fully homed (X moved to {})".format(s.position[0]))
print("PASS: NO_FORCE_HOMING gate blocks MDI after G28.3 Pn leaves the machine not fully homed")

# Recovery goes through the classic direct joint-home call (same path the
# GUI's Home button uses), not MDI text -- the gate has no exemption for a
# homing command submitted as MDI text.
c.home(1)
if not wait_homed([1, 1, 1]):
    fail("recovery home of joint 1 did not complete")

c.mode(linuxcnc.MODE_MDI)
time.sleep(0.2)
poll()
if s.task_mode != mode_before:
    fail("task_mode did not recover to MDI after re-homing (still {})".format(s.task_mode))

c.mdi("G0 X1")
if not wait_idle():
    fail("post-recovery MDI command did not settle")
poll()
if not near(s.position[0], 1.0):
    fail("MDI still blocked after recovering full homed state (X stayed at {})".format(s.position[0]))
print("PASS: MDI works again once fully homed")

# Machine is fully homed again here, so this passes the gate and reaches
# the actual Pn validation, which must reject joint 99 without leaving
# task_mode stuck (regression test for the fix in 65447329e9).
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
