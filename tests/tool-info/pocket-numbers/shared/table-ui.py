#!/usr/bin/env python3
# Task keeps the tools in the order the tool table file has them, and tells
# the operator about a line it skips. tool_offset() keeps the offsets it
# does not set. A G43 read right after a G10 L1 gets the new offset, though
# task has not stored it yet.

import sys
import time

import linuxcnc

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


def wait_for(condition, what, timeout=10.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        s.poll()
        if condition():
            return
        time.sleep(0.01)
    raise RuntimeError("timed out waiting for " + what)


def drain():
    while True:
        msg = e.poll()
        if not msg:
            return
        kind, text = msg
        if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            print("    ERROR: %s" % text)
        else:
            print("    %s" % text)
    sys.stdout.flush()


def table():
    s.poll()
    print("    tools: %s" % [t.id for t in s.tool_table])
    with open("table.tbl") as f:
        for line in f:
            print("    file: %s" % line.rstrip())
    sys.stdout.flush()


wait_for(lambda: s.task_state == linuxcnc.STATE_ESTOP and s.cycle_time > 0
         and s.interp_state == linuxcnc.INTERP_IDLE, "startup")
print("=== begin")
drain()
table()
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()

print("tool_offset(1, z=7, x=0.25)")
c.tool_offset(1, 7.0, 0.25, 0.0, 0.0, 0.0, 0)
c.wait_complete()
drain()
table()

# The machine dwells while the interpreter reads the G10 and the G43.
with open("table.ngc", "w") as f:
    f.write("G4 P1\nG10 L1 P1 Z5\nG43 H1\nM2\n")
c.mode(linuxcnc.MODE_AUTO)
c.wait_complete()
c.program_open("table.ngc")
c.wait_complete()
print("G10 L1 P1 Z5, G43 H1")
c.auto(linuxcnc.AUTO_RUN, 0)
wait_for(lambda: s.interp_state != linuxcnc.INTERP_IDLE, "the program to start")
wait_for(lambda: s.interp_state == linuxcnc.INTERP_IDLE, "the program to end")
drain()
s.poll()
print("    tool_offset.z=%.4f" % s.tool_offset[2])
table()

print("=== end")
sys.exit(0)
