#!/usr/bin/env python3
# Ported to gomc: gmi client instead of the removed NML linuxcnc module.
import gmi
from gmi.constants import *
import math, time, sys

c = gmi.Command()
s = gmi.Stat()
e = gmi.ErrorChannel()

# Wait for the interpreter/task to settle (gmi.Stat lacks some classic startup
# fields like max_acceleration/program_units/cycle_time, so poll the essentials).
start = time.time()
while time.time() - start < 10.0:
    s.poll()
    if s.interp_state == INTERP_IDLE and s.exec_state == EXEC_DONE:
        break
    time.sleep(0.1)

c.state(STATE_ESTOP_RESET)
c.state(STATE_ON)
c.home(0); c.home(1); c.home(2)
c.wait_complete()
time.sleep(0.4)
c.mode(MODE_MDI)
c.mdi('g0 x0.1 y0.2 z0.3')
c.mdi('g41.1 d0.1')
c.mdi('g40')
c.mdi('g0 x0.9  ; surprise motion on Y and Z, to 0!!')
c.wait_complete()
time.sleep(0.4)
s.poll()
print("position: {}".format(s.position))
assert(math.fabs(s.position[0] - 0.9) < 0.0000001)
assert(math.fabs(s.position[1] - 0.2) < 0.0000001)
assert(math.fabs(s.position[2] - 0.3) < 0.0000001)
sys.exit(0)
