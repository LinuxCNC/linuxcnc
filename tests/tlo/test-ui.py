#!/usr/bin/env python3

import linuxcnc
import hal

import time
import sys
import os
import math


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
        print("timeout!  pin %s didn't get to %f" % (pin_name, value))
        introspect()
        sys.exit(1)

    print("pin %s went to %f!" % (pin_name, value))


def verify_pin_value(pin_name, value):
    if (h[pin_name] != value):
        print("pin %s is %f, expected %f" % (pin_name, h[pin_name], value))
        sys.exit(1);

    print("pin %s is %f" % (pin_name, value))


def get_interp_param(param_number):
    c.mdi("(debug, #%d)" % param_number)
    while c.wait_complete() == -1:
        pass

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
    if math.fabs(param_value - expected_value) > 0.000001:
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


def verify_tool(tool, x, y, z, a, b, c, u, v, w, diameter, front_angle, back_angle):
    wait_for_pin_value('tool-number', tool)
    verify_interp_param(5400, tool)

    # verify the current location, as offset by TLO
    verify_interp_param(5420, x)
    verify_interp_param(5421, y)
    verify_interp_param(5422, z)
    verify_interp_param(5423, a)
    verify_interp_param(5424, b)
    verify_interp_param(5425, c)
    verify_interp_param(5426, u)
    verify_interp_param(5427, v)
    verify_interp_param(5428, w)

    # verify tool diameter & angles
    verify_interp_param(5410, diameter)
    verify_interp_param(5411, front_angle)
    verify_interp_param(5412, back_angle)

    return True




#
# connect to HAL
# shell out to halcmd to net our pins to where they need to go
#

h = hal.component("python-ui")

h.newpin("tool-number", hal.HAL_S32, hal.HAL_IN)

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


#
# this is a non-random toolchanger, so it starts with no tool in the spindle
# make sure the startup condition is sane
#

print("*** starting up, expecting T0 in spindle & no TLO")

verify_tool(
    tool=0,
    x=0, y=0, z=0,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0,
    front_angle=0,
    back_angle=0
)


print("*** load T100 but dont apply TLO")

c.mdi('t100 m6')
c.wait_complete()

verify_tool(
    tool=100,
    x=0, y=0, z=0,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)


print("*** apply TLO of loaded tool (T100)")

c.mdi('g43')
c.wait_complete()

verify_tool(
    tool=100,
    x=-2, y=0, z=-1,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)


print("*** apply TLO of T200 instead")

c.mdi('g43 h200')
c.wait_complete()

verify_tool(
    tool=100,
    x=-0.2, y=0, z=-0.1,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)


print("*** try to add in TLO with no H-word, should fail")

# first drain the error queue
start = time.time()
while (time.time() - start) < 2:
    error = e.poll()
    if error == None:
        # no more queued errors, continue with test
        break

c.mdi('g43.2')
c.wait_complete()
error = e.poll()
if error[1] != "G43.2: No axes specified and H word missing":
    print("G43.2 with missing H-word did not produce expected error")
    print("got [%s]" % error[1])
    sys.exit(1)


print("*** add in TLO of T100")

c.mdi('g43.2 h100')
c.wait_complete()

verify_tool(
    tool=100,
    x=-2.2, y=0, z=-1.1,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)


print("*** add in TLO of T300")

c.mdi('g43.2 h300')
c.wait_complete()

verify_tool(
    tool=100,
    x=-2.22, y=0, z=-1.11,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)


print("*** add in TLO of T400")

c.mdi('g43.2 h400')
c.wait_complete()

verify_tool(
    tool=100,
    x=-2.222, y=0, z=-1.111,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)


print("*** add in TLO of T400 again")

c.mdi('g43.2 h400')
c.wait_complete()

verify_tool(
    tool=100,
    x=-2.224, y=0, z=-1.112,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)

print("*** now let's try it rotated.  first, just rotate but don't change the tlo")

c.mdi('g10 l2 p1 r33')
c.wait_complete()

verify_tool(
    tool=100,
    x=-2.224 * math.cos(math.radians(33)), y=2.224 * math.sin(math.radians(33)), z=-1.112,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)

print("*** clear tlo")

c.mdi('g49')
c.wait_complete()

verify_tool(
    tool=100,
    x=-0, y=0, z=0,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)

print("*** apply t100")

c.mdi('g43 h100')
c.wait_complete()

verify_tool(
    tool=100,
    x=-2*math.cos(math.radians(33)), y=2*math.sin(math.radians(33)), z=-1,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)

print("*** add in t200")

c.mdi('g43.2 h200')
c.wait_complete()

verify_tool(
    tool=100,
    x=-2.2*math.cos(math.radians(33)), y=2.2*math.sin(math.radians(33)), z=-1.1,
    a=0, b=0, c=0,
    u=0, v=0, w=0,
    diameter=0.125,
    front_angle=0,
    back_angle=0
)



# if we get here it all worked!
sys.exit(0)

