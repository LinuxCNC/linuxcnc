#!/usr/bin/env python3

"""
Regression test: a partial G28.3 Pn on NON-IDENTITY kinematics must abort
cleanly instead of stranding the machine.

Motion refuses to (re-)enter TELEOP or COORD while any joint is unhomed, but
only when the kinematics are not identity -- switch_to_teleop_mode() (motion.c)
and the EMCMOT_COORD case (command.c) both gate on
"kinType != KINEMATICS_IDENTITY && !get_allhomed()".

The G28.2/G28.3 sequencing dips motion into FREE (do_homing() only advances
there) and restores the previous trajectory mode afterwards. Restoring that
mode unconditionally is wrong: a per-joint G28.3 Pn succeeds for its own joint
while leaving the machine as a whole unreferenced, so motion rejects the
restore, task still reports DONE, and the machine is stranded in FREE with the
GUI's mode controls greyed out -- recoverable only by cycling the controller.
Reported on real hardware (Mesa 7I95T gantry) in PR #4172, where it surfaced as
"all joints must be homed before going into coordinated mode" followed by a
dead UI needing F2.

Note this needs NO_FORCE_HOMING=1. With the default 0, an earlier branch
catches the partial-unhome case first and this path is never reached -- which
is exactly why the trivkins `sequencing` test (NO_FORCE_HOMING unset, identity
kins) passes both before and after the fix.

The witness is deliberately M64 P0 rather than a move: a digital output needs
no coordinated motion, so it still executes with the machine stuck in FREE.
If it ends up set, the program continued past the G28.3 -- the bug.
"""

import linuxcnc
import hal

import os
import sys
import time

h = hal.component("python-ui")
h.ready()

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


def poll():
    s.poll()


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def near(a, b, tol=0.001):
    return abs(a - b) < tol


def wait_idle(timeout=10.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        poll()
        if s.interp_state == linuxcnc.INTERP_IDLE:
            return True
        time.sleep(0.01)
    return False


def wait_homed(expected, timeout=10.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        poll()
        if list(s.homed[:3]) == expected:
            return True
        time.sleep(0.01)
    return False


def drain_errors():
    msgs = []
    while True:
        err = e.poll()
        if not err:
            return msgs
        msgs.append(err[1])


c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(0)
c.home(1)
c.home(2)
if not wait_homed([1, 1, 1]):
    fail("initial home-all did not home all joints: {}".format(list(s.homed[:3])))

# Confirm the machine really is in a coordinated mode and can move, so that the
# restore this test is about has something meaningful to restore to.
c.mode(linuxcnc.MODE_MDI)
c.mdi("G0 X1")
if not wait_idle():
    fail("setup MDI move did not settle")
poll()
if not near(s.position[0], 1.0):
    fail("setup MDI move did not run (X at {}) -- machine not in a usable coordinated mode".format(s.position[0]))
drain_errors()

# Run the program: G28.3 P0 (partial unhome) then M64 P0 (the witness).
c.mode(linuxcnc.MODE_AUTO)
c.program_open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "test.ngc"))
c.auto(linuxcnc.AUTO_RUN, 0)
if not wait_idle():
    # This is the unfixed behaviour: task reported DONE for the G28.3, motion
    # silently refused to take TELEOP/COORD back, and the interpreter now sits
    # forever on a line it can never run. A timeout here IS the wedge.
    poll()
    fail(
        "interpreter never returned to idle after the G28.3 -- the machine is "
        "wedged: task accepted the partial unhome and tried to restore a "
        "coordinated mode that motion cannot grant while unreferenced "
        "(motion_mode={}, homed={}, dout0={})".format(
            s.motion_mode, list(s.homed[:3]), s.dout[0])
    )
time.sleep(0.3)
poll()

if list(s.homed[:3]) == [1, 1, 1]:
    fail("G28.3 P0 did not actually unhome joint 0: {}".format(list(s.homed[:3])))

if s.dout[0]:
    fail(
        "program continued past a G28.3 that left the machine partially homed "
        "(digital-out 0 got set): the mode restore was attempted and silently "
        "refused by motion, leaving the machine stranded in FREE while task "
        "reported DONE"
    )
print("PASS: a partial G28.3 Pn aborts the program on non-identity kinematics")

if s.motion_mode != linuxcnc.TRAJ_MODE_FREE:
    fail("expected to be left in FREE after the aborted G28.3, got motion_mode={}".format(s.motion_mode))
print("PASS: machine is left in FREE, the only mode it may legally sit in unreferenced")

msgs = drain_errors()
if not msgs:
    fail("the aborted G28.3 reported no error at all -- the operator gets no explanation")
print("PASS: an operator error was reported ({!r})".format(msgs[0][:70]))

# The whole point of failing cleanly rather than wedging: ordinary recovery
# must work, with no controller restart.
c.mode(linuxcnc.MODE_MANUAL)
c.home(0)
if not wait_homed([1, 1, 1]):
    fail("could not re-home joint 0 after the aborted G28.3 -- machine is wedged")

c.mode(linuxcnc.MODE_MDI)
c.mdi("G0 X2")
if not wait_idle():
    fail("post-recovery MDI move did not settle")
poll()
if not near(s.position[0], 2.0):
    fail("coordinated motion did not come back after re-homing (X stayed at {})".format(s.position[0]))
print("PASS: re-homing recovers coordinated motion without cycling the controller")

print("done! it all worked")
sys.exit(0)
