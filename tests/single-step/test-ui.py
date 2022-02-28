#!/usr/bin/env python3

import linuxcnc
import linuxcnc_util
import hal

import time
import sys
import os

#
# connect to LinuxCNC
#

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()
l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)

#
# Create and connect test feedback comp
#
h = hal.component("test-ui")
h.newpin("Xpos", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("Ypos", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("Zpos", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("counter", hal.HAL_FLOAT, hal.HAL_IN)
h.ready()
os.system("halcmd source ./postgui.hal")

def print_state():
    sys.stderr.write(
        "line=%d; Xpos=%.2f; Ypos=%.2f; Zpos=%.2f; counter=%d\n" %
        (s.current_line, h["Xpos"], h["Ypos"], h["Zpos"], h["counter"]))

#
# Come out of E-stop, turn the machine on, home, and switch to Auto mode.
#

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(0)
c.home(1)
c.home(2)
l.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])
c.mode(linuxcnc.MODE_AUTO)
        

#
# run the .ngc test file, starting from the special line
#

c.program_open('test.ngc')

epsilon = 0.000001
def mod_5_is_0(x):
    return abs((x+epsilon) % 5) < 2*epsilon

# Take first step
c.auto(linuxcnc.AUTO_STEP)

count = 0
while True:
    s.poll()
    (x, y) = (h["Xpos"], h["Ypos"])
    if mod_5_is_0(x) and mod_5_is_0(y):
        # Both axes on goal; make next step and let motion start
        sys.stderr.write("Taking step from X%.2f Y%.2f\n" % (x, y))
        c.auto(linuxcnc.AUTO_STEP)
        time.sleep(0.1)

    print_state()

    count += 1
    if count >= 1000:  # Shouldn't happen, but prevent runaways
        sys.stderr.write("Finished:  Exceeded max cycles\n")
        sys.exit(1)
    if s.interp_state == linuxcnc.INTERP_IDLE:
        sys.stderr.write("Finished:  Detected program finish\n")
        break

    time.sleep(0.1)

if h["counter"] != 25:
    sys.stderr.write("End counter incorrect:  %d != 25\n" % h["counter"])
    sys.exit(1)

sys.exit(0)
