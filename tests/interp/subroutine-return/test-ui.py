#!/usr/bin/env python3
# Ported to the gomc gmi REST/WS client (removed NML linuxcnc module).
import gmi
from gmi.constants import *
import time, sys, os

c = gmi.Command(); s = gmi.Stat(); e = gmi.ErrorChannel()

c.state(STATE_ESTOP_RESET); c.state(STATE_ON)
c.home(0); c.home(1); c.home(2)
t = time.time()
while time.time() - t < 5.0:
    s.poll()
    if all(s.homed[0:3]): break
    time.sleep(0.1)

c.mode(MODE_AUTO)
c.program_open("test.ngc")
c.auto(AUTO_RUN, 1)

# wait for the interpreter to start running test.ngc
t = time.time()
while (time.time() - t) < 3.0:
    s.poll()
    if s.interp_state != INTERP_IDLE: break
    time.sleep(0.01)
if s.interp_state == INTERP_IDLE:
    print("failed to start interpreter"); sys.exit(1)

# rename the file mid-run: the interpreter should hold the program, not re-read it
os.rename('test.ngc', 'moved-test.ngc')

# wait for the program to finish
t = time.time()
while (time.time() - t) < 20.0:
    s.poll()
    if s.interp_state == INTERP_IDLE: break
    time.sleep(0.1)
else:
    print("program did not finish"); sys.exit(1)

os.rename('moved-test.ngc', 'test.ngc')
print("done! it all worked")
sys.exit(0)
