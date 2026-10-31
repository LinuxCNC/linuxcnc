#!/usr/bin/env python3
# The point-to-point moves on a machine whose slides do not line up with its
# frame: see README.

import linuxcnc
import hal
import gcode
import preview_helpers
import sys
import os
import time
import math
from rs274.interpret import StatMixin

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
# and the config reads machine moves in the machine frame: under the tip G53
# is G53.5, the carriage at the words whatever the tilt and the tool, where
# the tip's G53 would take the tool length off along machine Z
mdi("G0 B30")
j = mdi("G53 G0 X0 Y0 Z0")
expect("G53 X0 Y0 Z0 under the tip, tilted", j, carriage(0, 0, 0, 30))
j = mdi("G53 G0 Y10")
expect("G53 Y10 under the tip holds machine X", j, carriage(0, 10, 0, 30))
# G28.1 stores where the carriage stands and G28 and G30 go back to it the
# same way, after a plain move under the tip has taken the carriage along
j = mdi("G53 G0 X20 Y10 Z-5", "G28.1", "G30.1", "G0 X0 Y0 Z10")
j = mdi("G28")
expect("G28 under the tip, tilted", j, carriage(20, 10, -5, 30))
b = mdi("G0 X0 Y0 Z10")
j = mdi("G30 Y0")
expect("G30 Y0, Y alone back to the store", j,
       carriage(b[0] + b[1] * math.cos(SLANT), 10, b[2], 30))
# the plain identity is not the machine frame type either: its letters name
# the joints, where the machine frame holds machine X as Y moves
mdi("G49", "G0 B0", "G12.1 P2")
j = mdi("G53 G0 X5 Y0 Z0 B0 C0")
expect("G53 to (5, 0, 0) on the plain identity", j, carriage(5, 0, 0))
j = mdi("G53 G0 Y10")
expect("G53 Y10 on the plain identity holds X", j, carriage(5, 10, 0))
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

# --- the preview reads the codes on the type the machine is on -------------
# the status buffer says which type that is and what each type is; a GUI
# restates where the machine stands with G53 ahead of the program, in the
# world of the type in force, which the flag leaves alone, and the program
# after it reads G53 as running it would, through the switches it makes.
# AXIS hands the restating codes over as a list, the Gremlin based screens
# as a unit code and an init code.
class PreviewCanon(StatMixin):
    # Stay on the per-event canon protocol: the catch-all below would
    # otherwise answer gcode.parse's probe for the move-batch one.
    use_move_batches = False

    def __init__(self, stat, parameter):
        StatMixin.__init__(self, stat, False)
        self.parameter_file = parameter
        self.end = None

    # where the last move ends, in the program's terms
    def straight_traverse(self, *args):
        self.end = args[-9:]

    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        return lambda *args, **kwargs: None

def preview(program, listed):
    ini = linuxcnc.ini(os.environ["INI_FILE_NAME"])
    s.poll()
    canon = PreviewCanon(s, ini.getstring("RS274NGC", "PARAMETER_FILE"))
    if listed:
        # as AXIS builds them: the tool, the position, the modal codes, and
        # the offset in force, which under the tip the kinematics evaluates
        axes = [i for i in range(9) if s.axis_mask & (1 << i)]
        codes = ["G21", "G90", "T%d M6" % s.tool_in_spindle]
        codes += ["G53 G0 %s%.8f" % ("XYZABCUVW"[i], s.position[i]) for i in axes]
        codes += ["G%.1f" % (g * .1) for i, g in enumerate(s.gcodes)
                  if i not in (0, 1, 2, 12) and g != -1]
        codes.append("G43.1 " + " ".join("%s%.8f" % ("XYZABCUVW"[i], s.tool_offset[i]) for i in axes))
        result, line = gcode.parse(program, canon, codes, "")
    else:
        codes = preview_helpers.create_unitcode_and_initcode(s, ini)
        result, line = gcode.parse(program, canon, *codes)
    if result > gcode.MIN_ERROR:
        return line, gcode.strerror(result), None
    return None, None, canon.end

# The program's G53 ends where G53.5 to the same words ends, in each form,
# a list or an init code; line 0 is the codes ahead of the program
def far(a, b):
    return a is None or b is None or max(abs(x - y) for x, y in zip(a, b)) > 1e-9

def expect_preview(what, program, same):
    for listed in (True, False):
        form = "list" if listed else "init code"
        line, why, end = preview(program, listed)
        print("%-34s %s" % ("%s, %s" % (what, form),
                            "line %d: %s" % (line, why) if why else "accepted"))
        if why:
            error("%s: the preview refused line %d" % (what, line))
        line, why, want = preview(same, listed)
        if why or far(end, want):
            error("%s, %s: the preview ends at %s, %s at %s" % (what, form, end, same, want))

# The restating G53 is the type in force's, never read in the machine frame:
# a move that goes nowhere ends on the position the machine reports.  The
# list restates the tool offset after the position, which moves it.
def expect_restated(what, forms):
    s.poll()
    want = [v if i in (3, 4, 5) else v / 25.4 for i, v in enumerate(s.position)]
    for listed in forms:
        line, why, end = preview("here.ngc", listed)
        if why or far(end, want):
            error("%s, %s: the restated position reads %s, not %s" %
                  (what, "list" if listed else "init code", end, want))

# and running it ends there too: with no tool offset in force the restated
# position after the run is where the program's preview ended
def expect_run(what, program, *lines):
    ends = [preview(program, listed)[2] for listed in (True, False)]
    here = mdi()
    mdi(*lines)
    for listed, end in zip((True, False), ends):
        there = preview("here.ngc", listed)[2]
        if far(end, there):
            error("%s, %s: the preview ends at %s, running it at %s" %
                  (what, "list" if listed else "init code", end, there))
    mdi("G53.7 G0 " + " ".join("J%d=%.9f" % (j, v) for j, v in enumerate(here)))

mdi("G13.1")
s.poll()
flags = s.kins_type_flags
print("%-34s kins_type %d, flags %s" % ("the status buffer", s.kins_type, flags))
if s.kins_type != kins_type():
    error("stat.kins_type %d, motion.kins-type %d" % (s.kins_type, kins_type()))
if (flags[0] & (linuxcnc.KINSTYPE_MACHINE | linuxcnc.KINSTYPE_IDENTITY) != linuxcnc.KINSTYPE_MACHINE
        or not flags[1] & linuxcnc.KINSTYPE_PRIMARY
        or not flags[2] & linuxcnc.KINSTYPE_IDENTITY or flags[-1] != -1):
    error("stat.kins_type_flags does not say what slantkins declares")
mdi("G53 G0 X5 Y10 Z0")
expect_preview("G53 on the machine frame type", "g53.ngc", "g535.ngc")
expect_restated("on the machine frame type", (True, False))
mdi("G43.4 H1", "G0 B30")
expect_type("G43.4 for the preview", 1)
expect_preview("G53 under the tip, tilted", "g53.ngc", "g535.ngc")
expect_preview("G13.1 then G53 under the tip", "g13-g53.ngc", "g13-g535.ngc")
expect_restated("under the tip, tilted", (False,))
mdi("G49", "G0 B0", "G12.1 P2")
expect_type("G12.1 P2 for the preview", 2)
expect_preview("G53 on the plain identity", "g53.ngc", "g535.ngc")
expect_restated("on the plain identity", (True, False))
expect_run("G53 on the plain identity", "g53.ngc", "G53 G0 X0")
mdi("G13.1")
drain()

for f in ("sim.var", "sim.var.bak"):
    try:
        os.unlink(f)
    except OSError:
        pass

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
