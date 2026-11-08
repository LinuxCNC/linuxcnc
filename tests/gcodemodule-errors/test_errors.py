#!/usr/bin/env python3
import os
import tempfile

import gcode


class BoomError(Exception):
    pass


class Canon:
    """A canon that answers every callback, except the one under test."""
    parameter_file = ""

    def __init__(self, failing):
        self.failing = failing

    def get_external_length_units(self):
        if self.failing == "get_external_length_units":
            raise BoomError("length units")
        return 1.0

    def get_external_angular_units(self):
        return 1.0

    def get_axis_mask(self):
        return 7

    def get_block_delete(self):
        return False

    def get_tool(self, idx):
        return (-1,) + (0.0,) * 12 + (0,)

    def straight_feed(self, *args):
        if self.failing == "straight_feed":
            raise BoomError("straight feed")

    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)
        return lambda *args, **kw: None


with tempfile.TemporaryDirectory() as d:
    prog = os.path.join(d, "t.ngc")
    with open(prog, "w") as f:
        f.write("G0 X1\nG1 X2 F100\nM2\n")

    for failing in ("get_external_length_units", "straight_feed"):
        try:
            gcode.parse(prog, Canon(failing))
        except BoomError as e:
            print(failing, "->", type(e).__name__, e)
        except Exception as e:
            raise SystemExit("%s: expected BoomError, got %s: %s"
                             % (failing, type(e).__name__, e))
        else:
            raise SystemExit("%s: parse did not raise" % failing)
