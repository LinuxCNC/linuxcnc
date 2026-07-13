#!/usr/bin/env python3
# Drive the arc.ngc program to completion via the gmi REST client.
import gmi
from gmi.constants import *
import sys, time

c = gmi.Command()
s = gmi.Stat()

c.state(STATE_ESTOP_RESET)
c.state(STATE_ON)
c.home(-1)
c.wait_complete()

c.mode(MODE_AUTO)
c.program_open('arc.ngc')
c.auto(AUTO_RUN, 0)

# Wait for the program to actually START (leave IDLE) before waiting for it to
# finish, else the pre-run IDLE state ends the wait immediately.
t = time.time()
while time.time() - t < 5:
    s.poll()
    if s.interp_state != INTERP_IDLE:
        break
    time.sleep(0.02)

# Now wait for it to run the full arc back to idle.
t = time.time()
while time.time() - t < 30:
    s.poll()
    if s.interp_state == INTERP_IDLE:
        break
    time.sleep(0.05)

c.wait_complete()
time.sleep(0.3)   # let the final servo cycles capture
sys.exit(0)
