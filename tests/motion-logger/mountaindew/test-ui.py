#!/usr/bin/env python3

# Ported to the gomc REST/WS API (`gmi` client). The motion-logger is now an
# interceptor cmod between milltask and the real motmod. The program is run from
# a specific start line; a large feed override lets real motion complete quickly
# without changing the logged (programmed) SET_LINE velocities. We wait for
# INTERP_IDLE (true completion) so the full command stream is captured, then diff
# out.motion-logger against expected.motion-logger.

import gmi
from gmi.constants import *

import sys
import time
import subprocess

c = gmi.Command()
s = gmi.Stat()
e = gmi.ErrorChannel()


def wait_idle(timeout=30.0):
    start = time.time()
    while time.time() - start < 5.0:
        s.poll()
        if s.interp_state != INTERP_IDLE:
            break
        time.sleep(0.02)
    while time.time() - start < timeout:
        s.poll()
        if s.interp_state == INTERP_IDLE:
            return
        time.sleep(0.02)
    raise SystemExit("timeout waiting for interp idle")


c.state(STATE_ESTOP_RESET)
c.state(STATE_ON)
c.wait_complete()
c.mode(MODE_AUTO)
c.feedrate(10000.0)
c.wait_complete()

c.program_open('mountaindew.ngc')
c.auto(AUTO_RUN, 4)
wait_idle()
time.sleep(0.2)

status = subprocess.call(
    ["diff", "-u", "expected.motion-logger", "out.motion-logger"])
sys.exit(0 if status == 0 else 1)
