#!/usr/bin/env python

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
# Come out of E-stop, turn the machine on, home, and switch to Auto mode.
#

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_AUTO)


#
# run the .ngc test file, starting from the special line
#

c.program_open('test.ngc')
c.auto(linuxcnc.AUTO_RUN, 4)
c.wait_complete()

sys.exit(0)
