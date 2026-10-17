#!/usr/bin/env python3

import linuxcnc
import hal

import time
import sys


#
# connect to LinuxCNC
#

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


#
# Task queues the canon commands of interpreter init before it reads its
# first command. Let it issue them, or an early ESTOP_RESET wipes them
# and the log depends on how fast this script starts.
#

deadline = time.monotonic() + 60
while True:
    s.poll()
    if s.state == linuxcnc.RCS_DONE: break
    if time.monotonic() > deadline:
        print("task did not finish startup")
        sys.exit(1)
    time.sleep(0.01)


#
# Come out of E-stop, turn the machine on, home, and switch to Auto mode.
#

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_AUTO)


#
# run the .ngc test file, starting from the special line
#

c.program_open('mountaindew.ngc')
c.auto(linuxcnc.AUTO_RUN, 4)
c.wait_complete()

sys.exit(0)
