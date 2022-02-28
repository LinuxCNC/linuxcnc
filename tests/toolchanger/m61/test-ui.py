#!/usr/bin/env python3

import linuxcnc
import hal

import time
import sys
import os


# this is how long we wait for linuxcnc to do our bidding
timeout = 1.0


def introspect():
    os.system("halcmd show pin python-ui")


def wait_for_pin_value(pin_name, value, timeout=1):
    print("waiting for %s to go to %f (timeout=%f)" % (pin_name, value, timeout))

    start = time.time()
    while (h[pin_name] != value) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[pin_name] != value:
        print("timeout!  pin %s is %f, didn't get to %f" % (pin_name, h[pin_name], value))
        introspect()
        sys.exit(1)

    print("pin %s went to %f!" % (pin_name, value))


def verify_pin_value(pin_name, value):
    if (h[pin_name] != value):
        print("pin %s is %f, not %f" % (pin_name, h[pin_name], value))
        sys.exit(1);

    print("pin %s is %f" % (pin_name, value))


def get_interp_param(param_number):
    c.mdi("(debug, #%d)" % param_number)
    c.wait_complete()

    # wait up to 2 seconds for a reply
    start = time.time()
    while (time.time() - start) < 2:
        error = e.poll()
        if error == None:
            time.sleep(0.010)
            continue

        kind, text = error
        if kind == linuxcnc.OPERATOR_DISPLAY:
            return float(text)

        print(text)

    print("error getting parameter %d" % param_number)
    return None


def verify_interp_param(param_number, expected_value):
    param_value = get_interp_param(param_number)
    print("interp param #%d = %f (expecting %f)" % (param_number, param_value, expected_value))
    if param_value != expected_value:
        print("ERROR: interp param #%d = %f, expected %f" % (param_number, param_value, expected_value))
        sys.exit(1)


def verify_stable_pin_values(pins, duration=1):
    start = time.time()
    while (time.time() - start) < duration:
        for pin_name in list(pins.keys()):
            val = h[pin_name]
            if val != pins[pin_name]:
                print("ERROR: pin %s = %f (expected %f)" % (pin_name, val, pin[pin_name]))
                sys.exit(1)
        time.sleep(0.010)


def verify_tool_number(tool_number):
    verify_interp_param(5400, tool_number)        # 5400 == tool in spindle
    verify_pin_value('tool-number', tool_number)  # pin from iocontrol

    # verify stat buffer
    s.poll()
    if s.tool_in_spindle != tool_number:
        print("ERROR: stat buffer .tool_in_spindle is %f, should be %f" % (s.tool_in_spindle, tool_number))
        sys.exit(1)
    print("stat buffer .tool_in_spindle is %f" % s.tool_in_spindle)


def do_tool_change_handshake(tool_number, pocket_number):
    # prepare for tool change
    wait_for_pin_value('tool-prepare', 1)
    verify_pin_value('tool-prep-number', tool_number)
    verify_pin_value('tool-prep-pocket', pocket_number)

    h['tool-prepared'] = 1
    wait_for_pin_value('tool-prepare', 0)
    h['tool-prepared'] = 0

    time.sleep(0.1)
    s.poll()
    print("tool prepare done, s.pocket_prepped = ", s.pocket_prepped)
    if s.pocket_prepped != pocket_number:
        print("ERROR: wrong pocket prepped in stat buffer (got %d, expected %d)" % (s.pocket_prepped, pocket_number))
        sys.exit(1)

    # change tool
    wait_for_pin_value('tool-change', 1)
    h['tool-changed'] = 1
    wait_for_pin_value('tool-change', 0)
    h['tool-changed'] = 0




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

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()


# at startup, we should have the special tool 0 in the spindle, meaning
# "no tool" or "unknown tool"

verify_tool_number(0)


#
# test m6 to get a baseline
#

print("*** starting 'T1 M6' tool change")

c.mdi('t1 m6')
c.wait_complete()

do_tool_change_handshake(tool_number=1, pocket_number=1)

print("*** tool change complete")
verify_tool_number(1)

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

c.mdi('g43')
c.wait_complete()

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

print("*** starting 'M61 Q10' tool change")

c.mdi('m61 q10')
c.wait_complete()

verify_stable_pin_values(
    {
        'tool-change': 0,
        'tool-prep-number': 0,
        'tool-prep-pocket': 0,
        'tool-prepare': 0
    },
    duration=1
)

verify_tool_number(10)

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

c.mdi('g43')
c.wait_complete()

verify_interp_param(5420, 0)      # current x
verify_interp_param(5421, 0)      # current y
verify_interp_param(5422, -3)     # current z
verify_interp_param(5423, 0)      # current a
verify_interp_param(5424, 0)      # current b
verify_interp_param(5425, 0)      # current c
verify_interp_param(5426, 0)      # current u
verify_interp_param(5427, 0)      # current v
verify_interp_param(5428, 0)      # current w


#
# use M6 to unload the spindle (T0)
#

print("*** using 'T0 M6' to unload the spindle")
c.mdi("t0 m6")
c.wait_complete()
do_tool_change_handshake(tool_number=0, pocket_number=0)
verify_tool_number(0)


#
# use M61 to load T1 in the spindle again
#

print("*** using 'M61 Q1' to load a tool again")
c.mdi("m61 q1")
c.wait_complete()
verify_tool_number(1)


#
# use M61 to unload the spindle (T0)
#

print("*** using 'M61 Q0' to unload the spindle again")
c.mdi("m61 q0")
c.wait_complete()
verify_tool_number(0)


# if we get here it all worked!
sys.exit(0)

