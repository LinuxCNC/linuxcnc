#!/usr/bin/env python3
# The point-to-point moves on a machine whose slides do not line up with its
# frame: see README.

import linuxcnc
import hal
import sys
import os
import time
import math

JOINTS = 5
SLANT = math.radians(30.0)
TOOL = 50.0                     # T1 in tool.tbl

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
    return settled()

def kins_type():
    return int(hal.get_value("motion.kins-type"))

def show(what, j):
    print("%-34s %s" % (what, " ".join("%.4f" % v for v in j)))

def expect(what, j, want, tol=1e-4):
    show(what, j)
    if max(abs(a - b) for a, b in zip(j, want)) > tol:
        error("%s: expected %s" % (what, " ".join("%.4f" % v for v in want)))

# the joints of a carriage position: joint 1 is the slanted slide
def carriage(x, y, z, b=0.0, cc=0.0):
    j1 = y / math.sin(SLANT)
    return [x - j1 * math.cos(SLANT), j1, z, b, cc]

def refused(*cmds):
    for cmd in cmds:
        c.mdi(cmd)
        c.wait_complete(30)
        m = e.poll()
        if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            error("%s was accepted" % cmd)
        else:
            print("refused as expected:", m[1])
        c.mode(linuxcnc.MODE_MDI)

def expect_type(what, want):
    k = kins_type()
    print("%-34s kins-type %d" % (what, k))
    if k != want:
        error("%s: kins-type %d, expected %d" % (what, k, want))

# --- G13.1 and G49 select the machine frame type, not the identity ---------
mdi("G12.1 P2")
expect_type("G12.1 P2, the plain identity", 2)
mdi("G13.1")
expect_type("G13.1", 0)
mdi("G43.4 H1")
expect_type("G43.4", 1)
mdi("G49")
expect_type("G49", 0)
mdi("G12.1 P1", "G13.1")
expect_type("G12.1 P1 then G13.1", 0)

# --- two primary types: G43.4 keeps the head G12.1 selected ------------------
mdi("G12.1 P3", "G43.4 H1")
expect_type("G12.1 P3 then G43.4, the second head", 3)
mdi("G49")
expect_type("G49 from the second head", 0)
mdi("G12.1 P2", "G43.4 H1")
expect_type("G43.4 from the identity, the lowest", 1)
mdi("G12.1 P3", "G43.4 H1")
expect_type("G43.4 again after G12.1 P3", 3)
mdi("G49")
drain()

# --- G53.5 is the machine frame, not the slides ------------------------------
# on the machine frame type G53 and G53.5 agree, and a Y word alone moves
# both slides: X is held, which the slanted slide would otherwise pull
j = mdi("G53 G0 X5 Y0 Z0 B0 C0")
expect("G53 to (5, 0, 0)", j, carriage(5, 0, 0))
j = mdi("G53.5 G0 Y10")
expect("G53.5 Y10 holds machine X", j, carriage(5, 10, 0))
j = mdi("G53.5 G0 X0")
expect("G53.5 X0 holds machine Y", j, carriage(0, 10, 0))

# under the tip kinematics with the head tilted and a tool on, G53.5 still
# means the carriage: the tilt and the tool length are left out.  The tilt
# holds the tip, so the carriage has moved by the tool's swing, and that is
# where X is held
mdi("G43.4 H1")
j = mdi("G0 B30")
expect_type("G43.4 with the head at B30", 1)
swing_x = TOOL * math.sin(math.radians(30))
swing_z = TOOL * (1 - math.cos(math.radians(30)))
expect("the tilt holds the tip", j, carriage(swing_x, 10, -swing_z, 30))
j = mdi("G53.5 G0 Y0 Z0")
expect("G53.5 Y0 Z0 under the tip, tilted", j, carriage(swing_x, 0, 0, 30))
j = mdi("G53.5 G0 X20 Y10 Z-5")
expect("G53.5 X20 Y10 Z-5 under the tip", j, carriage(20, 10, -5, 30))
j = mdi("G53.5 G0 B0")
expect("G53.5 B0 is the joint", j, carriage(20, 10, -5, 0))
# and G53 under the tip is refused: the config wants machine moves on the
# machine frame type, and the plain identity is not that type either
refused("G53 G0 X0", "G28", "G30", "G28.1", "G30.1")
mdi("G12.1 P2")
refused("G53 G0 X0", "G28.1")
mdi("G13.1")
drain()

# --- G28.5 goes to a machine frame position stored on the machine frame ----
# with no words every letter goes, B to its stored zero included; with words
# the machine passes through that machine frame point and only those
# letters go on to the stored position.  Under G43.4 a program Z is the
# tool length above the carriage, the offset the interpreter carries, so the
# waypoint's Z is 10 + 50
mdi("G49", "G53 G0 X20 Y10 Z0 B0 C0", "G28.1")
mdi("G43.4 H1", "G0 B30", "G0 X0 Y0 Z10")
j = mdi("G28.5")
expect("G28.5 with the head tilted", j, carriage(20, 10, 0, 0))
mdi("G0 X0 Y0 Z10")
j = mdi("G28.5 X40")
expect("G28.5 X40, through X then to X only", j, carriage(20, 0, 10 + TOOL, 0))
mdi("G49", "G0 B0", "G53 G0 X0 Y0 Z0")
drain()

# --- the program position agrees with the move ------------------------------
# the interpreter reports the tip where the joints put it, so a plain move
# after a machine frame move starts from the right place
mdi("G43.4 H1", "G0 B30", "G53.5 G0 X20 Y10 Z0")
s.poll()
before = list(s.position[:3])
j = mdi("G91 G0 X1", "G90")
after = carriage(20, 10, 0, 30)
after[0] += 1.0
expect("G91 X1 after G53.5", j, after)
s.poll()
d = [s.position[i] - before[i] for i in range(3)]
print("%-34s dx %.4f dy %.4f dz %.4f" % ("the tip moved", d[0], d[1], d[2]))
if abs(d[0] - 1.0) > 1e-4 or abs(d[1]) > 1e-4 or abs(d[2]) > 1e-4:
    error("the tip did not move by X1 alone")
mdi("G49", "G0 B0", "G53 G0 X0 Y0 Z0")
drain()

for f in ("sim.var", "sim.var.bak"):
    try:
        os.unlink(f)
    except OSError:
        pass

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
