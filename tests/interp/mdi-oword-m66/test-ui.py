#!/usr/bin/env python3
# Ported to the gomc gmi REST/WS client (removed NML linuxcnc module).
import gmi
from gmi.constants import *
import time, sys

c = gmi.Command(); s = gmi.Stat(); e = gmi.ErrorChannel()

def settle():
    time.sleep(0.7)
    # gmi.Stat updates over WS and lags c.wait_complete(); wait for idle.
    for _ in range(50):
        s.poll()
        if s.interp_state == INTERP_IDLE:
            return
        time.sleep(0.1)

c.state(STATE_ESTOP_RESET); c.state(STATE_ON)
c.home(0); c.home(1); c.home(2)
start = time.time()
while time.time() - start < 5.0:
    s.poll()
    if all(s.homed[0:3]): break
    time.sleep(0.1)
else:
    print("timeout waiting for home"); sys.exit(1)

c.mode(MODE_MDI)

c.mdi("O<obug> call [0]"); c.wait_complete(); settle()
if s.position[0] != 0:
    print("ended at wrong location (did O-call terminate with error?)"); sys.exit(1)

c.mdi("O<obug> call [1]"); c.wait_complete(); settle()
if abs(s.position[0] - 1) > 1e-5:
    print("ended at wrong location (did O-call terminate with error?)", s.position); sys.exit(1)
print("done! it all worked")
sys.exit(0)
