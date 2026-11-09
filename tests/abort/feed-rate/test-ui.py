#!/usr/bin/env python3

import linuxcnc
import hal

import math
import time
import sys
import subprocess
import os
import signal
import glob
import re


def wait_for_linuxcnc_startup(status, timeout=10.0):

    """Poll the Status buffer waiting for it to look initialized,
    rather than just allocated (all-zero).  Returns on success, throws
    RuntimeError on failure."""

    start_time = time.time()
    while time.time() - start_time < timeout:
        status.poll()
        if (status.angular_units == 0.0) \
            or (status.axis_mask == 0) \
            or (status.cycle_time == 0.0) \
            or (status.exec_state != linuxcnc.EXEC_DONE) \
            or (status.interp_state != linuxcnc.INTERP_IDLE) \
            or (status.inpos == False) \
            or (status.linear_units == 0.0) \
            or (status.max_acceleration == 0.0) \
            or (status.max_velocity == 0.0) \
            or (status.program_units == 0.0) \
            or (status.rapidrate == 0.0) \
            or (status.state != linuxcnc.RCS_DONE) \
            or (status.task_state != linuxcnc.STATE_ESTOP):
            time.sleep(0.1)
        else:
            # looks good
            return

    # timeout, throw an exception
    raise RuntimeError("Timeout")


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

# Wait for LinuxCNC to initialize itself so the Status buffer stabilizes.
wait_for_linuxcnc_startup(s)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()

c.mode(linuxcnc.MODE_MDI)

feed_rate = 234.5
c.mdi('g94 ; normal "units per minute" feed mode')
c.mdi('f%.1f' % feed_rate)
c.wait_complete()
c.abort()
c.wait_complete()
s.poll()
print("feed rate:{}".format(s.settings[1]))
assert(math.fabs(s.settings[1] - feed_rate) < 0.0000001)

feed_rate = 345.6
c.mdi('g95 ; "units per revolution" feed mode')
c.mdi('f%.1f' % feed_rate)
c.wait_complete()
c.abort()
c.wait_complete()
s.poll()
print("feed rate:{}".format(s.settings[1]))
assert(math.fabs(s.settings[1] - feed_rate) < 0.0000001)


def wait_for(condition, timeout=10.0):
    start_time = time.time()
    while time.time() - start_time < timeout:
        s.poll()
        if condition():
            return True
        time.sleep(0.01)
    return False


# An E-stop reset sent to a machine that is on leaves it alone, and does
# not run the abort that goes with leaving E-stop.
c.mdi('g20 g90 g94 m3 s1000 m8')
c.wait_complete()
c.mdi('g1 x3 f30')
assert wait_for(lambda: s.current_vel > 0), "the move did not start"
c.state(linuxcnc.STATE_ESTOP_RESET)
c.wait_complete()
s.poll()
x = s.position[0]
assert wait_for(lambda: s.position[0] > x + 0.1), "the reset stopped the move"
assert s.task_state == linuxcnc.STATE_ON, "the reset turned the machine off"
assert s.spindle[0]['enabled'], "the reset stopped the spindle"
assert s.flood, "the reset turned the flood off"
c.abort()
c.wait_complete()

sys.exit(0)

