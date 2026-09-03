#!/usr/bin/env python3
import gcode
import linuxcnc
import linuxcnc_util
from rs274.interpret import PrintCanon, StatMixin
import preview_helpers
import sys
import os


class PreviewCanon(PrintCanon, StatMixin):

    def __init__(self, stat, randomtc, parameter):
        StatMixin.__init__(self, stat, randomtc)
        self.parameter_file = parameter
        self.feed_rate = 0.0
        self.current_x = 0.0
        self.current_y = 0.0
        self.current_z = 0.0

    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        print(f"[WARN] unimplemented canon: {name}()", file=sys.stderr)
        return lambda *args, **kwargs: None

    def set_xy_rotation(self, *args):
        print("set_xy_rotation", args)

    def set_g5x_offset(self, *args):
        print("set_g5x_offset", args)

    def set_g92_offset(self, *args):
        print("set_g92_offset", args)

    def tool_offset(self, *args):
        print("tool_offset", args)

    def next_line(self, state):
        #print("next_line", state.sequence_number)
        self.state = state

    def set_plane(self, plane):
        print("set plane", plane)

    def set_feed_rate(self, arg):
        print("set feed rate", arg)

    def comment(self, arg):
        print("#", arg)

    def straight_traverse(self, *args):
        print("straight_traverse " , args)

    def straight_feed(self, *args):
        print("straight_feed" , args)

    def dwell(self, arg):
        if arg < .1:
            print("dwell %f ms" % (1000 * arg))
        else:
            print("dwell %f seconds" % arg)

    def arc_feed(self, *args):
        print("arc_feed " , args)

    def close(self, *args):
        pass


# route output to log file now
log_file = open("temp_log","w")
sys.stdout = log_file

# connect
c = linuxcnc.command()
e = linuxcnc.error_channel()
s = linuxcnc.stat()
l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)

# parse inifile variables
inifile = linuxcnc.ini("test.ini")
random = inifile.getbool("EMCIO", "RANDOM_TOOLCHANGER", fallback=False)
parameter = inifile.getstring("RS274NGC", "PARAMETER_FILE", fallback="test.var")

# setup preview interpreter
filename = "test.ngc"
canon = PreviewCanon(s, random, parameter)

# setup linuxcnc
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MANUAL)   
c.home(-1)   
l.wait_for_home(joints=[1,1,1,1,0,0,0,0,0])

# check for startup codes (must match ini [RS274NGC] RS274NGC_STARTUP_CODE )
s.poll()
print("startup gcodes ", s.gcodes)

# change some states
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()
# move tool
c.mdi("G0 x 1.23")
c.wait_complete()
# Set G55 with offset
c.mdi("G55")
c.wait_complete()
c.mdi("G10 L2 P0 Z1")
c.wait_complete()

# parse gcode test file
try:
    initcodes = preview_helpers.create_unitcode_and_initcode(s, inifile)
    result, seq = gcode.parse(filename, canon, *initcodes)
    if result > gcode.MIN_ERROR:
        print(f"G-code error at line {seq}: error code {result}",file=sys.stderr)
        sys.exit(1)

except Exception as e:
    print(e,file=sys.stderr)
    sys.exit(1)

finally:
    log_file.close()
    canon.close()
