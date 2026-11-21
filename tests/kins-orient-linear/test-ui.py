#!/usr/bin/env python3
# 5axiskins orients with B and C; [AXIS_B] TYPE = LINEAR: see README.

import linuxcnc
import os
import sys

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()

said = []
for cmd in ("G12.1 P0", "G43.5 H1", "G0 X0 K1"):
    c.mdi(cmd)
    c.wait_complete(30)
    while True:
        m = e.poll()
        if not m:
            break
        said.append(m)

errors = [m[1] for m in said if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR)]
print("errors: %s" % errors)
ok = any("orients the tool with B, which [AXIS_B] TYPE makes LINEAR" in m for m in errors)
c.state(linuxcnc.STATE_ESTOP)
for f in ("sim.var", "sim.var.bak"):
    try:
        os.unlink(f)
    except OSError:
        pass
print("PASS" if ok else "FAIL: no refusal naming B")
sys.exit(0 if ok else 1)
