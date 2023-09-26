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


def wait_complete_step():

    '''The normal `linuxcnc.command.wait_complete()` function does not
    understand single-stepping.  It waits for the `state` to return to
    `RCS_DONE` but this does not happen when single-stepping, `state`
    stays at `RCS_EXEC` while waiting for the next step.

    This function instead waits for status.task.execState to tell us
    that Task is no longer waiting for Motion in any way.'''

    timeout = 5.0
    start = time.time()

    # Wait for the command to be acknowledged by Task (FIXME or is it motion?).
    while time.time() - start < timeout:
        s.poll()
        if s.echo_serial_number >= c.serial:
            break
        time.sleep(0.1)

    # Wait for Task to be done waiting for Motion.
    while time.time() - start < timeout:
        s.poll()
        if s.exec_state not in [ linuxcnc.EXEC_WAITING_FOR_MOTION, linuxcnc.EXEC_WAITING_FOR_MOTION_AND_IO, linuxcnc.EXEC_WAITING_FOR_MOTION_QUEUE ]:
            return
        time.sleep(0.1)

    raise SystemExit('timeout in wait_complete_step()')


# Take first three steps (these cause no motion).
c.auto(linuxcnc.AUTO_STEP)
c.auto(linuxcnc.AUTO_STEP)
c.auto(linuxcnc.AUTO_STEP)
wait_complete_step()

count = 0
while True:
    s.poll()
    if s.interp_state == linuxcnc.INTERP_IDLE:
        sys.stderr.write("Finished:  Detected program finish\n")
        break

    (x, y) = (h["Xpos"], h["Ypos"])
    if mod_5_is_0(x) and mod_5_is_0(y):
        # Both axes on goal; command the next step and wait for it to finish.
        sys.stderr.write("Taking step from X%.2f Y%.2f\n" % (x, y))
        c.auto(linuxcnc.AUTO_STEP)
        wait_complete_step()

    print_state()

    count += 1
    if count >= 1000:  # Shouldn't happen, but prevent runaways
        sys.stderr.write("Finished:  Exceeded max cycles\n")
        sys.exit(1)

    time.sleep(0.1)

if h["counter"] != 25:
    sys.stderr.write("End counter incorrect:  %d != 25\n" % h["counter"])
    sys.exit(1)

sys.exit(0)
