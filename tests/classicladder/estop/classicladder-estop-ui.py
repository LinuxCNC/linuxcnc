#!/usr/bin/env python3

import os
import sys
import time

import linuxcnc
import linuxcnc_util
import hal

def wait_for_hal_pin(name, value, timeout=10):
    start_time = time.time()
    while (time.time() - start_time) < timeout:
        if hal.get_value(name) == value:
            return
        time.sleep(0.1)
    raise RuntimeError("error: hal pin %s didn't get to %s after %.3f seconds" % (name, value, timeout))


# unbuffer stdout
sys.stdout = os.fdopen(sys.stdout.fileno(), 'w')

retval = 0

h = hal.component("python-ui")
h.newpin("ext-estop-button", hal.HAL_BIT, hal.HAL_OUT)
h.ready() # mark the component as 'ready'

hal.connect('python-ui.ext-estop-button', 'ext-estop')

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete()

if False == hal.get_value('estop-all-ok'):
    print("error: estop not OK before ext-estop-button is touched")
    retval = 1

h['ext-estop-button'] = 1

# Give HAL and classicladder time to react
try:
    wait_for_hal_pin('estop-all-ok', False)
except RuntimeError as e:
    print("error: estop still OK after ext-estop-button is set")
    retval = 1

h['ext-estop-button'] = 0
c.state(linuxcnc.STATE_ESTOP_RESET)

# Give HAL and classicladder time to react
try:
    wait_for_hal_pin('estop-all-ok', True)
except RuntimeError as e:
    print("error: estop not OK aftet ext-estop-button is unset")
    retval = 1

print("exiting linuxcnc")

sys.exit(retval)
