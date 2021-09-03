#!/usr/bin/env python3

import linuxcnc_util
import hal
import time
import sys
import os


# this is how long we wait for linuxcnc to do our bidding
timeout = 5.0


# unbuffer stdout
sys.stdout = os.fdopen(sys.stdout.fileno(), 'w')


def wait_for_pin_value(pin_name, value, timeout=5.0):
    start_time = time.time()
    while (time.time() - start_time) < timeout:
        time.sleep(0.1)
        if h[pin_name] == value:
            print("pin '%s' reached target value '%s' after %f seconds" % (pin_name, value, time.time() - start_time))
            return
    print("Error: pin '%s' didn't reach value '%s' within timeout of %f seconds (it's %s instead)" % (pin_name, value, timeout, h[pin_name]))
    sys.exit(1)


f = open('expected-startup-tool-number', 'r')
contents = f.read()
f.close()
expected_startup_tool_number = int(contents)
print("expecting tool number %d" % expected_startup_tool_number)

#
# set up pins
# shell out to halcmd to make nets to halui and motion
#

h = hal.component("test-ui")

h.newpin("tool-number", hal.HAL_S32, hal.HAL_IN)

h.ready() # mark the component as 'ready'

os.system("halcmd source ../../postgui.hal")


#
# connect to LinuxCNC
#

l = linuxcnc_util.LinuxCNC()

wait_for_pin_value('tool-number', expected_startup_tool_number)

l.wait_for_tool_in_spindle(expected_startup_tool_number)

sys.exit(0)

