#!/usr/bin/env python3
# Drives T, M6, M61, G43 and G10 L1 and reports, after each step, what the
# interpreter's Python attributes and named parameters say about the
# current and selected pocket, which tool is in the spindle, and which tool
# length offset is applied. The tool table puts tools in pockets that differ
# from their position in the table, so a pocket number and a table index
# can not be mistaken for each other.

import sys
import time

import hal
import linuxcnc

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


def wait_for_startup(timeout=10.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        s.poll()
        if s.task_state == linuxcnc.STATE_ESTOP and s.cycle_time > 0 \
                and s.interp_state == linuxcnc.INTERP_IDLE:
            return
        time.sleep(0.1)
    raise RuntimeError("linuxcnc did not start")


def drain(until=None, timeout=5.0):
    """Print what arrives on the error channel, until a message starting with
    `until` (or, without one, until the channel is quiet)."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        msg = e.poll()
        if not msg:
            if until is None:
                return
            time.sleep(0.05)
            continue
        kind, text = msg
        if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            print("    ERROR: %s" % text)
        else:
            print("    %s" % text)
        if until is not None and text.startswith(until):
            return
    if until is not None:
        print("    (timed out waiting for %r)" % until)


def mdi(cmd):
    c.mdi(cmd)
    c.wait_complete(10)


def step(cmd, show_offset=False):
    print(cmd)
    if cmd:
        mdi(cmd)
    drain()
    mdi("o<report> call")
    drain(until="py:")
    s.poll()
    print("    tool_in_spindle=%d tool-prep-pocket=%d"
          % (s.tool_in_spindle, hal.get_value("iocontrol.0.tool-prep-pocket")))
    if show_offset:
        print("    tool_offset.z=%.4f" % s.tool_offset[2])
    sys.stdout.flush()


wait_for_startup()
print("=== begin")
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()
drain()

step("")
step("T1")
step("M6")
step("G43", show_offset=True)
step("T10")
step("M6")
step("G43", show_offset=True)
step("G10 L1 P10 Z7")
step("G43", show_offset=True)
step("M61 Q2")
step("G43", show_offset=True)
# T10 is the third tool in the table, so a tool number and an index differ.
step("M61 Q10")
step("G43", show_offset=True)
step("T99999 M6")
step("G43", show_offset=True)

# Unloading: T0 is no tool on a non-random changer only.
inifile = linuxcnc.ini(sys.argv[sys.argv.index("-ini") + 1])
if inifile.find("EMCIO", "RANDOM_TOOLCHANGER") != "1":
    step("M61 Q0")
    step("G43", show_offset=True)
    step("T1 M6")
    step("T0")
    step("M6")
    step("G43", show_offset=True)

print("=== end")
sys.exit(0)
