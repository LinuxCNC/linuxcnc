#!/usr/bin/env python3
#
# Checks that stat.call_stack describes the move motion is EXECUTING, not the
# position the interpreter has read ahead to.
#
# The interpreter reads the whole of this short program long before the moves
# finish, so while the machine is still cutting inside o<inner> the interpreter
# has already returned to the main program and read to EOF.  A call stack taken
# from the live interpreter would be empty (or hold stale frames) at that point;
# one taken from the executing move's StateTag still names outer/inner.

import linuxcnc
import linuxcnc_util

import time
import sys
import os

INTERPTIMEOUT = 5
GCODETIMEOUT = 120

failures = []


def fail(msg):
    print("FAIL: {}".format(msg))
    failures.append(msg)


ngcfile = None
for i in range(1, len(sys.argv) - 1):
    if "-ngc" == sys.argv[i]:
        ngcfile = sys.argv[i + 1]
        break

if not ngcfile:
    print("Missing NGC-file; run with: test-ui.py -ngc ngcfile.ngc")
    sys.exit(1)
if not os.path.exists(ngcfile):
    print("NGC-file '{}' does not exist".format(ngcfile))
    sys.exit(1)

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()
l.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])

c.mode(linuxcnc.MODE_AUTO)
c.program_open(ngcfile)
c.auto(linuxcnc.AUTO_RUN, 1)

start = time.time()
while time.time() - start < INTERPTIMEOUT:
    s.poll()
    if s.interp_state != linuxcnc.INTERP_IDLE:
        break
    time.sleep(0.01)
if s.interp_state == linuxcnc.INTERP_IDLE:
    print("Timed out starting interpreter")
    sys.exit(1)

# Collect (call_level, call_stack, read_line, motion_line) while the program runs
saw_depth2 = None
saw_lagging_stack = False
last_shown = None
eof_line = 0
with open(ngcfile) as f:
    eof_line = len(f.readlines())

start = time.time()
s.poll()
while s.interp_state != linuxcnc.INTERP_IDLE and time.time() - start < GCODETIMEOUT:
    s.poll()
    stack = s.call_stack
    level = s.call_level

    # Invariant: depth and frames must come from the same point in the program.
    if len(stack) != level:
        fail("len(call_stack)={} != call_level={}".format(len(stack), level))

    # print only when the reported stack changes, so the log stays readable
    shown = (level, tuple((fr["subname"], os.path.basename(fr["filename"]),
                           fr["line"]) for fr in stack))
    if shown != last_shown:
        print("read_line={:3d} motion_line={:3d} level={} stack={}".format(
            s.read_line, s.motion_line, level, list(shown[1])))
        last_shown = shown

    if level == 2 and saw_depth2 is None:
        saw_depth2 = [dict(fr) for fr in stack]

    # The interpreter has read to EOF but motion is still inside a subroutine:
    # this is exactly the window where a live-interpreter stack would be wrong.
    if level > 0 and s.read_line >= eof_line:
        saw_lagging_stack = True

    time.sleep(0.01)

if s.interp_state != linuxcnc.INTERP_IDLE:
    print("Timed out running the GCode program")
    sys.exit(1)

# --- checks -----------------------------------------------------------------

if saw_depth2 is None:
    fail("never observed a 2-deep call stack while executing")
else:
    outer, inner = saw_depth2[0], saw_depth2[1]
    print("observed depth-2 stack: {}".format(saw_depth2))
    if outer["subname"] != "outer":
        fail("frame 0 subname is '{}', expected 'outer'".format(outer["subname"]))
    if inner["subname"] != "inner":
        fail("frame 1 subname is '{}', expected 'inner'".format(inner["subname"]))
    if os.path.basename(outer["filename"]) != "test.ngc":
        fail("frame 0 filename is '{}', expected test.ngc".format(outer["filename"]))
    if os.path.basename(inner["filename"]) != "outer.ngc":
        fail("frame 1 filename is '{}', expected outer.ngc".format(inner["filename"]))

if not saw_lagging_stack:
    fail("call stack never lagged the interpreter; it is not reporting the "
         "executing move")

s.poll()
if s.call_level != 0:
    fail("call_level is {} after program end, expected 0".format(s.call_level))
if len(s.call_stack) != 0:
    fail("call_stack is {} after program end, expected empty".format(s.call_stack))

if failures:
    print("{} check(s) failed".format(len(failures)))
    sys.exit(1)

print("call-stack checks passed")
sys.exit(0)
