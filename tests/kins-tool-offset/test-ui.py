#!/usr/bin/env python3
# The kinematics module takes the tool offset from motion.
#
# xyzac-trt-kins runs with nothing connected to its tool-offset pin.  A
# tool length applied with G43 must still reach the joints, since motion
# hands the offset to the module; connecting motion.tooloffset.z to the
# pin afterwards, the old way, must change nothing; and G49 must take the
# length back out again through motion alone.

import linuxcnc
import hal
import subprocess
import sys
import os
import time

TOOL_LENGTH = 25.0
POSE = "G0 X10 Y20 Z30 A30 C45"
AWAY = "G0 X0 Y0 Z0 A0 C0"

c = linuxcnc.command()
s = linuxcnc.stat()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()
c.mode(linuxcnc.MODE_MDI)

errors = 0

def error(msg):
    global errors
    errors += 1
    print("*** ERROR " + msg)

def mdi(*cmds):
    for cmd in cmds:
        c.mdi(cmd)
        c.wait_complete(30)

def joints():
    # the commanded joint positions once the move has settled: in position,
    # nothing queued, and the same answer twice in a row, since the in
    # position flag can go up a cycle before the last increment lands
    deadline = time.time() + 30
    last = None
    while time.time() < deadline:
        s.poll()
        now = [s.joint_position[i] for i in range(5)]
        if s.inpos and not s.queue and now == last:
            return now
        last = now
        time.sleep(0.1)
    error("timed out waiting for the move")
    return last

def same(a, b, tol=1e-6):
    return all(abs(x - y) <= tol for x, y in zip(a, b))

def show(what, j):
    print("%-28s %s" % (what, " ".join("%.6f" % v for v in j)))

# no tool: the pose with nothing applied
mdi("G49", POSE)
base = joints()
show("G49", base)

# tool applied through motion, the pin still at its default
mdi("G43 H1", AWAY, POSE)
with_tool = joints()
show("G43 H1, pin unconnected", with_tool)
pin = hal.get_value("xyzac-trt-kins.tool-offset")
if pin != 0.0:
    error("the tool-offset pin reads %g with nothing connected" % pin)
if same(base, with_tool):
    error("the tool length did not reach the joints")

# the table on rotaries at A30 C45: the tool length moves Y and Z joints,
# by a known amount, since the pivot geometry is the module's alone
tool_z = hal.get_value("motion.tooloffset.z")
if abs(tool_z - TOOL_LENGTH) > 1e-9:
    error("motion.tooloffset.z is %g, expected %g" % (tool_z, TOOL_LENGTH))
if abs(with_tool[0] - base[0]) > 1e-6:
    error("the tool length moved joint 0, which the A rotation does not touch")

# the old connection: nothing may change
subprocess.check_call(["halcmd", "net", ":tool-offset",
                       "motion.tooloffset.z", "xyzac-trt-kins.tool-offset"])
mdi(AWAY, POSE)
with_net = joints()
show("G43 H1, pin connected", with_net)
pin = hal.get_value("xyzac-trt-kins.tool-offset")
if abs(pin - TOOL_LENGTH) > 1e-9:
    error("the connected tool-offset pin reads %g" % pin)
if not same(with_tool, with_net):
    error("connecting the pin changed the joints")

# and back out, through motion, with the pin connected
mdi("G49", AWAY, POSE)
without = joints()
show("G49, pin connected", without)
if not same(base, without):
    error("G49 did not take the tool length back out")

for f in ("sim.var", "sim.var.bak"):
    try:
        os.unlink(f)
    except OSError:
        pass

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
