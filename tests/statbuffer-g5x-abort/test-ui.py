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


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)

c.mode(linuxcnc.MODE_AUTO)
c.program_open('test.ngc')

c.mode(linuxcnc.MODE_MDI)
c.mdi('G10 L2 P1 X0 Y0 Z-4 (Set G54)')
c.mdi('G10 L2 P2 X0 Y0 Z-5 (Set G55)')
c.mdi('G10 L2 P3 X0 Y0 Z-6 (Set G56)')
c.mdi('G54 (Use G54)')
c.mdi('G0 X0 Y0 Z0 (Move to 0,0,0)')
c.wait_complete()

# FIXME: this is lame
time.sleep(1)

c.mode(linuxcnc.MODE_AUTO)
c.auto(linuxcnc.AUTO_RUN, 0)

# FIXME: this is lame
time.sleep(1)

c.abort()

c.wait_complete()
s.poll()

# The program was aborted in g55; check
if not 550 in s.gcodes:
    print "Current coordinate system is G%d, not G55" % \
        ([i/10 for i in s.gcodes if i >= 540 and i < 600] + [None])[0]
    retval = 1
else:
    print "Current coordinate system is G55"

# MDI G0 takes us to the programmed location in G54
c.mode(linuxcnc.MODE_MDI)
c.mdi('G0 X0 Y0 Z0 (Move to 0,0,0)')
c.wait_complete()
s.poll()

if math.fabs(s.position[0]) > 0.000001:
    print "'G0 X0 Y0 Z0' took us to X=%.6f, should be 0" % s.position[0]
    retval = 1

if math.fabs(s.position[1]) > 0.000001:
    print "'G0 X0 Y0 Z0' took us to Y=%.6f, should be 0" % s.position[1]
    retval = 1

if math.fabs(s.position[2] + 5) > 0.000001:
    print "'G0 X0 Y0 Z0' took us to Z=%.6f, should be -5" % s.position[2]
    retval = 1

c.mdi('(debug,G53+#5220 ?= G55; Z#5422 ?= Z0.0)')
c.wait_complete()
err = e.poll()
print err

s.poll()
if s.g5x_index != 2:
    print "status has wrong g5x index: %d (expected 2)" % s.g5x_index
    retval = 1
else:
    print "status has correct g5x index:  2"

if math.fabs(s.g5x_offset[0]) > 0.000001:
    print "g5x_offset.x is %.6f, should be 0" % s.g5x_offset[0]
    retval = 1

if math.fabs(s.g5x_offset[1]) > 0.000001:
    print "g5x_offset.y is %.6f, should be 0" % s.g5x_offset[1]
    retval = 1

if math.fabs(s.g5x_offset[2] + 5) > 0.000001:
    print "g5x_offset.z is %.6f, should be -5" % s.g5x_offset[2]
    retval = 1

if retval == 0:
    print "Everything ok!"

sys.exit(retval)

