#!/usr/bin/env python

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

retval = 0


def wait_for_linuxcnc_startup(status, timeout=10.0):

    """Poll the Status buffer waiting for it to look initialized,
    rather than just allocated (all-zero).  Returns on success, throws
    RuntimeError on failure."""

    start_time = time.time()
    while time.time() - start_time < timeout:
        status.poll()
        if (status.angular_units == 0.0) \
            or (status.axes == 0) \
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
            or (status.state != linuxcnc.STATE_ESTOP) \
            or (status.task_state != linuxcnc.STATE_ESTOP):
            time.sleep(0.1)
        else:
            # looks good
            return

    # timeout, throw an exception
    raise RuntimeError


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

wait_for_linuxcnc_startup(s)

s.poll()

if s.g5x_index != 1:
    print "Expected g5x_index=1 (startup in G54), got %d instead" % s.g5x_index
    retval = 1

if math.fabs(s.tool_offset[2] - 0.1234) > 0.000001:
    print "Expected tool offset of 0.1234 via startup gcode not detected"
    print "Got %f instead." % s.tool_offset[2]
    retval = 1

sys.exit(retval)

