#!/usr/bin/env python3
# The interpreter reads ahead of the machine. A program is aborted after the
# interpreter has read a T word that the tool changer never got to prepare:
# the next M6 loads the tool that is prepared, and the interpreter has to
# agree about which tool that is - for #<_selected_tool>, and for the G43
# on the M6 line.

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


def mdi(cmd):
    print(cmd)
    c.mdi(cmd)
    c.wait_complete(10)
    drain()


def report():
    c.mdi("(debug,selected_tool=#<_selected_tool> selected_pocket=#<_selected_pocket>)")
    c.wait_complete(10)
    drain()
    s.poll()
    print("    tool_in_spindle=%d tool_prepped=%d tool_offset.z=%.4f"
          % (s.tool_in_spindle, s.tool_prepped, s.tool_offset[2]))
    sys.stdout.flush()


wait_for(lambda: s.task_state == linuxcnc.STATE_ESTOP and s.cycle_time > 0
         and s.interp_state == linuxcnc.INTERP_IDLE, "startup")
print("=== begin")
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()
drain()

mdi("T1")
report()

# The dwell holds the T10 after it back for 10 s. The interpreter reads it
# long before that, and the abort comes while the machine still dwells.
with open("abort.ngc", "w") as f:
    f.write("G4 P10\nT10\nG4 P1\nG4 P1\nM2\n")
c.mode(linuxcnc.MODE_AUTO)
c.wait_complete()
c.program_open("abort.ngc")
c.wait_complete()
c.auto(linuxcnc.AUTO_RUN, 0)
wait_for(lambda: s.read_line >= 3, "the interpreter to read past T10")
print("abort with T10 read, read ahead of the machine: %s"
      % (s.tool_prepped == 1))
c.abort()
c.wait_complete()
wait_for(lambda: s.interp_state == linuxcnc.INTERP_IDLE, "the abort")
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()
drain()
report()

mdi("M6 G43")
report()

print("=== end")
sys.exit(0)
