#!/usr/bin/env python3

# Ported to the gomc REST/WS API: uses the `gmi` client package instead of the
# removed NML `linuxcnc` python module.  Positions come from gmi.Stat; the
# custom `motion.analog-out-00` counter is read with `halcmd getp` (there is no
# userspace HAL component anymore).

import gmi
from gmi.constants import *

import subprocess
import time
import sys

c = gmi.Command()
s = gmi.Stat()
e = gmi.ErrorChannel()

_WAITING = (
    EXEC_WAITING_FOR_MOTION,
    EXEC_WAITING_FOR_MOTION_AND_IO,
    EXEC_WAITING_FOR_MOTION_QUEUE,
)


def counter():
    # halcmd getp prints "<type> <dir> <name> = <value>"; take the last field.
    out = subprocess.check_output(["halcmd", "getp", "motion.analog-out-00"])
    return float(out.split()[-1])


def positions():
    p = s.actual_position
    return p[0], p[1], p[2]


def print_state():
    x, y, z = positions()
    sys.stderr.write(
        "line=%d; Xpos=%.2f; Ypos=%.2f; Zpos=%.2f; counter=%d\n" %
        (s.current_line, x, y, z, counter()))


#
# Come out of E-stop, turn the machine on, home, and switch to Auto mode.
#

c.state(STATE_ESTOP_RESET)
c.state(STATE_ON)
c.home(0)
c.home(1)
c.home(2)

start = time.time()
while time.time() - start < 5.0:
    s.poll()
    if all(s.homed[0:3]):
        break
    time.sleep(0.1)
else:
    raise SystemExit("timeout waiting for home")

c.mode(MODE_AUTO)
c.program_open('test.ngc')

epsilon = 0.000001


def mod_5_is_0(x):
    return abs((x + epsilon) % 5) < 2 * epsilon


def wait_complete_step():
    '''Single-stepping keeps task in RCS_EXEC (not RCS_DONE), so we cannot use
    wait_complete().  Instead, wait for exec_state to enter a wait-for-motion
    state (the step was accepted) and then leave it (the step finished).'''
    timeout = 5.0
    start = time.time()

    # Wait for the step to be accepted (exec_state enters a waiting state),
    # or for the interpreter to go idle (program finished).
    while time.time() - start < timeout:
        s.poll()
        if s.exec_state in _WAITING or s.interp_state == INTERP_IDLE:
            break
        time.sleep(0.02)

    # Wait for Task to be done waiting for Motion.
    while time.time() - start < timeout:
        s.poll()
        if s.exec_state not in _WAITING:
            return
        time.sleep(0.1)

    raise SystemExit('timeout in wait_complete_step()')


# Take first three steps (these cause no motion).
c.auto(AUTO_STEP)
c.auto(AUTO_STEP)
c.auto(AUTO_STEP)
wait_complete_step()

count = 0
while True:
    s.poll()
    if s.interp_state == INTERP_IDLE:
        sys.stderr.write("Finished:  Detected program finish\n")
        break

    (x, y) = positions()[0:2]
    if mod_5_is_0(x) and mod_5_is_0(y):
        # Both axes on goal; command the next step and wait for it to finish.
        sys.stderr.write("Taking step from X%.2f Y%.2f\n" % (x, y))
        c.auto(AUTO_STEP)
        wait_complete_step()

    print_state()

    count += 1
    if count >= 1000:  # Shouldn't happen, but prevent runaways
        sys.stderr.write("Finished:  Exceeded max cycles\n")
        sys.exit(1)

    time.sleep(0.1)

end_counter = counter()
if end_counter != 25:
    sys.stderr.write("End counter incorrect:  %d != 25\n" % end_counter)
    sys.exit(1)

sys.exit(0)
