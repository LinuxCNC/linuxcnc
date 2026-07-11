#!/usr/bin/env python3

# Ported to the gomc gmi REST/WS client (from the removed NML linuxcnc module).

import gmi
from gmi.constants import *

import math
import time
import sys

c = gmi.Command()
s = gmi.Stat()
e = gmi.ErrorChannel()

# Wait for startup + RS274NGC_STARTUP_CODE (G43 H1) to be applied.
t = time.time()
while time.time() - t < 10:
    s.poll()
    if s.interp_state == INTERP_IDLE and s.task_state != 0:
        break
    time.sleep(0.1)
time.sleep(0.5)

s.poll()
retval = 0

if s.g5x_index != 1:
    print("Expected g5x_index=1 (startup in G54), got %d instead" % s.g5x_index)
    retval = 1

if math.fabs(s.tool_offset[2] - 0.1234) > 0.000001:
    print("Expected tool offset of 0.1234 via startup gcode not detected")
    print("Got %f instead." % s.tool_offset[2])
    retval = 1

sys.exit(retval)
