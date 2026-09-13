#!/usr/bin/env python3

"""
Regression test: a G28.2 homing cycle resyncs the interpreter's model of
the current position, so a following G91 (incremental) move or an I/J/K
arc centre is computed from where the machine actually is after homing,
not from the stale pre-home point.

The motivating case (raised in PR #4172 review): a rotary head or a
spindle re-tasked as a C axis is re-homed mid-program with G28.2 Pn.
An immediate home rewrites that joint's coordinate to HOME_OFFSET with no
physical motion (homing.c HOME_SET_INDEX_POSITION), so if the interpreter
keeps its pre-home value a subsequent `G91 C90` targets `stale_C + 90` and
the axis sweeps the difference -- on a wrapped rotary there is no soft
limit to stop it.

Checked here on joint 3 (C, angular) and joint 0 (X, linear): move the
axis away from zero, re-home it (coordinate jumps back to 0, no motion),
then make an incremental move and confirm it lands at 0 + increment.
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


def wait_idle(timeout=10.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        poll()
        if s.exec_state == linuxcnc.EXEC_DONE and s.interp_state == linuxcnc.INTERP_IDLE:
            return True
        time.sleep(0.01)
    return False


def wait_homed(expected, timeout=10.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        poll()
        if list(s.homed[:len(expected)]) == expected:
            return True
        time.sleep(0.01)
    return False


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def near(a, b, tol=0.01):
    return abs(a - b) < tol


def mdi(cmd):
    c.mdi(cmd)
    if c.wait_complete(10) == -1:
        fail("MDI %r timed out" % cmd)
    if not wait_idle():
        fail("MDI %r did not settle" % cmd)
    poll()


c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
for j in range(4):
    c.home(j)
if not wait_homed([1, 1, 1, 1]):
    fail("initial home-all did not home all joints: {}".format(list(s.homed[:4])))

c.mode(linuxcnc.MODE_MDI)
time.sleep(0.2)

# --- rotary C (joint 3) ---
mdi("G0 C30")
if not near(s.position[5], 30.0):
    fail("C did not reach 30 (got {})".format(s.position[5]))

# Immediate re-home: C's coordinate snaps back to 0 with no physical move.
mdi("G28.2 P3")
if not near(s.position[5], 0.0):
    fail("G28.2 P3 did not put C back to 0 (got {})".format(s.position[5]))

# The interpreter must now know C is 0. Without the resync it still thinks
# C is 30, so this incremental move targets 120 and the axis sweeps 90 deg
# too far.
mdi("G91 G0 C90")
mdi("G90")
if near(s.position[5], 120.0):
    fail("G91 C90 after G28.2 P3 targeted stale_C + 90 = 120 -- "
         "interpreter position not resynced after homing")
if not near(s.position[5], 90.0):
    fail("G91 C90 after G28.2 P3 landed at {}, expected 90".format(s.position[5]))
print("PASS: G91 rotary move after G28.2 Pn is computed from the homed position")

# --- linear X (joint 0) ---
mdi("G0 X5")
if not near(s.position[0], 5.0):
    fail("X did not reach 5 (got {})".format(s.position[0]))

mdi("G28.2 P0")
if not near(s.position[0], 0.0):
    fail("G28.2 P0 did not put X back to 0 (got {})".format(s.position[0]))

mdi("G91 G0 X1")
mdi("G90")
if near(s.position[0], 6.0):
    fail("G91 X1 after G28.2 P0 targeted stale_X + 1 = 6 -- "
         "interpreter position not resynced after homing")
if not near(s.position[0], 1.0):
    fail("G91 X1 after G28.2 P0 landed at {}, expected 1".format(s.position[0]))
print("PASS: G91 linear move after G28.2 Pn is computed from the homed position")

# --- absolute moves are unaffected either way ---
mdi("G0 X0 C0")
if not (near(s.position[0], 0.0) and near(s.position[5], 0.0)):
    fail("absolute move back to origin failed: X={} C={}".format(s.position[0], s.position[5]))
print("PASS: absolute moves after G28.2 still land where asked")

print("done! it all worked")
sys.exit(0)
