#!/usr/bin/env python3
# 3+2 under the machine frame type, the second feature coordinate system:
# see README.

import linuxcnc
import sys
import os
import time
import gcode
import preview_helpers
from rs274.interpret import Translated

ini = linuxcnc.ini(os.environ["INI_FILE_NAME"])
HEAD = ini.find("KINS", "KINEMATICS").startswith("5axiskins")
JOINTS = int(ini.find("KINS", "JOINTS"))
# the type that follows the part, and the rotaries' letters
TCP = "G12.1 P0" if HEAD else "G12.1 P1"
FIRST, SECOND = ("B", "C") if HEAD else ("A", "C")
START = "G53 G0 X20 Y-10 Z120 B0 C0" if HEAD else "G53 G0 X20 Y-10 Z120 A0 C0"

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

def drain(expect_error=False):
    got = False
    while True:
        m = e.poll()
        if not m:
            return got
        print("channel:", m)
        if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            got = True
            if not expect_error:
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

def mdi(cmd):
    c.mdi(cmd)
    c.wait_complete(60)
    j = settled()
    drain()
    return j

def run(select, steps):
    # from one machine point, on the type asked for, the joints after each step
    for cmd in ("G49", "G69", "G13.1", START, select):
        mdi(cmd)
    return [mdi(cmd) for cmd in steps]

def same(a, b, n=None):
    n = JOINTS if n is None else n
    return max(abs(p - q) for p, q in zip(a[:n], b[:n])) < 1e-6

def show(j):
    return " ".join("%.6f" % v for v in j)

# the same program under the type that follows the part and under the
# machine frame type: the machine goes to the same joints
plane = "G68.2 X10 Y-5 Z0 I60 J30 K0"
cut = ["G0 X10 Y5 Z20", "G1 X-10 F2000", "G2 X10 R10 F2000", "G1 Z-5"]
programs = {
    "no tool": [plane, "G53.1"] + cut,
    "tool first": ["G43 H1", plane, "G53.1"] + cut,
    "tool in the plane": [plane, "G53.1", "G43 H1"] + cut,
    "G53.6": [plane, "G0 X0 Y0 Z30", "G53.6"] + cut,
    "G68.3": [plane, "G53.1", "G69", "G68.3 X10 Y-5 Z0 R20"] + cut,
}
for name, steps in programs.items():
    tcp = run(TCP, steps)
    fixed = run("G13.1", steps)
    for cmd, a, b in zip(steps, tcp, fixed):
        if not same(a, b):
            error("%s: after %s the machine frame type is at %s, the part's type at %s"
                  % (name, cmd, show(b), show(a)))
    print("%s: %s" % (name, show(fixed[-1])))

# a rotary word moves the rotary alone and the frame stays where the
# plane was taken (Fanuc); a new plane at the turned table follows it
steps = [plane, "G53.1", "G0 X10 Y5 Z20", "G91", "G0 %s30" % SECOND, "G90",
         "G0 X10 Y5 Z20"]
fixed = run("G13.1", steps)
if not same(fixed[2], fixed[4], 3):
    error("a %s word moved the slides: %s to %s" % (SECOND, show(fixed[2]), show(fixed[4])))
if not same(fixed[2], fixed[6], 3):
    error("after a %s word the frame moved: %s to %s" % (SECOND, show(fixed[2]), show(fixed[6])))
tcp = run(TCP, [plane, "G53.1", "G91", "G0 %s30" % SECOND, "G90", "G1 X-10 Y5 Z20 F2000"])
fixed = run("G13.1", [plane, "G53.1", "G91", "G0 %s30" % SECOND, "G90",
                      "G68.4 X0 Y0 Z0 I0 J0 K0", "G1 X-10 Y5 Z20 F2000"])
if not same(tcp[-1], fixed[-1]):
    error("G68.4 after a %s word: the machine frame type is at %s, the part's type at %s"
          % (SECOND, show(fixed[-1]), show(tcp[-1])))

# G53 stays in the machine frame, and G69 leaves plain machine moves
fixed = run("G13.1", [plane, "G53.1", "G53 G0 X0 Y0 Z100", "G69", "G0 X10 Y0 Z50"])
if not same(fixed[2], [0, 0, 100], 3):
    error("G53 in the plane went to %s, not X0 Y0 Z100" % show(fixed[2]))
if not same(fixed[4], [10, 0, 50], 3):
    error("G0 after G69 went to %s, not X10 Y0 Z50" % show(fixed[4]))

# the type can change in the plane: on to the part's type and back
tcp = run(TCP, [plane, "G53.1", "G1 X-10 Y5 Z20 F2000"])
fixed = run("G13.1", [plane, "G53.1", TCP, "G13.1", "G1 X-10 Y5 Z20 F2000"])
if not same(tcp[-1], fixed[-1]):
    error("a switch in the plane: %s, the part's type at %s" % (show(fixed[-1]), show(tcp[-1])))

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
    c.wait_complete(30)
    c.teleop_enable(1)
    c.wait_complete(30)
    c.jog_frame(linuxcnc.JOG_FRAME_PLANE)
    c.wait_complete(30)
    c.jog(linuxcnc.JOG_INCREMENT, False, 0, 50, 5)
    time.sleep(0.3)
    after = settled()
    drain()
    c.jog_frame(linuxcnc.JOG_FRAME_MACHINE)
    c.wait_complete(30)
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete(30)
    return after

cut_x = run("G13.1", [plane, "G53.1", "G0 X10 Y5 Z20", "G91 G1 X5 F2000", "G90"])[-1]
run("G13.1", [plane, "G53.1", "G0 X10 Y5 Z20"])
jogged = jog_plane_x()
if not same(jogged, cut_x):
    error("a plane X jog under G13.1 ended at %s, G1 X+5 at %s" % (show(jogged), show(cut_x)))

# An abort with RETAIN_WORK_PLANE keeps the plane the machine was in, and
# its second feature coordinate system, while the read ahead has gone on
# to another: the machine's plane comes back onto the part it was
# defined on.
if ini.find("RS274NGC", "RETAIN_WORK_PLANE") == "1":
    want = run("G13.1", [plane, "G53.1", "G1 X-10 Y5 Z20 F2000"])[-1]
    run("G13.1", [plane, "G53.1", "G1 X10 Y5 Z20 F2000"])
    with open("abort.ngc", "w") as f:
        f.write("G1 X-200 F60\nG69\nG68.2 X0 Y0 Z0 I0 J45 K0\nG1 X0\nM2\n")
    c.mode(linuxcnc.MODE_AUTO)
    c.wait_complete()
    c.program_open("abort.ngc")
    c.wait_complete()
    c.auto(linuxcnc.AUTO_RUN, 0)
    time.sleep(2.0)
    c.abort()
    c.wait_complete()
    deadline = time.time() + 30
    while time.time() < deadline:
        s.poll()
        if s.interp_state == linuxcnc.INTERP_IDLE and not s.queue:
            break
        time.sleep(0.05)
    drain()
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete()
    got = mdi("G1 X-10 Y5 Z20 F2000")
    if not same(got, want):
        error("after an abort the plane put X-10 Y5 Z20 at %s, not %s" % (show(got), show(want)))
    os.unlink("abort.ngc")

# The preview: a program after a plane set in MDI starts from the plane
# as status has it, folded into the second feature coordinate system,
# and one with its own plane takes its own; both end where the machine
# does, which the preview draws without the tool offset.
class Canon(Translated):
    def __init__(self):
        self.parameter_file = "preview.var"
        self.end = None
    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        return lambda *a, **k: None
    def straight_feed(self, *a):
        self.end = self.rotate_and_translate(*a)[:3]
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

def preview_case(name, prog, text):
    with open(prog, "w") as f:
        f.write(text)
    s.poll()
    canon = Canon()
    res, seq = gcode.parse(prog, canon, *preview_helpers.create_unitcode_and_initcode(s, ini))
    if res > gcode.MIN_ERROR:
        error("%s: the preview stops at line %d: %s" % (name, seq, gcode.strerror(res).strip()))
        return
    c.mode(linuxcnc.MODE_AUTO)
    c.wait_complete()
    c.program_open(prog)
    c.wait_complete()
    c.auto(linuxcnc.AUTO_RUN, 0)
    c.wait_complete()
    time.sleep(0.3)
    settled()
    deadline = time.time() + 60
    while time.time() < deadline:
        s.poll()
        if s.interp_state == linuxcnc.INTERP_IDLE:
            break
        time.sleep(0.05)
    drain()
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete()
    s.poll()
    machine = [s.position[i] - s.tool_offset[i] for i in range(3)]
    # the preview works in inches
    end = [25.4 * v for v in canon.end]
    if max(abs(p - q) for p, q in zip(end, machine)) > 1e-6:
        error("%s: the preview ends at %s, the machine at %s" % (name, end, machine))
    os.unlink(prog)

# the tool in the spindle, so that the G43 the preview restates has its length
for cmd in ("G49", "G69", "G13.1", START, "T1 M6", "G43", plane, "G53.1"):
    mdi(cmd)
preview_case("plane from MDI", "mdi-plane.ngc",
             "G1 X-10 Y5 Z20 F2000\nG1 Z-5\nM2\n")
# a rotary turned since the plane was set keeps the second feature
# coordinate system it was cut through, which the preview takes from status
for cmd in ("G49", "G69", "G13.1", START, "T1 M6", "G43", plane, "G53.1",
            "G91 G0 %s30" % SECOND, "G90"):
    mdi(cmd)
preview_case("plane from MDI, a rotary turned since", "mdi-turned.ngc",
             "G1 X-10 Y5 Z20 F2000\nG1 Z-5\nM2\n")
for cmd in ("G49", "G69", "G13.1", START):
    mdi(cmd)
preview_case("plane in the program", "own-plane.ngc",
             "G43 H1\n%s\nG53.1\nG1 X-10 Y5 Z20 F2000\nG1 Z-5\nM2\n" % plane)

mdi("G69")
mdi("G49")

for f in ("sim.var", "sim.var.bak", "preview.var"):
    try:
        os.unlink(f)
    except OSError:
        pass

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
