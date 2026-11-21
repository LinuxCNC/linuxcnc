#!/usr/bin/env python3
# A coordinate system turned about X, Y and Z (G10 L2 I J K): see README.

import linuxcnc
import hal
import math
import os
import sys
import time
import gcode
import preview_helpers
from rs274 import glcanon_bake
from rs274.interpret import Translated

PHASE = os.environ.get("G5X_PHASE", "set")

ini = linuxcnc.ini(os.environ["INI_FILE_NAME"])
c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

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

def wait_for(what, cond, timeout=60):
    deadline = time.time() + timeout
    while time.time() < deadline:
        s.poll()
        if cond():
            return True
        time.sleep(0.02)
    error("timed out waiting for %s" % what)
    return False

def settled():
    last = [None]
    def still():
        now = tuple(s.joint_position[:3])
        done = s.inpos and not s.queue and now == last[0]
        last[0] = now
        return done
    wait_for("the move to end", still)

def mdi(cmd, expect=None):
    print("mdi:", cmd)
    c.mdi(cmd)
    c.wait_complete(60)
    if expect:
        # a refused line reaches the error channel by itself
        wait_for("the error for %r" % cmd, lambda: s.interp_state == linuxcnc.INTERP_IDLE)
        deadline = time.time() + 10
        while True:
            m = e.poll()
            if m:
                print("channel:", m)
                if expect in m[1]:
                    break
                error("reported: %s" % m[1])
            elif time.time() > deadline:
                error("%r: no error saying %r" % (cmd, expect))
                break
            else:
                time.sleep(0.02)
    settled()
    drain()

def close(a, b, tol=1e-6):
    return max(abs(p - q) for p, q in zip(a, b)) < tol

def machine():
    s.poll()
    return [s.actual_position[i] for i in range(3)]

def program():
    # the current point as the interpreter has it, #5420 to #5422
    c.mdi("(debug,here #5420 #5421 #5422)")
    c.wait_complete(60)
    deadline = time.time() + 10
    while time.time() < deadline:
        m = e.poll()
        if not m:
            time.sleep(0.02)
            continue
        print("channel:", m)
        if m[1].startswith("here "):
            return [float(v) for v in m[1].split()[1:4]]
        if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            error("reported: %s" % m[1])
    error("no current point from the interpreter")
    return [math.nan] * 3

def rot(yz, zx, xy, v):
    # Rz(xy) * Ry(zx) * Rx(yz) * v, as the documentation has it
    x, y, z = v
    a, b, t = math.radians(yz), math.radians(zx), math.radians(xy)
    y, z = y * math.cos(a) - z * math.sin(a), y * math.sin(a) + z * math.cos(a)
    x, z = x * math.cos(b) + z * math.sin(b), z * math.cos(b) - x * math.sin(b)
    x, y = x * math.cos(t) - y * math.sin(t), x * math.sin(t) + y * math.cos(t)
    return [x, y, z]

def unrot(yz, zx, xy, v):
    # the rotation back: Rx(-yz) * Ry(-zx) * Rz(-xy) * v
    return rot(-yz, 0, 0, rot(0, -zx, 0, rot(0, 0, -xy, v)))

def world(origin, angles, p):
    return [o + d for o, d in zip(origin, rot(*angles, p))]

def relative():
    # halui's relative position, the DRO a panel reads
    return [hal.get_value("halui.axis.%s.pos-relative" % a) for a in "xyz"]

def check_relative(name, want):
    if not wait_for("halui at %s" % want, lambda: close(relative(), want), timeout=10):
        error("%s: halui reads %s, not %s" % (name, relative(), want))

def angles():
    s.poll()
    return (s.rotation_yz, s.rotation_zx, s.rotation_xy)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()
wait_for("homing", lambda: all(s.homed[:3]))
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()

ORIGIN = [10.0, 20.0, 30.0]

if PHASE == "restore":
    # the var file the first run left: G55 turned I-90 J0 R15
    mdi("G55")
    if not close(angles(), (-90, 0, 15)):
        error("G55 comes back turned %s, not (-90, 0, 15)" % (angles(),))
    mdi("G0 X1 Y2 Z3")
    want = world(ORIGIN, (-90, 0, 15), (1, 2, 3))
    if not close(machine(), want):
        error("after the restart G55 X1 Y2 Z3 is at %s, not %s" % (machine(), want))
    print("Exiting with %d errors" % errors)
    sys.exit(0)

# a turn about Y: program X lies along machine -Z
mdi("G10 L2 P1 X10 Y20 Z30 I0 J90 K0")
mdi("G54")
if not close(angles(), (0, 90, 0)):
    error("status has the rotation %s, not (0, 90, 0)" % (angles(),))
mdi("G0 X1 Y2 Z3")
want = world(ORIGIN, (0, 90, 0), (1, 2, 3))
if not close(machine(), want):
    error("J90: X1 Y2 Z3 is at %s, not %s" % (machine(), want))
check_relative("J90", [1, 2, 3])
if not close(program(), [1, 2, 3]):
    error("J90: the interpreter has the current point at %s" % program())

# the tool stays where it is when the rotation changes, and its
# coordinates follow; a G92 sits inside the rotation and turns with it
mdi("G92 X0 Y0 Z0")
before = machine()
mdi("G10 L2 P1 J0 I30 R40")
if not close(machine(), before):
    error("G10 L2 moved the machine from %s to %s" % (before, machine()))
want = [a - b for a, b in zip(unrot(30, 0, 40, [p - o for p, o in zip(before, ORIGIN)]), (1, 2, 3))]
if not close(program(), want):
    error("after G10 L2 the interpreter has the point at %s, not %s" % (program(), want))
check_relative("G10 L2 under G92", want)
mdi("G0 X0 Y0 Z0")
want = world(ORIGIN, (30, 0, 40), (1, 2, 3))
if not close(machine(), want):
    error("G92 zero on the turned system is at %s, not %s" % (machine(), want))
mdi("G92.1")

# K is R
mdi("G10 L2 P1 I0 J0 K25")
if not close(angles(), (0, 0, 25)):
    error("K25 gives the rotation %s" % (angles(),))
mdi("G10 L2 P1 R5 K5", expect="as R or as K, not both")

# an arc in the program's XY plane on a system turned -90 about X lies in
# the machine's XZ plane, around its centre there
mdi("G10 L2 P1 X10 Y20 Z30 I-90 J0 K0")
mdi("G0 X0 Y0 Z0")
mdi("F100")
c.mdi("G2 X2 Y0 I1 J0")
samples = []
end = world(ORIGIN, (-90, 0, 0), (2, 0, 0))
def sample():
    p = machine()
    samples.append(p)
    return s.inpos and not s.queue and close(p, end)
wait_for("the arc", sample)
c.wait_complete(60)
drain()
centre = world(ORIGIN, (-90, 0, 0), (1, 0, 0))
off_plane = max(abs(p[1] - centre[1]) for p in samples)
off_circle = max(abs(math.hypot(p[0] - centre[0], p[2] - centre[2]) - 1) for p in samples)
deepest = max(abs(p[2] - centre[2]) for p in samples)
print("arc: %d samples, off the plane %g, off the circle %g, deepest %g"
      % (len(samples), off_plane, off_circle, deepest))
if off_plane > 1e-5 or off_circle > 1e-5:
    error("the arc leaves its circle in the machine's XZ plane: %g off the plane, %g off the circle"
          % (off_plane, off_circle))
if deepest < 0.9:
    error("the arc never bows out of the X axis, deepest %g" % deepest)

# a tilted work plane sits on the turned system and turns with it
mdi("G68.2 X0 Y0 Z0 I0 J30 K0")
mdi("G0 X0 Y1 Z0")
want = world(ORIGIN, (-90, 0, 0), rot(30, 0, 0, (0, 1, 0)))
if not close(machine(), want):
    error("G68.2 on the turned system: Y1 is at %s, not %s" % (machine(), want))
mdi("G69")

# G10 L20 sets the coordinate given along the system's turned axes: its Z
# is machine +Y, so the origin moves along Y alone
mdi("G0 X1 Y2 Z3")
before = machine()
s.poll()
g5x = list(s.g5x_offset[:3])
mdi("G10 L20 P1 Z0")
s.poll()
moved = [a - b for a, b in zip(s.g5x_offset[:3], g5x)]
if not close(machine(), before):
    error("G10 L20 moved the machine")
if not close(moved, [0, 3, 0]):
    error("G10 L20 Z0 moved the origin by %s, not along Y by 3" % moved)
check_relative("G10 L20 Z0", [1, 2, 0])
mdi("G10 L20 P1 I5", expect="I J K not allowed in G10 L20")

# a tool offset is along the machine's axes
mdi("G10 L10 P1 Z1", expect="turned about X or Y")

# the preview draws the same points: the renderer and the Python canon
class Machine:
    def get_tool(self, pocket):
        return tuple(s.tool_table[pocket]) if 0 <= pocket < len(s.tool_table) else (-1,) + (0,) * 13
    def get_external_length_units(self):
        return s.linear_units
    def get_external_angular_units(self):
        return 1.0
    def get_axis_mask(self):
        return s.axis_mask
    def get_block_delete(self):
        return 0

class Rendered(Machine, gcode.RendererCanon):
    def __init__(self):
        self.program_geometry = glcanon_bake.ProgramGeometry(geometry="XYZ")
        self.parameter_file = "preview.var"
    def adopt_geometry(self, program):
        self.program_geometry.adopt(program, {"dwell": (1, 0, 0, 1), "m1xx": (0, 0, 1, 1)})
    def comment(self, text): pass
    def message(self, text): pass
    def check_abort(self): return False
    def next_line(self, state): pass
    def change_tool(self, pocket): pass

class Python(Machine, Translated):
    def __init__(self):
        self.parameter_file = "preview.var"
        self.ends = []
    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        return lambda *a, **k: None
    def straight_feed(self, *a):
        self.ends.append([25.4 * v for v in self.rotate_and_translate(*a)[:3]])

PROG = ("G21 G90 G17\nG10 L2 P2 X10 Y20 Z30 I-90 J0 R15\nG55\n"
        "G1 X1 Y2 Z3 F100\nG2 X3 Y2 I1 J0\nM2\n")
with open("preview.ngc", "w") as f:
    f.write(PROG)
for f in ("preview.var", "preview.var.bak"):
    if os.path.exists(f):
        os.unlink(f)
canon = Rendered()
res, seq = gcode.parse("preview.ngc", canon, preview_helpers.preview_initcodes(s, ini))
if res > gcode.MIN_ERROR:
    error("the renderer stops at line %d: %s" % (seq, gcode.strerror(res).strip()))
else:
    pts = [[25.4 * v for v in p] for p in canon.program_geometry.positions()]
    start = world(ORIGIN, (-90, 0, 15), (1, 2, 3))
    near = [i for i, p in enumerate(pts) if close(p, start, 1e-4)]
    if not near:
        error("the renderer draws no point at X1 Y2 Z3 of G55, %s" % start)
    else:
        centre = world(ORIGIN, (-90, 0, 15), (2, 2, 3))
        normal = rot(-90, 0, 15, (0, 0, 1))
        arc = pts[near[-1]:]
        worst = max(abs(math.dist(p, centre) - 1) for p in arc)
        tilt = max(abs(sum((a - b) * n for a, b, n in zip(p, centre, normal))) for p in arc)
        print("renderer: %d points, arc off the radius %g, off the plane %g" % (len(pts), worst, tilt))
        if len(arc) < 3 or worst > 1e-4 or tilt > 1e-4:
            error("the renderer's arc leaves its circle: %d points, %g off the radius, %g off the plane"
                  % (len(arc), worst, tilt))
canon = Python()
res, seq = gcode.parse("preview.ngc", canon, preview_helpers.preview_initcodes(s, ini))
if res > gcode.MIN_ERROR:
    error("the Python canon stops at line %d: %s" % (seq, gcode.strerror(res).strip()))
elif not canon.ends or not close(canon.ends[0], world(ORIGIN, (-90, 0, 15), (1, 2, 3))):
    error("the Python canon puts X1 Y2 Z3 of G55 at %s" % (canon.ends[:1],))
for f in ("preview.ngc", "preview.var", "preview.var.bak"):
    if os.path.exists(f):
        os.unlink(f)

# for the second run: G55 kept in the var file
mdi("G10 L2 P2 X10 Y20 Z30 I-90 J0 R15")
mdi("G54")
print("Exiting with %d errors" % errors)
