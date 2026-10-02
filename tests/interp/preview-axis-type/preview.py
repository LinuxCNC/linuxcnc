#!/usr/bin/env python3
# What the preview canon hands on, in inches, for a LINEAR A and an ANGULAR
# V: A converts like X, V stays in degrees.
import gcode
import sys
import tempfile

class Canon:
    def __getattr__(self, attr):
        def inner(*args):
            pass
        return inner

    def report(self, what, args):
        print("%-17s %s" % (what, " ".join("%.4f" % a for a in args)))

    def straight_feed(self, *args): self.report("straight_feed", args)
    def set_g5x_offset(self, index, *args): self.report("set_g5x_offset", args)
    def next_line(self, linecode): pass
    def get_external_length_units(self): return 1.0
    def get_external_angular_units(self): return 1.0
    def get_axis_mask(self): return 0x1ff
    def get_block_delete(self): return False
    def get_tool(self, pocket):
        return -1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0

parameter = tempfile.NamedTemporaryFile()
canon = Canon()
canon.parameter_file = parameter.name
result, seq = gcode.parse(sys.argv[1], canon, '', '', '')
if result > gcode.MIN_ERROR: raise SystemExit(gcode.strerror(result))
