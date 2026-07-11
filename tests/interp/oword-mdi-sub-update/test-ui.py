#!/usr/bin/env python3
# Ported to the gomc gmi REST/WS client (removed NML linuxcnc module).
import gmi
from gmi.constants import *
import time, sys, os
from shutil import copyfile

c = gmi.Command(); s = gmi.Stat(); e = gmi.ErrorChannel()

def wait_for_startup(timeout=10.0):
    t = time.time()
    while time.time() - t < timeout:
        s.poll()
        if (s.angular_units != 0.0 and s.linear_units != 0.0 and s.axis_mask != 0
                and s.exec_state == EXEC_DONE and s.interp_state == INTERP_IDLE
                and s.task_state == STATE_ESTOP):
            return
        time.sleep(0.1)
    raise RuntimeError("Timeout")

def drain_errors():
    # (DEBUG,) operator messages arrive on the error channel; print them so
    # checkresult can grep the result for 'test RAN'.
    for _ in range(50):
        r = e.poll()
        if not r:
            time.sleep(0.05); continue
        code, msg = r
        print(msg); sys.stdout.flush()

wait_for_startup()
c.state(STATE_ESTOP_RESET); c.state(STATE_ON); c.home(-1); c.wait_complete()
c.mode(MODE_MDI)

copyfile('subs/test1-orig.ngc', 'subs/test1.ngc')
c.mdi('o<test1> call [20]'); c.wait_complete(); time.sleep(0.3); drain_errors()

os.system('./edit.sh')
c.mdi('o<test1> call [20]'); c.wait_complete(); time.sleep(0.3); drain_errors()

c.mdi('o<test1> call [20]'); c.wait_complete(); time.sleep(0.3); drain_errors()
sys.exit(0)
