#!/usr/bin/env python

from linuxcnccontrol import LinuxcncControl
from linuxcnccontrol import LinuxcncError

import linuxcnc
import hal

import time
import sys
import os


# FIXME: read these from ini file
x_acc_max = 10.0
x_vel_max = 4.0

y_acc_max = 10.0
y_vel_max = 4.0

z_acc_max = 10.0
z_vel_max = 4.0

xyz_acc_max = 10.0
xyz_vel_max = 4.0


def test_jog_continuous(axis, vel):
    axis_number = 'xyz'.index(axis)

    # FIXME: handle the other axes/joints too
    if vel > x_vel_max:
        print "limiting jog vel:  requested %.3f, getting %.3f" % (vel, x_vel_max)
        vel = x_vel_max

    h['x-vel-max'] = vel + 0.0001
    h['x-vel-min'] = (-1.0 * vel) - 0.0001

    print "commanding axis %s to jog at speed %.3f" % (axis, vel)
    e.c.jog(linuxcnc.JOG_CONTINUOUS, axis_number, vel)

    # wait to reach the requested jog speed
    start = time.time()
    while abs(h['%s-vel' % axis] - vel) > 0.0001:
        assert (time.time() - start) < 5.0, 'jog took too long to accelerate'
        print "    vel = %.3f" % h['%s-vel' % axis]
        time.sleep(0.010)
    print "    reached cruise velocity, vel = %.3f" % h['%s-vel' % axis]

    print "cruising for 1s"
    start = time.time()
    while (time.time() - start) < 1.0:
        assert abs(h['%s-vel' % axis] - vel) < 0.0001, 'jog speed fluctuated during cruise'
        time.sleep(0.010)
    print "    completed cruise"

    print "commanding axis %s to stop jogging" % axis
    e.c.jog(linuxcnc.JOG_STOP, 0)

    # wait to stop
    start = time.time()
    while h['%s-vel' % axis] != 0:
        assert (time.time() - start) < 5.0, 'jog took too long to stop'
        print "    vel = %.3f" % h['%s-vel' % axis]
        time.sleep(0.010)
    print "    reached jog stop, vel = %.3f" % h['%s-vel' % axis]


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

os.system("halcmd source ../postgui.hal")

h['x-acc-max'] = x_acc_max + 0.0001
h['x-acc-min'] = (-1.0 * x_acc_max) - 0.0001

h['x-vel-max'] = x_vel_max + 0.0001
h['x-vel-min'] = (-1.0 * x_vel_max) - 0.0001

h['y-acc-max'] = y_acc_max + 0.0001
h['y-acc-min'] = (-1.0 * y_acc_max) - 0.0001

h['y-vel-max'] = y_vel_max + 0.0001
h['y-vel-min'] = (-1.0 * y_vel_max) - 0.0001

h['z-acc-max'] = z_acc_max + 0.0001
h['z-acc-min'] = (-1.0 * z_acc_max) - 0.0001

h['z-vel-max'] = z_vel_max + 0.0001
h['z-vel-min'] = (-1.0 * z_vel_max) - 0.0001

h['xyz-acc-max'] = xyz_acc_max + 0.0001
h['xyz-acc-min'] = (-1.0 * xyz_acc_max) - 0.0001

h['xyz-vel-max'] = xyz_vel_max + 0.0001
h['xyz-vel-min'] = (-1.0 * xyz_vel_max) - 0.0001


e.set_state(linuxcnc.STATE_ESTOP_RESET)
e.set_state(linuxcnc.STATE_ON)
e.set_mode(linuxcnc.MODE_MANUAL)




#
# jog around a bit, see if check_constraints estops the machine
#

test_jog_continuous('x', vel=1.000)
test_jog_continuous('x', vel=0.123)
test_jog_continuous('x', vel=2.999)
test_jog_continuous('x', vel=6.000)


# If the test had jogged in to any any velocity or acceleration constraint
# violations it would have e-stopped the machine, which would have thrown
# an exception and terminated the program with a non-zero return code.
# Since we got here we know no constraints were violated, so we return
# success.
sys.exit(0)

