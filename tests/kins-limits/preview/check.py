#!/usr/bin/env python3
# The preview follows the joints along each move through the kinematics and
# reports where one would leave its travel, the move task refuses at run
# time; a GUI warns before the run.  Each case is a program and the moves
# reported, as (line, joint, side).
import gcode
import linuxcnc
import os
import sys
from rs274.interpret import Translated

errors = 0

def error(what):
    global errors
    errors += 1
    print("*** ERROR %s" % what)

class Canon(Translated):
    # what a preview reads off a machine on its 5axiskins type, nothing homed
    kinematics_type = linuxcnc.KINEMATICS_BOTH
    def __init__(self):
        self.parameter_file = "preview.var"
        self.left = []
    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        return lambda *a, **k: None
    def get_tool(self, pocket):
        return (-1,) + (0,) * 13
    def get_external_length_units(self):
        return 1.0
    def get_external_angular_units(self):
        return 1.0
    def get_axis_mask(self):
        return 0x137                    # X Y Z B C W
    def get_block_delete(self):
        return 0
    def get_external_joint_positions(self):
        return ()
    def get_kinematics_type(self):
        return self.kinematics_type
    def get_kins_type(self):
        return 0
    def get_kins_type_flags(self):
        return (linuxcnc.KINSTYPE_PRIMARY,
                linuxcnc.KINSTYPE_IDENTITY | linuxcnc.KINSTYPE_MACHINE, 0) + (-1,) * 13
    def joint_travel(self, line, joint, side, where, limit):
        self.left.append((line, joint, side, where, limit))

def case(name, text, want, canon=None):
    with open("case.ngc", "w") as f:
        f.write("G21 G90 G17\nG12.1 P0\n" + text + "\nM2\n")
    canon = canon or Canon()
    res, seq = gcode.parse("case.ngc", canon, "G21", "")
    if res > gcode.MIN_ERROR:
        error("%s: the preview stops at line %d: %s" % (name, seq, gcode.strerror(res).strip()))
        return []
    got = [(line, joint, side) for line, joint, side, where, limit in canon.left]
    if got != want:
        error("%s: reported %s, not %s" % (name, canon.left, want))
    else:
        print("%s: %s" % (name, canon.left))
    return canon.left

# the head laid over to B90, the tip held while C turns: the carriage
# swings round the tip at the pivot plus the tool, to Y+ that at C-90,
# against joint 1's MAX_LIMIT of 400.  A tool of 0.05 peaks 0.05 past it
# between two samples 1.2 inside, both ways round; -0.05 stays inside.
SWING = "G43.1 Z%s\nG0 X0 Y0 Z0 B90 C-5\nG0 C-175\nG0 C-5"
left = case("swing past", SWING % "0.05", [(5, 1, 1), (6, 1, 1)])
if left and abs(left[0][3] - 400.05) > 1e-3:
    error("the swing peaks at %.4f, not 400.05" % left[0][3])
case("swing inside", SWING % "-0.05", [])
case("swing samples past", SWING % "100", [(5, 1, 1), (6, 1, 1)])

# a parse starts from nowhere the preview knows, the GUI's restating move
# going to where the machine stands: the first move is not followed from
# zero, only its end checked
case("first move", "G43.1 Z100\nG0 X0 Y0 Z0 B90 C-175", [])
case("first move past", "G43.1 Z100\nG0 X0 Y0 Z0 B90 C-90", [(4, 1, 1)])

# with the head upright Y is joint 1: an arc whose ends are inside and whose
# top is past, one way round, and the same arc the other way round, under
case("arc", "G0 X-100 Y350 Z0 B0 C0\nG2 X100 Y350 I100 J-50\nG0 X-100 Y350\n"
     "G3 X100 Y350 I100 J-50", [(4, 1, 1)])
# in the ZX plane, one way round over Z's MAX_LIMIT of 1000
case("arc zx", "G18 G0 X0 Y0 Z900 B0 C0\nG2 X300 Z900 I150 K0\nG0 X0 Z900\n"
     "G3 X300 Z900 I150 K0", [(6, 2, 1)])
# a joint move goes straight in joint space: its end alone can be past
case("joint move", "G0 X0 Y0 Z0 B0 C0\nG53.7 G0 J1=450\nG53.7 G0 J1=0", [(4, 1, 1)])
# on the identity type the joints are the axes
case("identity", "G0 X0 Y0 Z0 B0 C0\nG13.1\nG0 Y450\nG0 Y0", [(5, 1, 1)])
# a machine whose kinematics is the identity, a gantry: each joint is its
# axis, nothing loaded; the second Y joint has the shorter travel, which the
# extents of Y do not show.  A line is checked at its end, an arc also where
# the circle turns back, here its top, one way round and not the other.
os.environ["INI_FILE_NAME"] = "gantry.ini"
def gantry(name, text, want):
    flat = Canon()
    flat.kinematics_type = linuxcnc.KINEMATICS_IDENTITY
    return case(name, text, want, flat)
gantry("gantry line", "G0 X0 Y0 Z0\nG0 Y390\nG0 Y0", [(4, 3, 1)])
gantry("gantry arc", "G0 X-100 Y300 Z0\nG2 X100 Y300 I100 J0\nG0 X-100 Y300\n"
       "G3 X100 Y300 I100 J0", [(4, 3, 1)])
gantry("gantry inside", "G0 X0 Y0 Z0\nG0 Y370\nG2 X0 Y370 I0 J-5\nG0 Y0", [])
# three joints and no coordinates=: trivkins' nine letters cover them
os.environ["INI_FILE_NAME"] = "plain.ini"
gantry("three joints", "G0 X0 Y0 Z0\nG0 Y450\nG0 Y0", [(4, 1, 1)])

os.unlink("case.ngc")
print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
