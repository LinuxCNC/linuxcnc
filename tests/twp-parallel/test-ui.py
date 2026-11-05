#!/usr/bin/env python3
# The tilted work plane on a parallel machine: see README.

import linuxcnc
import sys
import os
import time
import math
import numpy as np

ini = linuxcnc.ini(sys.argv[2])
HEXA = ini.find("KINS", "KINEMATICS") == "genhexkins"
JOINTS = int(ini.find("KINS", "JOINTS"))

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
    got = []
    while True:
        m = e.poll()
        if not m:
            return got
        got.append(m)

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
    for m in drain():
        print("channel:", m)
        if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            error("reported: %s" % m[1])
    return j

def refused(cmd, words):
    c.mdi(cmd)
    c.wait_complete(60)
    settled()
    msgs = [m[1] for m in drain() if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR)]
    if not any(words in m for m in msgs):
        error("%s was not refused with '%s': %s" % (cmd, words, msgs))

# columns are the tool's axes, in the work frame
def tool():
    s.poll()
    if s.tool_frame is None:
        error("no tool frame reported")
        return np.eye(3)
    return np.array(s.tool_frame).reshape(3, 3)

# columns are the plane's axes; and its origin
def plane():
    s.poll()
    return np.array(s.g68_rotation).reshape(3, 3), np.array(s.g68_offset[:3])

def point():
    s.poll()
    return np.array(s.position[:3])

def close(a, b, tol=1e-6):
    return np.max(np.abs(np.array(a) - np.array(b))) < tol

# the rotation of least angle taking unit vector a to unit vector b
def shortest(a, b):
    k = np.cross(a, b)
    sn, cs = np.linalg.norm(k), np.dot(a, b)
    if sn < 1e-12:
        return np.eye(3)
    k = k / sn
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + sn * K + (1 - cs) * K.dot(K)

if HEXA:
    start = "G0 X0 Y0 Z20 A5 B-3 C10"
    define = "G68.2 X1 Y2 Z3 I30 J15 K0"
    target = [0, 0, 17]
else:
    start = "G0 X10 Y-20 Z0 A5 B-3"
    define = "G68.2 X1 Y2 Z3 I30 J12 K0"
    target = [5, 5, 5]

# --- G53.6 turns the tool to the plane about the tool tip ----------------
mdi(start)
T0, P0 = tool(), point()
mdi(define)
R, O = plane()
n = R[:, 2]
mdi("G53.6")
T1 = tool()
if not close(T1[:, 2], n):
    error("the tool axis after G53.6 is %s, not the plane normal %s" % (T1[:, 2], n))
if not close(point(), P0):
    error("G53.6 moved the tool point from %s to %s" % (P0, point()))
if HEXA:
    # the axis leaves the turn about it free: the platform takes the
    # shortest rotation from where it stood, no turn about the tool
    if not close(T1, shortest(T0[:, 2], T1[:, 2]).dot(T0)):
        error("G53.6 turned the platform about the tool:\n%s\nnot\n%s"
              % (T1, shortest(T0[:, 2], T1[:, 2]).dot(T0)))
else:
    # the spindle's turn is free in the machine, and tool x is the chain's,
    # in the machine's Z/X plane
    if abs(T1[1, 0]) > 1e-9:
        error("the pentapod's tool x %s is out of the machine's Z/X plane" % T1[:, 0])

# --- G53.1 turns rotary joints alone, and there are none -----------------
before = point()
refused("G53.1", "no rotary joint")
if not close(point(), before):
    error("the refused G53.1 moved the machine")

# --- G53.3 goes to a point in the plane with the tool on its normal -------
mdi("G69", start, define, "G53.3 X%g Y%g Z%g" % tuple(target))
R, O = plane()
prog = R.T.dot(point() - O)
if not close(prog, target):
    error("G53.3 ended at %s in the plane, not %s" % (prog, target))
T3 = tool()
if not close(T3[:, 2], R[:, 2]):
    error("the tool axis after G53.3 is not the plane normal")

# --- G68.3 reads the plane back off the tool -----------------------------
mdi("G69", "G68.3 X1 Y2 Z3")
s.poll()
R3, O3 = plane()
if not s.g68_active or not close(O3, [1, 2, 3], 1e-9):
    error("G68.3 did not set the origin asked for")
if not close(R3[:, 2], T3[:, 2]):
    error("G68.3's normal %s is not the tool axis %s" % (R3[:, 2], T3[:, 2]))
mdi("G69")

# --- the [AXIS_L] box is the platform's, the tool left out ---------------
# A long tool and a tilt about Y put the platform off the tool's pose along
# X, by L sin B: a move whose pose is inside the box with the platform past
# it is refused, and one whose pose is past it with the platform inside is
# accepted, as on a head, where the box is the carriage's.
length, h, tilt, edge = (10, 1, 20, 10) if HEXA else (100, 2, 15, 250)
shift = length * math.sin(math.radians(tilt))
z = (20 if HEXA else 0) - length
mdi("G43 H%d" % h, "G0 X0 Y0 Z%g A0 B%g%s" % (z, tilt, " C0" if HEXA else ""))
refused("G0 X%g" % (edge - shift / 2), "limit")
mdi("G0 X%g" % (-edge - shift / 2))
if not close(point()[0], -edge - shift / 2, 1e-3):
    error("the move with the platform inside the box did not get there: X %.4f" % point()[0])
mdi("G0 X0", "G0 B0", "G49")

for f in ("sim.var", "sim.var.bak"):
    try:
        os.unlink(f)
    except OSError:
        pass

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
