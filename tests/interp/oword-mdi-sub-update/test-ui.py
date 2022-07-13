#!/usr/bin/env python3

import linuxcnc
import hal

import time
import sys
import os

from shutil import copyfile

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

#
# set up pins
# shell out to halcmd to net our pins to where they need to go
#

h = hal.component("python-ui")
h.ready()


#
# connect to LinuxCNC
#

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


# Wait for LinuxCNC to initialize itself so the Status buffer stabilizes.
wait_for_linuxcnc_startup(s)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()


# Get into MDI mode
c.mode(linuxcnc.MODE_MDI)

# Copy the original sub into place and call it
copyfile('subs/test1-orig.ngc', 'subs/test1.ngc')
c.mdi('o<test1> call [20]')
c.wait_complete()

# Pretend the file was edited:  copy new sub with spacing changes and
# call it
os.system('./edit.sh')
c.mdi('o<test1> call [20]')
c.wait_complete()

# Run again
c.mdi('o<test1> call [20]')
c.wait_complete()

sys.exit(0)

