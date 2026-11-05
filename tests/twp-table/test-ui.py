#!/usr/bin/env python3
# G53.1 P1 and P2 on the xyzac-trt sim: see README.

import linuxcnc
import sys
import os
import time
import math

JOINTS = 5
A, C = 3, 4

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()
c.mode(linuxcnc.MODE_MDI)

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
    j = settled()
    drain()
    return j

# the tool axis in the work frame, conventional-directions 0: the table
# turns C then A, so the tool is Rz(-C) Rx(-A) along Z seen from the part
def tool_axis(j):
    a, cc = math.radians(j[A]), math.radians(j[C])
    return (math.sin(cc)*math.sin(a), math.cos(cc)*math.sin(a), math.cos(a))

# the plane's Z for G68.2 I J with K0, Euler Z X Z
def normal(i, j):
    i, j = math.radians(i), math.radians(j)
    return (math.sin(i)*math.sin(j), -math.cos(i)*math.sin(j), math.cos(j))

def angle(u, v):
    return math.degrees(math.acos(max(-1.0, min(1.0, sum(p*q for p, q in zip(u, v))))))

def travel(j, start):
    return abs(j[A] - start[0]) + abs(j[C] - start[1])

mdi("G12.1 P1", "G0 X0 Y0 Z60 A0 C0")

# half.ini: A tilts one way only, its [AXIS_A] travel 0 to 100 while
# [JOINT_3] runs on to 9999, so a pose turned whole turns along to fit the
# joint must still fit the axis: each side plane goes to A90, C carrying it
one_sided = linuxcnc.ini(os.environ["INI_FILE_NAME"]).find("AXIS_A", "MIN_LIMIT") == "0"
if one_sided:
    for i in (0, 90, -90, 180):
        mdi("G69", "G0 X0 Y0 Z60 A0 C0", "G68.2 X0 Y0 Z0 I%g J90 K0" % i)
        j = mdi("G53.1")
        off = angle(tool_axis(j), normal(i, 90))
        if off > 1e-6 or abs(j[A] - 90.0) > 1e-6:
            error("G53.1 on I%g J90 went to A%.3f C%.3f, %.6f degrees off the normal"
                  % (i, j[A], j[C], off))
        print("I%g J90 G53.1 on the one-sided A: A%.3f C%.3f" % (i, j[A], j[C]))

planes = [] if one_sided else [(0, 30), (60, 30), (-90, 45), (45, -30), (150, 20)]
starts = [(0, 0), (-40, 200), (40, -100)]
for i, jj in planes:
    nearest = []
    for word, sign in (("", 0), ("P1", 1), ("P2", -1)):
        landed = []
        for start in starts:
            mdi("G69", "G0 X0 Y0 Z60 A%g C%g" % start,
                "G68.2 X0 Y0 Z0 I%g J%g K0" % (i, jj))
            j = mdi("G53.1 %s" % word)
            off = angle(tool_axis(j), normal(i, jj))
            if off > 1e-6:
                error("G53.1 %s on I%g J%g from A%g C%g left the tool %.6f degrees off the normal"
                      % (word, i, jj, start[0], start[1], off))
            if sign and j[A] * sign <= 0:
                error("G53.1 %s on I%g J%g from A%g C%g put A at %.4f"
                      % (word, i, jj, start[0], start[1], j[A]))
            landed.append(j)
            if not sign:
                nearest.append((j, start))
        if sign:
            # one pose, C on whichever turn is nearest the start
            for j in landed[1:]:
                d = (j[C] - landed[0][C]) % 360.0
                if abs(j[A] - landed[0][A]) > 1e-6 or min(d, 360.0 - d) > 1e-6:
                    error("G53.1 %s on I%g J%g landed on different poses: %s and %s"
                          % (word, i, jj, landed[0], j))
        print("I%g J%g G53.1 %s: %s" % (i, jj, word,
              ", ".join("A%.3f C%.3f" % (j[A], j[C]) for j in landed)))
    # no P: the nearer of the two poses, by rotary travel
    for (j, start) in nearest:
        mdi("G69", "G0 X0 Y0 Z60 A%g C%g" % start, "G68.2 X0 Y0 Z0 I%g J%g K0" % (i, jj))
        other = mdi("G53.1 %s" % ("P2" if j[A] > 0 else "P1"))
        if travel(other, start) < travel(j, start) - 1e-6:
            error("G53.1 on I%g J%g from A%g C%g went to A%.3f C%.3f, A%.3f C%.3f is nearer"
                  % (i, jj, start[0], start[1], j[A], j[C], other[A], other[C]))

# after the turn, a move in the plane ends where the plane puts it
turns = () if one_sided else ((60, 30, "P1"), (60, 30, "P2"), (-90, 45, "P1"))
for i, jj, word in turns:
    mdi("G69", "G0 X0 Y0 Z60 A0 C0", "G68.2 X0 Y0 Z0 I%g J%g K0" % (i, jj),
        "G53.1 %s" % word, "G0 X10 Y5 Z20", "G1 X-10 F2000")
    s.poll()
    ri, rj = math.radians(i), math.radians(jj)
    px, py, pz = -10.0, 5.0, 20.0
    y1 = py*math.cos(rj) - pz*math.sin(rj)
    z1 = py*math.sin(rj) + pz*math.cos(rj)
    want = (px*math.cos(ri) - y1*math.sin(ri), px*math.sin(ri) + y1*math.cos(ri), z1)
    got = s.position[:3]
    if max(abs(p - q) for p, q in zip(got, want)) > 1e-6:
        error("after G53.1 %s on I%g J%g the tip is at %s, the plane puts it at %s"
              % (word, i, jj, got, want))

mdi("G69", "G0 X0 Y0 Z60 A0 C0")

for f in ("sim.var", "sim.var.bak"):
    try:
        os.unlink(f)
    except OSError:
        pass

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
