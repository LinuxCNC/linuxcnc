#!/usr/bin/env python3

# Ported to the gomc REST/WS API (`gmi` client). motion-logger is now an
# interceptor between milltask and the real motmod.
#
# Github issue #49: the config runs startup gcode (RS274NGC_STARTUP_CODE =
# o<init> call). init.ngc rapids to (-1,-2,-3), then G4 P99 dwells 99 s (the
# abort lands here), then rapids to (1,2,3). We wait until the machine has
# reached the first target (so we are provably inside the dwell), then abort;
# this makes the captured command stream deterministic under real motion. The
# second rapid never runs.

import gmi
from gmi.constants import *

import sys
import time
import subprocess

c = gmi.Command()
s = gmi.Stat()
e = gmi.ErrorChannel()

# Wait until the startup gcode's first rapid to (-1,-2,-3) has completed, i.e.
# we are in the 99 s dwell. Abort then, so the abort deterministically lands in
# the dwell (after the first move, before the second).
# NOTE: currently xfail — gomc parses RS274NGC_STARTUP_CODE but never executes it
# (config.go:174 stores it in an unused field; module.go:305 reads and discards
# it), so the startup gcode never runs and no motion happens. This wait then times
# out. When gomc executes the startup code, remove the xfail; the ~1 s rapid makes
# a 15 s timeout ample.
start = time.time()
while time.time() - start < 15.0:
    s.poll()
    p = s.actual_position
    if abs(p[0] - (-1)) < 1e-3 and abs(p[1] - (-2)) < 1e-3 and abs(p[2] - (-3)) < 1e-3:
        break
    time.sleep(0.05)
else:
    raise SystemExit("timeout waiting for startup gcode first move")

print("UI abort")
sys.stdout.flush()
c.abort()
c.wait_complete()
time.sleep(0.3)
print("UI done with abort")
sys.stdout.flush()

status = subprocess.call(
    ["diff", "-u", "expected.motion-logger", "out.motion-logger"])
sys.exit(0 if status == 0 else 1)
