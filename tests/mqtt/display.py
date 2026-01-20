#!/usr/bin/env python3

import time
import os
import sys

import linuxcnc
import linuxcnc_util
import hal

sys.stdout = os.fdopen(sys.stdout.fileno(), 'w')

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel
l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)

print("info: Turning machine on")
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete()

c.home(0)
c.home(1)
c.home(2)

print("info: Waiting for homing to complete")
l.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])

c.mode(linuxcnc.MODE_MDI)

print("info: Moving to X1")
c.mdi("G0 X1Y0Z0")
c.wait_complete()
s.poll()

h = hal.component("python-ui")

# Wait for next MQTT publication
name='mqtt-publisher.lastpublish'
timeout = 10
lastpublish = hal.get_value(name)
start_time = time.time()
while (time.time() - start_time) < timeout and \
      hal.get_value(name) == lastpublish:
    time.sleep(0.1)
if hal.get_value(name) == lastpublish:
    raise RuntimeError(f"error: no MQTT publishing happened within {timeout} seconds")

print("info: Turning machine off")
c.state(linuxcnc.STATE_OFF)

sys.exit(0)
