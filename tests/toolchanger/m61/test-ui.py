#!/usr/bin/env python

from linuxcnccontrol import LinuxcncControl
from linuxcnccontrol import LinuxcncError

import linuxcnc
import hal

import time
import sys
import os


def introspect():
    os.system("halcmd show pin python-ui")


def wait_for_pin_value(pin_name, value, timeout=1):
    print "waiting for %s to go to %f (timeout=%f)" % (pin_name, value, timeout)

    start = time.time()
    while (h[pin_name] != value) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[pin_name] != value:
        print "timeout!  pin %s didn't get to %f" % (pin_name, value)
        introspect()
        sys.exit(1)

    print "pin %s went to %f!" % (pin_name, value)


def verify_pin_value(pin_name, value):
    if (h[pin_name] != value):
        print "pin %s is not %f" % (pin_name, value)
        sys.exit(1);

    print "pin %s is %f" % (pin_name, value)


def get_interp_param(param_number):
    e.c.mdi("(debug, #%d)" % param_number)
    while e.c.wait_complete() == -1:
        pass

    # wait up to 2 seconds for a reply
    start = time.time()
    while (time.time() - start) < 2:
        error = e.e.poll()
        if error == None:
            time.sleep(0.010)
            continue

        kind, text = error
        if kind == linuxcnc.OPERATOR_DISPLAY:
            return float(text)

        print text

    print "error getting parameter %d" % param_number
    return None


def verify_interp_param(param_number, expected_value):
    param_value = get_interp_param(param_number)
    print "interp param #%d = %f (expecting %f)" % (param_number, param_value, expected_value)
    if param_value != expected_value:
        print "ERROR: interp param #%d = %f, expected %f" % (param_number, param_value, expected_value)
        sys.exit(1)


def verify_stable_pin_values(pins, duration=1):
    start = time.time()
    while (time.time() - start) < duration:
        for pin_name in pins.keys():
            val = h[pin_name]
            if val != pins[pin_name]:
                print "ERROR: pin %s = %f (expected %f)" % (pin_name, val, pin[pin_name])
                sys.exit(1)
        time.sleep(0.010)




#
# set up pins
# shell out to halcmd to net our pins to where they need to go
#

h = hal.component("python-ui")

h.newpin("tool-number", hal.HAL_S32, hal.HAL_IN)
h.newpin("tool-prep-number", hal.HAL_S32, hal.HAL_IN)
h.newpin("tool-prep-pocket", hal.HAL_S32, hal.HAL_IN)

h.newpin("tool-prepare", hal.HAL_BIT, hal.HAL_IN)
h.newpin("tool-prepared", hal.HAL_BIT, hal.HAL_OUT)

h.newpin("tool-change", hal.HAL_BIT, hal.HAL_IN)
h.newpin("tool-changed", hal.HAL_BIT, hal.HAL_OUT)

h.ready() # mark the component as 'ready'

os.system("halcmd source ./postgui.hal")


#
# connect to LinuxCNC
#

e = LinuxcncControl()
e.g_raise_except = False
e.set_state(linuxcnc.STATE_ESTOP_RESET)
e.set_state(linuxcnc.STATE_ON)
e.set_mode(linuxcnc.MODE_MDI)




#
# test m6 to get a baseline
#

e.g('t1 m6')

# prepare for tool change
wait_for_pin_value('tool-prepare', 1)
verify_pin_value('tool-prep-number', 1)
verify_pin_value('tool-prep-pocket', 1)
h['tool-prepared'] = 1
wait_for_pin_value('tool-prepare', 0)
h['tool-prepared'] = 0

time.sleep(0.1)
e.s.poll()
print "tool prepare done, e.s.pocket_prepped = ", e.s.pocket_prepped

# change tool
wait_for_pin_value('tool-change', 1)
h['tool-changed'] = 1
wait_for_pin_value('tool-change', 0)
h['tool-changed'] = 0

time.sleep(0.1)
e.s.poll()
print "tool change done, e.s.tool_in_spindle = ", e.s.tool_in_spindle
print "current tool: ", e.get_current_tool()

verify_interp_param(5400, 1)      # tool in spindle
verify_interp_param(5401, 0)      # tlo x
verify_interp_param(5402, 0)      # tlo y
verify_interp_param(5403, 1)      # tlo z
verify_interp_param(5404, 0)      # tlo a
verify_interp_param(5405, 0)      # tlo b
verify_interp_param(5406, 0)      # tlo c
verify_interp_param(5407, 0)      # tlo u
verify_interp_param(5408, 0)      # tlo v
verify_interp_param(5409, 0)      # tlo w
verify_interp_param(5410, 0.125)  # tool diameter
verify_interp_param(5411, 0)      # front angle
verify_interp_param(5412, 0)      # back angle
verify_interp_param(5413, 0)      # tool orientation

verify_interp_param(5420, 0)      # current x
verify_interp_param(5421, 0)      # current y
verify_interp_param(5422, 0)      # current z
verify_interp_param(5423, 0)      # current a
verify_interp_param(5424, 0)      # current b
verify_interp_param(5425, 0)      # current c
verify_interp_param(5426, 0)      # current u
verify_interp_param(5427, 0)      # current v
verify_interp_param(5428, 0)      # current w

e.g('g43')

verify_interp_param(5420, 0)      # current x
verify_interp_param(5421, 0)      # current y
verify_interp_param(5422, -1)     # current z
verify_interp_param(5423, 0)      # current a
verify_interp_param(5424, 0)      # current b
verify_interp_param(5425, 0)      # current c
verify_interp_param(5426, 0)      # current u
verify_interp_param(5427, 0)      # current v
verify_interp_param(5428, 0)      # current w

introspect()




#
# now finally test m61
#

e.g('m61 q10')

verify_stable_pin_values(
    {
        'tool-change': 0,
        'tool-prep-number': 0,
        'tool-prep-pocket': 0,
        'tool-prepare': 0
    },
    duration=1
)

print "m61 done, e.s.tool_in_spindle = ", e.s.tool_in_spindle
print "current tool: ", e.get_current_tool()

verify_interp_param(5400, 10)     # tool in spindle
verify_interp_param(5401, 0)      # tlo x
verify_interp_param(5402, 0)      # tlo y
verify_interp_param(5403, 3)      # tlo z
verify_interp_param(5404, 0)      # tlo a
verify_interp_param(5405, 0)      # tlo b
verify_interp_param(5406, 0)      # tlo c
verify_interp_param(5407, 0)      # tlo u
verify_interp_param(5408, 0)      # tlo v
verify_interp_param(5409, 0)      # tlo w
verify_interp_param(5410, 0.500)  # tool diameter
verify_interp_param(5411, 0)      # front angle
verify_interp_param(5412, 0)      # back angle
verify_interp_param(5413, 0)      # tool orientation

verify_interp_param(5420, 0)      # current x
verify_interp_param(5421, 0)      # current y
verify_interp_param(5422, -1)     # current z
verify_interp_param(5423, 0)      # current a
verify_interp_param(5424, 0)      # current b
verify_interp_param(5425, 0)      # current c
verify_interp_param(5426, 0)      # current u
verify_interp_param(5427, 0)      # current v
verify_interp_param(5428, 0)      # current w

e.g('g43')

verify_interp_param(5420, 0)      # current x
verify_interp_param(5421, 0)      # current y
verify_interp_param(5422, -3)     # current z
verify_interp_param(5423, 0)      # current a
verify_interp_param(5424, 0)      # current b
verify_interp_param(5425, 0)      # current c
verify_interp_param(5426, 0)      # current u
verify_interp_param(5427, 0)      # current v
verify_interp_param(5428, 0)      # current w


# if we get here it all worked!
sys.exit(0)

