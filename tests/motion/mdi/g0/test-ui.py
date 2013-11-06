#!/usr/bin/env python

from linuxcnccontrol import LinuxcncControl
from linuxcnccontrol import LinuxcncError

import linuxcnc
import hal

import time
import sys
import os


#
# connect to LinuxCNC
#

e = LinuxcncControl()
e.g_raise_except = True


#
# set up pins
# shell out to halcmd to net our pins to where they need to go
#

h = hal.component("test-ui")

h.newpin("x-acc-max", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("x-acc-min", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("x-acc", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("x-vel-max", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("x-vel-min", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("x-vel", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("y-acc-max", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("y-acc-min", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("y-acc", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("y-vel-max", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("y-vel-min", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("y-vel", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("z-acc-max", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("z-acc-min", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("z-acc", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("z-vel-max", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("z-vel-min", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("z-vel", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("xyz-acc-max", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("xyz-acc-min", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("xyz-acc", hal.HAL_FLOAT, hal.HAL_IN)

h.newpin("xyz-vel-max", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("xyz-vel-min", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("xyz-vel", hal.HAL_FLOAT, hal.HAL_IN)

h.ready() # mark the component as 'ready'

os.system("halcmd source ../../postgui.hal")

h['x-acc-max'] = 50.0001
h['x-acc-min'] = -50.0001

h['x-vel-max'] = 4.0001
h['x-vel-min'] = -4.0001

h['y-acc-max'] = 50.0001
h['y-acc-min'] = -50.0001

h['y-vel-max'] = 4.0001
h['y-vel-min'] = -4.0001

h['z-acc-max'] = 50.0001
h['z-acc-min'] = -50.0001

h['z-vel-max'] = 4.0001
h['z-vel-min'] = -4.0001

h['xyz-acc-max'] = 50.0001
h['xyz-acc-min'] = -50.0001

h['xyz-vel-max'] = 4.0001
h['xyz-vel-min'] = -4.0001

e.set_state(linuxcnc.STATE_ESTOP_RESET)
e.set_state(linuxcnc.STATE_ON)
e.set_mode(linuxcnc.MODE_MDI)




#
# run a single g0 command by mdi
#

try:
    e.g('g0x1', wait=False)

    # have to sleep here, can't use wait=True above because it doesn't
    # return upon estop
    time.sleep(2)

except LinuxcncError, e:
    print "Linuxcnc Error: ", str(e)
    sys.exit(1)


# we return success, here, but test success/failure is determined by
# checkresults, which verifies the trajectory
sys.exit(0)

