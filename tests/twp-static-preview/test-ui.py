#!/usr/bin/env python3
# Status, a plane jog, an abort and the preview under the machine frame type: see README.

import os
import sys
import threading
import collections
import gcode
import preview_helpers
import numpy as np
from rs274 import glcanon_bake
from rs274.interpret import Translated
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "twp-static"))
from static import *

# Status has the plane as defined under every type, and the second feature
# coordinate system apart, as Siemens keeps its workpiece reference frame
# apart from its table and tool ones: a switch changes only the latter.
IDENTITY = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)

def plane_status():
    s.poll()
    return (tuple(s.g68_offset[:3]), tuple(s.g68_rotation),
            tuple(s.g68_kins_offset[:3]), tuple(s.g68_kins_rotation))

def near(a, b):
    return max(abs(p - q) for p, q in zip(a, b)) < 1e-9

run(TCP, [plane, "G53.1"])
on_tcp = plane_status()
mdi("G13.1")
on_fixed = plane_status()
mdi(TCP)
back = plane_status()
if not near(on_tcp[0], (10, -5, 0)):
    error("status has the plane's origin at %s, not X10 Y-5 Z0" % show(on_tcp[0]))
for name, got in (("under G13.1", on_fixed), ("back on the part's type", back)):
    if not near(got[0], on_tcp[0]) or not near(got[1], on_tcp[1]):
        error("%s status has the plane at %s %s, not %s %s"
              % (name, show(got[0]), show(got[1]), show(on_tcp[0]), show(on_tcp[1])))
for name, got in (("on the part's type", on_tcp), ("back on the part's type", back)):
    if not near(got[2], (0, 0, 0)) or not near(got[3], IDENTITY):
        error("%s status has a second feature coordinate system %s %s"
              % (name, show(got[2]), show(got[3])))
if near(on_fixed[2], (0, 0, 0)) and near(on_fixed[3], IDENTITY):
    error("under G13.1 status has no second feature coordinate system")

# The plane jog frame under G13.1 is the plane turned by the second feature
# coordinate system: a jog along the plane's X goes where G1 X+5 does.
def jog_plane_x():
    c.mode(linuxcnc.MODE_MANUAL)
    done()
    c.teleop_enable(1)
    done()
    c.jog_frame(linuxcnc.JOG_FRAME_PLANE)
    done()
    c.jog(linuxcnc.JOG_INCREMENT, False, 0, 500, 5)
    done()
    # in teleop motion keeps no in position: the joints still is the end
    after = still()
    drain()
    c.jog_frame(linuxcnc.JOG_FRAME_MACHINE)
    done()
    c.mode(linuxcnc.MODE_MDI)
    done()
    return after

cut_x = run("G13.1", [plane, "G53.1", "G0 X10 Y5 Z20", "G91 G1 X5 F60000", "G90"])[-1]
run("G13.1", [plane, "G53.1", "G0 X10 Y5 Z20"])
jogged = jog_plane_x()
# a jog stops within its planner's dead band of the end, the acceleration
# times the servo period squared over a thousand, on each axis
band = 2 * float(ini.find("AXIS_X", "MAX_ACCELERATION")) * (float(ini.find("EMCMOT", "SERVO_PERIOD")) * 1e-9) ** 2 / 1000
if max(abs(p - q) for p, q in zip(jogged, cut_x)) > band:
    error("a plane X jog under G13.1 ended at %s, G1 X+5 at %s" % (show(jogged), show(cut_x)))

# An abort with RETAIN_WORK_PLANE keeps the plane the machine was in, and
# its second feature coordinate system, while the read ahead has gone on
# to another: the machine's plane comes back onto the part it was
# defined on.
if ini.find("RS274NGC", "RETAIN_WORK_PLANE") == "1":
    want = run("G13.1", [plane, "G53.1", "G1 X-10 Y5 Z20 F60000"])[-1]
    run("G13.1", [plane, "G53.1", "G1 X10 Y5 Z20 F60000"])
    with open("abort.ngc", "w") as f:
        f.write("G1 X-200 F60\nG69\nG68.2 X0 Y0 Z0 I0 J45 K0\nG1 X0\nM2\n")
    c.mode(linuxcnc.MODE_AUTO)
    done()
    c.program_open("abort.ngc")
    done()
    c.auto(linuxcnc.AUTO_RUN, 0)
    # the read ahead past the other plane, the machine on its first move
    deadline = time.time() + 30
    while time.time() < deadline:
        s.poll()
        if s.read_line >= 4 and s.motion_line == 1 and s.current_vel > 0:
            break
        time.sleep(0.002)
    else:
        error("abort.ngc never read ahead past its plane: line %d, the machine on %d"
              % (s.read_line, s.motion_line))
    c.abort()
    done()
    settled()
    drain()
    c.mode(linuxcnc.MODE_MDI)
    done()
    got = mdi("G1 X-10 Y5 Z20 F60000")
    if not same(got, want):
        error("after an abort the plane put X-10 Y5 Z20 at %s, not %s" % (show(got), show(want)))
    os.unlink("abort.ngc")

# The preview's interpreter: a program after a plane set in MDI starts
# from the plane as status has it, folded into the second feature
# coordinate system, and one with its own plane takes its own; through the
# plane canon applies, which a callback canon gets, both end where the
# machine does, without the tool offset.
class Machine:
    # what a preview reads off the machine
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
    def get_external_joint_positions(self):
        return tuple(s.joint_actual_position[:s.joints])
    def get_external_g68_kins_frame(self):
        return (tuple(s.g68_kins_offset[:3]), tuple(s.g68_kins_rotation))
    def get_kinematics_type(self):
        return s.kinematics_type
    def get_kins_type(self):
        return s.kins_type
    def get_kins_type_flags(self):
        return s.kins_type_flags

class Canon(Machine, Translated):
    def __init__(self):
        self.parameter_file = "preview.var"
        self.end = None
    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        return lambda *a, **k: None
    def straight_feed(self, *a):
        self.end = self.rotate_and_translate(*a)[:3]

def preview_case(name, prog, text):
    with open(prog, "w") as f:
        f.write(text)
    s.poll()
    canon = Canon()
    res, seq = gcode.parse(prog, canon, preview_helpers.preview_initcodes(s, ini))
    if res > gcode.MIN_ERROR:
        error("%s: the preview stops at line %d: %s" % (name, seq, gcode.strerror(res).strip()))
        return
    c.mode(linuxcnc.MODE_AUTO)
    done()
    c.program_open(prog)
    done()
    c.auto(linuxcnc.AUTO_RUN, 0)
    done()
    settled()
    drain()
    c.mode(linuxcnc.MODE_MDI)
    done()
    s.poll()
    machine = [s.position[i] - s.tool_offset[i] for i in range(3)]
    # the preview works in inches
    end = [25.4 * v for v in canon.end]
    if max(abs(p - q) for p, q in zip(end, machine)) > 1e-6:
        error("%s: the preview ends at %s, the machine at %s" % (name, end, machine))
    os.unlink(prog)

# the tool in the spindle, so that the G43 the preview restates has its length
for cmd in (RESET, START, "T1 M6", "G43", plane, "G53.1"):
    mdi(cmd)
preview_case("plane from MDI", "mdi-plane.ngc",
             "G1 X-10 Y5 Z20 F60000\nG1 Z-5\nM2\n")
# a rotary turned since the plane was set keeps the second feature
# coordinate system it was cut through, which the preview takes from status
for cmd in (RESET, START, "T1 M6", "G43", plane, "G53.1",
            "G91 G0 %s30" % SECOND, "G90"):
    mdi(cmd)
preview_case("plane from MDI, a rotary turned since", "mdi-turned.ngc",
             "G1 X-10 Y5 Z20 F60000\nG1 Z-5\nM2\n")
for cmd in (RESET, START):
    mdi(cmd)
preview_case("plane in the program", "own-plane.ngc",
             "G43 H1\n%s\nG53.1\nG1 X-10 Y5 Z20 F60000\nG1 Z-5\nM2\n" % plane)

# What the preview draws is on the part, as under the part's type, where
# the program cuts: the plane as defined and the moves in it, not the
# slides that cut them. The same program, under G13.1 and under the part's
# type, draws one plane in the same place and ends at the same point.
# The program as the renderer hands it to a GUI, with no display: a
# headless build has no OpenGL.
Plane = collections.namedtuple("Plane", "origin axes")

class Rendered(Machine, gcode.RendererCanon):
    COLORS = {"dwell": (1, 0, 0, 1), "m1xx": (0, 0, 1, 1)}
    def __init__(self):
        self.program_geometry = glcanon_bake.ProgramGeometry(geometry="XYZ")
        self.parameter_file = "preview.var"
        self.workplanes = []
    def adopt_geometry(self, program):
        self.program_geometry.adopt(program, self.COLORS)
        self.workplanes = [Plane(origin, axes)
                           for _line, origin, axes, _lo, _hi in program.workplanes()]
    def comment(self, text): pass
    def message(self, text): pass
    def check_abort(self): return False
    def next_line(self, state): pass
    def change_tool(self, pocket): pass

def rendered(name, text, axes=False):
    prog = "rendered.ngc"
    with open(prog, "w") as f:
        f.write(text)
    s.poll()
    canon = Rendered()
    canon.program_geometry.configure(want_axis_positions=axes)
    res, seq = gcode.parse(prog, canon, preview_helpers.preview_initcodes(s, ini))
    os.unlink(prog)
    if res > gcode.MIN_ERROR:
        error("%s: the preview stops at line %d: %s" % (name, seq, gcode.strerror(res).strip()))
        return None
    return canon

def close(a, b):
    # the drawn points are single precision
    return max(abs(p - q) for p, q in zip(a, b)) < 1e-5

for cmd in (RESET, START):
    mdi(cmd)
body = "G43 H1\n%s\nG53.1\nG1 X-10 Y5 Z20 F60000\nG1 Z-5\nM2\n" % plane
on_part = rendered("drawn under the part's type", TCP + "\n" + body)
on_g131 = rendered("drawn under G13.1", "G13.1\n" + body)
if on_part and on_g131:
    a, b = on_part.workplanes, on_g131.workplanes
    if len(a) != 1 or len(b) != 1:
        error("the preview draws %d plane(s) under the part's type and %d under G13.1, not one"
              % (len(a), len(b)))
    elif not close(a[0].origin, b[0].origin) or not all(close(u, v) for u, v in zip(a[0].axes, b[0].axes)):
        error("the preview draws the plane under G13.1 at %s %s, under the part's type at %s %s"
              % (show(b[0].origin), b[0].axes, show(a[0].origin), a[0].axes))
    end_part = on_part.program_geometry.positions()[-1]
    end_g131 = on_g131.program_geometry.positions()[-1]
    if not close(end_part, end_g131):
        error("the preview ends the cut under G13.1 at %s, under the part's type at %s"
              % (show(end_g131), show(end_part)))

# The live plot is on the part too, so the tool marker that stands at its
# end is: under G13.1 in the plane the point the machine plots is the one
# the part's type plots at the same joints, and a switch to that type and
# back moves it nowhere and draws no line, at the moment the type has
# changed and the plane's map not yet too.
lp = linuxcnc.positionlogger(linuxcnc.stat(), (255, 255, 0, 255), (255, 0, 0, 255),
                             (0, 255, 0, 255), (0, 0, 255, 255), (255, 255, 255, 255),
                             (0, 255, 255, 255), "XYZ", 0)
sampler = threading.Thread(target=lp.start, args=(0.002,), daemon=True)
sampler.start()

def plotted():
    # the plot once it has caught up with the tool on the part in status,
    # or where it stands after a second
    deadline = time.time() + 1
    while True:
        s.poll()
        status = [s.part_position[i] - s.tool_offset[i] for i in range(3)]
        plot = lp.last(0)[:3]
        if max(abs(p - q) for p, q in zip(status, plot)) < 1e-4 or time.time() > deadline:
            return plot
        time.sleep(0.002)

def logged():
    raw, n, xyuv = lp.points()
    return np.frombuffer(raw, dtype=glcanon_bake.LOGGER_DTYPE, count=n)

run(TCP, [plane, "G53.1", "G1 X-10 Y5 Z20 F60000"])
on_part = plotted()
s.poll()
tool_on_part = s.tool_frame
run("G13.1", [plane, "G53.1", "G1 X-10 Y5 Z20 F60000"])
on_g131 = plotted()
if max(abs(p - q) for p, q in zip(on_part, on_g131)) > 1e-4:
    error("under G13.1 the live plot is at %s, under the part's type at %s"
          % (show(on_g131), show(on_part)))
# and the tool marker stands where the part's type has the tool against
# the part
s.poll()
if s.tool_frame is None or tool_on_part is None \
        or max(abs(p - q) for p, q in zip(s.tool_frame, tool_on_part)) > 1e-9:
    error("under G13.1 status has the tool frame %s, under the part's type %s"
          % (s.tool_frame, tool_on_part))
start = len(logged())
for i in range(3):
    mdi(TCP)
    mdi("G13.1")
back = plotted()
if max(abs(p - q) for p, q in zip(on_g131, back)) > 1e-4:
    error("a switch to the part's type and back moved the live plot from %s to %s"
          % (show(on_g131), show(back)))
for q in logged()[max(start - 1, 0):]:
    if np.abs(q['pos'] - np.asarray(back)).max() > 1e-3:
        error("a switch in the plane drew a line in the live plot to %s" % show(q['pos']))
        break

# G53.6 under G13.1 is drawn as the machine runs it, the tip held on the
# part all along, as under the part's type: the same vertices, and no loop
# where the slides swing round the tip.
for cmd in (RESET, START):
    mdi(cmd)
body = "G43 H1\n%s\nG1 X0 Y0 Z30 F60000\nG53.6\nM2\n" % plane
on_part = rendered("G53.6 drawn under the part's type", TCP + "\n" + body)
on_g131 = rendered("G53.6 drawn under G13.1", "G13.1\n" + body)
def g536_drawn(canon):
    # the vertices of the G53.6 line, the fifth, that are drawn
    pg = canon.program_geometry
    keep = (np.asarray(pg.lines) == 5) & (np.asarray(pg.kinds) != glcanon_bake.KIND_NOOP)
    return 25.4 * np.asarray(pg.positions())[keep]
if on_part and on_g131:
    a = 25.4 * np.asarray(on_part.program_geometry.positions())
    b = 25.4 * np.asarray(on_g131.program_geometry.positions())
    tip = g536_drawn(on_g131)
    if len(tip) == 0 or a.shape != b.shape:
        error("the preview draws the program with %d vertices under G13.1, %d under the part's type"
              % (len(b), len(a)))
    else:
        worst = np.max(np.abs(a - b))
        spread = np.max(np.ptp(tip, axis=0))
        print("G53.6 drawn under G13.1: %d vertices within %.6f of the part's type, spread %.6f"
              % (len(b), worst, spread))
        if worst > 1e-3:
            error("the preview draws G53.6 under G13.1 up to %.4f away from the part's type" % worst)
        if spread > 1e-3:
            error("the preview draws the tip of G53.6 under G13.1 moving %.4f" % spread)

# With no plane, under G13.1 alone, the preview and the live plot are on
# the part as well: the slides put the tool on the part where the part's
# type reads the joints. A rotary turned under G13.1 sweeps the tool round
# the part, which the preview draws through the points the machine passes.
def on_part_now():
    # status, then the live plot, where the tool stands against the part
    s.poll()
    status = [s.part_position[i] - s.tool_offset[i] for i in range(3)]
    plot = plotted()
    if max(abs(p - q) for p, q in zip(status, plot)) > 1e-3:
        error("the live plot is at %s, status has the tool on the part at %s"
              % (show(plot), show(status)))
    return status

def drawn_at(canon, where, letter, value):
    # the vertex drawn where the axis stands at a value, in machine units
    pg = canon.program_geometry
    turned = np.abs(pg.axis_position(letter) - value) < 1e-4
    turned &= pg.kinds != glcanon_bake.KIND_NOOP
    if not turned.any():
        error("%s: the preview draws no point at %s%g" % (where, letter, value))
        return None
    return [25.4 * v for v in pg.positions()[np.nonzero(turned)[0][-1]]]

turn = ["G43 H1", "G0 %s30 %s20" % (FIRST, SECOND), "G1 X10 Y5 Z20 F60000",
        "G1 %s60 F60000" % SECOND]
for cmd in (RESET, START):
    mdi(cmd)
canon = rendered("a rotary turned under G13.1", "\n".join(turn) + "\nM2\n", axes=True)
run("G13.1", turn[:-1] + ["G1 %s40 F60000" % SECOND])
middle = on_part_now()
mdi(TCP)
s.poll()
tcp = [s.position[i] - s.tool_offset[i] for i in range(3)]
if max(abs(p - q) for p, q in zip(middle, tcp)) > 1e-6:
    error("status has the tool on the part at %s under G13.1, the part's type at %s"
          % (show(middle), show(tcp)))
mdi("G13.1")
mdi("G1 %s60 F60000" % SECOND)
end = on_part_now()
if canon:
    for where, value, want in (("half way round", 40, middle), ("at the end", 60, end)):
        got = drawn_at(canon, "a rotary turned under G13.1, " + where, SECOND, value)
        if got and max(abs(p - q) for p, q in zip(got, want)) > 1e-3:
            error("a rotary turned under G13.1, %s: the preview draws %s, the machine is at %s"
                  % (where, show(got), show(want)))

# A switch moves no joint: the preview's interpreter reads the point back
# from the joints under the new type, as the machine does, and goes on
# from there, with a rotary turned and no plane, both ways.
tilt = "G0 %s30 %s20" % (FIRST, SECOND)
switches = {
    "a switch to the part's type": ["G43 H1", tilt, "G1 X10 Y5 Z20 F60000", TCP,
                                    "G91 G1 X5", "G90"],
    "a switch to G13.1": [TCP, "G43 H1", tilt, "G1 X10 Y5 Z20 F60000", "G13.1", "G1 X5"],
}
for name, lines in switches.items():
    for cmd in (RESET, START):
        mdi(cmd)
    canon = rendered(name, "\n".join(lines) + "\nM2\n")
    run("G13.1", lines)
    end = on_part_now()
    if canon:
        got = [25.4 * v for v in canon.program_geometry.positions()[-1]]
        if max(abs(p - q) for p, q in zip(got, end)) > 1e-3:
            error("after %s the preview ends at %s, the machine at %s"
                  % (name, show(got), show(end)))
lp.stop()
sampler.join(5)

finish()
