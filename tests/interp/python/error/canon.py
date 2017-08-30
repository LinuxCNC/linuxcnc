#!/usr/bin/python
import tempfile
import gcode
import sys

def float_fmt(f):
    if isinstance(f, float): return "% 5.1g" % f
    return "%5s" % f

class Canon:
    def __getattr__(self, attr):
        """Assume that any unknown attribute is a canon call; just print
        its args and return None"""

        def inner(*args):
            args = map(float_fmt, args)
            print "%-17s %s" % (attr, " ".join(args))
        return inner

    # this is just noisy
    def next_line(self, linecode): pass

    # These can't just return None...
    def get_external_length_units(self): return 1.0
    def get_external_angular_units(self): return 1.0
    def get_axis_mask(self): return 7 # (x y z)
    def get_block_delete(self): return False
    def get_tool(self, pocket):
        return -1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0

parameter = tempfile.NamedTemporaryFile()
canon = Canon()
canon.parameter_file = parameter.name
result, seq = gcode.parse(sys.argv[1], canon, '', '', '')
if result > gcode.MIN_ERROR: raise SystemExit, gcode.strerror(result)
