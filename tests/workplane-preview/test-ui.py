#!/usr/bin/env python3
# A tilted work plane set in MDI reaches the preview: the preview parse
# starts from the same plane as the machine and ends where the machine does.
import math, sys, time
import gcode, linuxcnc, linuxcnc_util, preview_helpers
from rs274.interpret import Translated

class Canon(Translated):
    def __init__(self, s):
        self.s = s
        self.parameter_file = "test.var"
        self.end = None
    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        return lambda *a, **k: None
    def set_g5x_offset(self, i, x, y, z, *rest):
        self.g5x_offset_x, self.g5x_offset_y, self.g5x_offset_z = x, y, z
    def set_g92_offset(self, x, y, z, *rest):
        self.g92_offset_x, self.g92_offset_y, self.g92_offset_z = x, y, z
    def set_xy_rotation(self, t):
        self.rotation_xy = t
        self.rotation_cos = math.cos(math.radians(t))
        self.rotation_sin = math.sin(math.radians(t))
    def set_g68_frame(self, x, y, z, *rest):
        self.g68_offset = (x, y, z)
        self.g68_rotation = rest[:9]
        self.g68_active = rest[9]
    def straight_feed(self, *a):
        self.end = self.rotate_and_translate(*a)[:3]
    def get_tool(self, pocket):
        return (-1,) + (0,) * 13
    def get_external_length_units(self):
        return self.s.linear_units
    def get_external_angular_units(self):
        return 1.0
    def get_axis_mask(self):
        return self.s.axis_mask
    def get_block_delete(self):
        return 0

result = open("result", "w")
def say(*a):
    print(*a, file=result, flush=True)

c = linuxcnc.command()
e = linuxcnc.error_channel()
s = linuxcnc.stat()
l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)
inifile = linuxcnc.ini("test.ini")
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MANUAL)
c.home(-1)
l.wait_for_home(joints=[1, 1, 1, 1, 0, 0, 0, 0, 0])

def errors():
    msgs = []
    while True:
        m = e.poll()
        if not m:
            return msgs
        msgs.append(m[1])

def mdi(*cmds):
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete()
    for cmd in cmds:
        c.mdi(cmd)
        c.wait_complete()
        l.wait_for_interp_state(linuxcnc.INTERP_IDLE)

def case(name, prog, *setup):
    mdi("G69", *setup)
    errors()
    s.poll()
    canon = Canon(s)
    res, seq = gcode.parse(prog, canon, *preview_helpers.create_unitcode_and_initcode(s, inifile))
    c.mode(linuxcnc.MODE_AUTO)
    c.wait_complete()
    c.program_open(prog)
    c.wait_complete()
    c.auto(linuxcnc.AUTO_RUN, 0)
    c.wait_complete()
    time.sleep(0.3)
    l.wait_for_interp_state(linuxcnc.INTERP_IDLE, timeout=30)
    s.poll()
    machine = errors()
    if res > gcode.MIN_ERROR:
        say("%s: preview line %d: %s; machine: %s" % (name, seq, gcode.strerror(res).strip(), "; ".join(machine)))
        return
    d = max(abs(canon.end[i] - s.position[i]) for i in range(3))
    say("%s: %s%s" % (name, "same end" if d < 1e-9 else "ends %g apart" % d,
                      "; machine: " + "; ".join(machine) if machine else ""))

case("plane", "move.ngc", "G21", "G68.2 X10 Y20 Z5 I10 J20 K30")
case("composed", "move.ngc", "G21", "G68.2 X10 Y20 Z5 I10 J20 K30", "G68.4 X2 J15")
case("composed then G68.4", "compose.ngc", "G21", "G68.2 X10 Y20 Z5 I10 J20 K30", "G68.4 X2 J15")
case("gimbal", "move.ngc", "G20", "G68.2 P1 X0.5 Y0.2 Z0.1 I20 J90 K30")
case("no plane", "compose.ngc", "G20")
case("unfinished MDI sequence", "move.ngc", "G20", "G68.2 P2 Q1 X0 Y0 Z0")
c.state(linuxcnc.STATE_OFF)
