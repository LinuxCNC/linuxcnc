#!/usr/bin/env python3

"""
Regression test: G28.2 Pn / G28.3 Pn with a joint number the machine does not
have must be rejected immediately, with an error that names the bad joint, and
must not disturb the trajectory mode.

Reported on real hardware (Mesa 7I95T gantry) in PR #4172: "g28.2 p5" on a
5-joint machine gave the generic

    G28.2 home did not start -- check machine mode, motion.homing-inhibit, ...

after a two-second stall, and left the GUI jogging in joint mode until the next
G-code command happened to put it back.

Two separate defects: emcJointHome()/emcJointUnhome() range-checked against
EMCMOT_MAX_JOINTS (the compile-time maximum, 16) instead of the machine's
configured joint count, so the command went to motion, which silently ignored
it; and the FREE-mode dip taken for homing sequencing then sat there for the
whole start timeout.

This config has JOINTS = 3, so P3 and up are unconfigured.
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


def mode_name(m):
    return {linuxcnc.TRAJ_MODE_FREE: "FREE",
            linuxcnc.TRAJ_MODE_COORD: "COORD",
            linuxcnc.TRAJ_MODE_TELEOP: "TELEOP"}.get(m, str(m))


c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(0)
c.home(1)
c.home(2)
if not wait_homed([1, 1, 1]):
    fail("initial home-all did not home all joints: {}".format(list(s.homed[:3])))

# Establish a coordinated mode worth preserving.
c.mode(linuxcnc.MODE_MDI)
c.mdi("G0 X1")
if not wait_idle():
    fail("setup MDI move did not settle")
poll()
if not near(s.position[0], 1.0):
    fail("setup MDI move did not run (X at {})".format(s.position[0]))
drain_errors()
poll()
prior_mode = s.motion_mode
print("setup: coordinated motion works, motion_mode={}".format(mode_name(prior_mode)))

for cmd in ("G28.2 P3", "G28.3 P7"):
    drain_errors()
    t0 = time.time()
    c.mdi(cmd)
    if not wait_idle():
        fail("{} never returned to idle".format(cmd))
    elapsed = time.time() - t0
    time.sleep(0.3)
    poll()

    msgs = drain_errors()
    if not msgs:
        fail("{} on an unconfigured joint reported no error at all".format(cmd))
    joined = " ".join(msgs)
    if "did not start" in joined:
        fail("{} stalled into the generic start-timeout error instead of being "
             "rejected up front: {!r}".format(cmd, msgs[0][:120]))
    if "joint" not in joined.lower():
        fail("{} error does not mention the joint number: {!r}".format(cmd, msgs[0][:120]))
    print("PASS: {} reported {!r}".format(cmd, msgs[0][:90]))

    if elapsed > 1.5:
        fail("{} took {:.1f}s to be rejected -- it went to motion and sat out "
             "the homing start timeout".format(cmd, elapsed))
    print("PASS: {} was rejected in {:.2f}s, no start-timeout stall".format(cmd, elapsed))

    if list(s.homed[:3]) != [1, 1, 1]:
        fail("{} changed the homed state of a real joint: {}".format(cmd, list(s.homed[:3])))

    if s.motion_mode != prior_mode:
        fail("{} left the machine in {} (was {}) -- the FREE-mode dip taken for "
             "homing sequencing was not undone, so the GUI is stuck jogging in "
             "joint mode".format(cmd, mode_name(s.motion_mode), mode_name(prior_mode)))
    print("PASS: {} left the trajectory mode untouched ({})".format(cmd, mode_name(s.motion_mode)))

# The rejection must not have cost anything: ordinary work continues.
c.mode(linuxcnc.MODE_MDI)
c.mdi("G0 X2")
if not wait_idle():
    fail("MDI move after the rejected G28.2 did not settle")
poll()
if not near(s.position[0], 2.0):
    fail("coordinated motion did not survive the rejected Pn (X stayed at {})".format(s.position[0]))
print("PASS: coordinated motion still works after the rejection")

# A valid Pn on the same machine must still work, so the check is not just
# refusing everything.
drain_errors()
c.mdi("G28.2 P1")
if not wait_idle():
    fail("a valid G28.2 P1 did not settle")
time.sleep(0.3)
poll()
if list(s.homed[:3]) != [1, 1, 1]:
    fail("valid G28.2 P1 left joint 1 unhomed: {}".format(list(s.homed[:3])))
print("PASS: a valid G28.2 P1 still homes on the same machine")

print("done! it all worked")
sys.exit(0)
