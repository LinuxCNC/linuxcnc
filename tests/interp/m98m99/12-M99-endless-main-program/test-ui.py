#!/usr/bin/env python3

# Ported to the gomc REST/WS API (`gmi` client). motion-logger is now an
# interceptor between milltask and the real motmod. Runs test.ngc from line 4;
# the M99 in the main program loops it (terminated by a counter at 3 loops). A
# large feed override lets real motion complete quickly without changing the
# logged (programmed) SET_LINE velocities.
#
# The motion-logger diff is written to STDERR (and reflected in the exit code) so
# it does NOT pollute stdout, which test.sh reserves for the standalone rs274
# output compared against `expected`.

import gmi
from gmi.constants import *

import sys
import time
import subprocess

c = gmi.Command()
s = gmi.Stat()
e = gmi.ErrorChannel()


def wait_idle(timeout=60.0):
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

c.program_open('test.ngc')
c.auto(AUTO_RUN, 4)
wait_idle()
time.sleep(0.2)

status = subprocess.call(
    ["diff", "-u", "expected.motion-logger", "out.motion-logger"],
    stdout=sys.stderr)
sys.exit(0 if status == 0 else 1)
