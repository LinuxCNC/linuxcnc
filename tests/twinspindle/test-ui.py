#!/usr/bin/env python3
# twinspindlekins: two primary types, one per spindle, orienting with
# different rotaries: see README.

import linuxcnc
import math
import os
import sys
import time

JOINTS = 6
PIVOT = 150.0                   # twinspindlekins.pivot-length default
DISTANCE = 500.0                # twinspindlekins.spindle-distance default
TOOL = 50.0                     # T1 in tool.tbl
FACE = 60.0                     # G54 and G55 Z, the part faces
PROGRAM = os.path.abspath("../../configs/sim/axis/vismach/twinspindle/twin-spindle-holes.ngc")

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()

errors = 0

def error(msg):
    global errors
    errors += 1
    print("*** ERROR " + msg)

def drain():
    while True:
        m = e.poll()
        if not m:
            return
        print("channel:", m)
        if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            error("reported: %s" % m[1])

def settled():
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        s.poll()
        now = [s.joint_position[i] for i in range(JOINTS)]
        if s.inpos and not s.queue and now == last:
            return now
        last = now
        time.sleep(0.05)
    error("timed out waiting for the move")
    return last

def mdi(*cmds):
    for cmd in cmds:
        c.mdi(cmd)
        c.wait_complete(60)
    return settled()

def param(name):
    drain()
    c.mdi("(debug,#<%s>)" % name)
    c.wait_complete(30)
    deadline = time.time() + 5
    while time.time() < deadline:
        m = e.poll()
        if not m:
            time.sleep(0.01)
            continue
        if m[0] == linuxcnc.OPERATOR_DISPLAY:
            return float(m[1])
    error("#<%s> gave no value" % name)
    return None

def turn(a, b):
    # a and b the same angle, whole turns apart
    return abs((a - b + 180.0) % 360.0 - 180.0) < 1e-6

# the module's maths, written out again: the tip and the tool axis, tip
# towards holder, in the frame of the part on spindle `sub`.  The slides
# carry the B axis PIVOT above the gauge line, the tip hangs PIVOT + TOOL
# below it along the tool axis.
def part_pose(j, sub):
    L = PIVOT + TOOL
    b = math.radians(j[3])
    m = [j[0] - L*math.sin(b), j[1], j[2] + PIVOT - L*math.cos(b)]
    u = [math.sin(b), 0.0, math.cos(b)]
    if sub:
        m = [m[0], -m[1], DISTANCE - m[2]]
        u = [u[0], -u[1], -u[2]]
    a = math.radians(j[5] if sub else j[4])
    rz = lambda v: [math.cos(a)*v[0] - math.sin(a)*v[1],
                    math.sin(a)*v[0] + math.cos(a)*v[1], v[2]]
    return rz(m), rz(u)

ORIENT = ("_kins_orient_1", "_kins_orient_2", "_kins_orient_3",
          "_kins_orient_1_head", "_kins_orient_2_head", "_kins_orient_3_head")

mdi("T1 M6", "G43",
    "G10 L2 P1 X0 Y0 Z%g C0 W0" % FACE,
    "G10 L2 P2 X0 Y0 Z%g C0 W0" % FACE)

# ---- each type names its own rotaries --------------------------------------

for ktype, want, what in ((0, [5, 4, -1, 0, 1, -1], "C then B"),
                          (1, [8, 4, -1, 0, 1, -1], "W then B"),
                          (2, [-1] * 6, "nothing")):
    mdi("G12.1 P%d" % ktype)
    got = [param(n) for n in ORIENT]
    if got != want:
        error("type %d orients with %s, not %s: table then head" % (ktype, got, what))

# ---- the same hole on either spindle ---------------------------------------
#
# A hole 30 out on the face at 60 degrees, tilted 20 degrees outwards.  On
# the main spindle C turns it under the head and B leans the tool; on the
# sub spindle, whose part faces the other way, W turns it and B leans the
# tool past 180.

A, T, R = 60.0, 20.0, 30.0
n = [math.sin(math.radians(T))*math.cos(math.radians(A)),
     math.sin(math.radians(T))*math.sin(math.radians(A)),
     math.cos(math.radians(T))]
hole = [R*math.cos(math.radians(A)), R*math.sin(math.radians(A)), 0.0]
PLANE = ("G68.2 P3 Q1 X%.9f Y%.9f Z0 I%.9f J%.9f K0"
         % (hole[0], hole[1], -math.sin(math.radians(A)), math.cos(math.radians(A))),
         "G68.2 P3 Q2 I%.9f J%.9f K%.9f" % tuple(n))

for ktype, offset, joint, b in ((0, "G54", 4, T), (1, "G55", 5, 180.0 - T)):
    sub = ktype == 1
    mdi("G13.1", "G53 G0 X300", "G53 G0 Z50 B90")
    mdi("G12.1 P%d" % ktype, offset, *PLANE)
    mdi("G53.2")
    rot1, rot2 = param("_orient_rot1"), param("_orient_rot2")
    print("type %d: rot1 %.6f rot2 %.6f" % (ktype, rot1, rot2))
    if not (turn(rot1, A) and abs(rot2 - b) < 1e-6):
        error("type %d: G53.2 gave rot1 %s rot2 %s, not %s and %s" % (ktype, rot1, rot2, A, b))
    j = mdi("G53.3 X0 Y0 Z5")
    if not (turn(j[joint], A) and abs(j[3] - b) < 1e-6):
        error("type %d: G53.3 left joint %d at %s and B at %s" % (ktype, joint, j[joint], j[3]))
    tip, axis = part_pose(j, sub)
    want = [hole[i] + 5*n[i] + (FACE if i == 2 else 0.0) for i in range(3)]
    if max(abs(tip[i] - want[i]) for i in range(3)) > 1e-6:
        error("type %d: the tip is at %s in the part, not %s" % (ktype, tip, want))
    if max(abs(axis[i] - n[i]) for i in range(3)) > 1e-9:
        error("type %d: the tool points along %s in the part, not %s" % (ktype, axis, n))
    mdi("G69")
drain()

# ---- the demo program, both spindles, one subroutine -----------------------

mdi("G13.1", "G54")
c.mode(linuxcnc.MODE_AUTO)
c.wait_complete()
c.program_open(PROGRAM)
c.auto(linuxcnc.AUTO_RUN, 0)
deadline = time.time() + 300
time.sleep(0.5)
while time.time() < deadline:
    s.poll()
    if s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0 and s.inpos:
        break
    time.sleep(0.1)
else:
    error("the demo program did not finish")
drain()
j = settled()
if max(abs(j[i] - w) for i, w in ((0, 300.0), (2, 50.0), (3, 90.0))) > 1e-6:
    error("the demo program ended at %s, not parked" % (j,))

c.state(linuxcnc.STATE_ESTOP)
for f in ("sim.var", "sim.var.bak"):
    try:
        os.unlink(f)
    except OSError:
        pass
print("Exiting with %d errors" % errors)
sys.exit(errors != 0)
