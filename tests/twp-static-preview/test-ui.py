#!/usr/bin/env python3
# Status, a plane jog, an abort and the preview under the machine frame type: see README.

import os
import sys
import gcode
import preview_helpers
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

finish()
