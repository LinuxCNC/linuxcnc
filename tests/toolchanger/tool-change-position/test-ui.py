#!/usr/bin/env python3

# M6 moves to [EMCIO]TOOL_CHANGE_POSITION, shown on the
# iocontrol.0.tool-change-position.default-* pins, or, when
# iocontrol.0.tool-change-position.custom-enable is TRUE, to the
# custom-* pins; a custom pin left at NaN keeps the default, an axis
# without a default (NaN) is not moved.
#
# Run from ini-position (TOOL_CHANGE_POSITION = 0.111 0.222 0.333)
# and no-ini-position (no TOOL_CHANGE_POSITION).

import linuxcnc
import linuxcnc_util
import hal

import math
import os
import sys

away = (1.0, 2.0, 3.0)
NAN = float('nan')

inifile = linuxcnc.ini(os.environ['INI_FILE_NAME'])
tcp = inifile.find('EMCIO', 'TOOL_CHANGE_POSITION')
if tcp:
    default = tuple(float(v) for v in tcp.split())
else:
    default = (NAN, NAN, NAN)
print("TOOL_CHANGE_POSITION: %s" % (tcp,))
EPSILON = 1e-6

IOC = 'iocontrol.0.tool-change-position.'

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)
l.wait_for_linuxcnc_startup()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()

c.mode(linuxcnc.MODE_MDI)
c.wait_complete()


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def check_position(expected, what):
    s.poll()
    pos = s.position[:3]
    for i, axis in enumerate("XYZ"):
        if abs(pos[i] - expected[i]) > EPSILON:
            fail("%s: %s is %.6f, expected %.6f (position %s)" % (what, axis, pos[i], expected[i], pos))
    print("ok: %s at %s" % (what, tuple(round(p, 6) for p in pos)))


def mdi(cmd):
    c.mdi(cmd)
    if c.wait_complete(10) == -1:
        fail("mdi '%s' did not complete" % cmd)


custom = [NAN, NAN, NAN]
custom_enable = False


def set_custom(axis, value):
    custom["xyz".index(axis)] = value
    hal.set_p(IOC + 'custom-' + axis, repr(value))


def set_custom_enable(value):
    global custom_enable
    custom_enable = value
    hal.set_p(IOC + 'custom-enable', '1' if value else '0')


def expected():
    # custom when enabled and not NaN, else the default, else not moved
    pos = []
    for i in range(3):
        if custom_enable and not math.isnan(custom[i]):
            pos.append(custom[i])
        elif not math.isnan(default[i]):
            pos.append(default[i])
        else:
            pos.append(away[i])
    return tuple(pos)


def change_tool(tool, what):
    # move away first, so the M6 move is visible on every axis
    mdi('g53 g0 x%f y%f z%f' % away)
    check_position(away, what + " (before M6)")
    mdi('t%d m6' % tool)
    check_position(expected(), what)


#
# pins: only the axes in [TRAJ]COORDINATES exist, defaults come from
# the INI, custom pins start as NaN, custom-enable as FALSE
#

for i, axis in enumerate("xyz"):
    v = hal.get_value(IOC + 'default-' + axis)
    if not (math.isnan(v) and math.isnan(default[i])) and not abs(v - default[i]) <= EPSILON:
        fail("default-%s is %r, expected %r" % (axis, v, default[i]))
    v = hal.get_value(IOC + 'custom-' + axis)
    if not math.isnan(v):
        fail("custom-%s is %r, expected NaN" % (axis, v))

for axis in "abcuvw":
    for kind in ("default", "custom"):
        try:
            hal.get_value(IOC + kind + '-' + axis)
        except Exception:
            continue
        fail("pin %s%s-%s exists, axis is not in [TRAJ]COORDINATES" % (IOC, kind, axis))

if hal.get_value(IOC + 'custom-enable'):
    fail("custom-enable starts TRUE")

# default-* is an output, it cannot be written from outside
try:
    hal.set_p(IOC + 'default-x', '5')
except Exception:
    pass
else:
    fail("default-x is writable")

#
# custom-enable FALSE: the INI position, or no move without one
#
change_tool(1, "M6, custom-enable FALSE")

#
# custom-enable TRUE and every custom pin NaN: same as FALSE
#
set_custom_enable(True)
change_tool(2, "M6, custom-enable TRUE, custom NaN")

#
# custom X only: X custom, Y and Z keep the default
#
set_custom('x', -1.5)
change_tool(1, "M6, custom X only")

#
# custom X Y Z
#
set_custom('y', -2.5)
set_custom('z', 4.5)
change_tool(2, "M6, custom X Y Z")

#
# custom-enable FALSE again: back to the default, the custom values
# are kept
#
set_custom_enable(False)
change_tool(1, "M6, custom-enable FALSE again")
if hal.get_value(IOC + 'custom-x') != -1.5:
    fail("custom-x was not kept")

#
# E-stop and machine on again do not reset the custom pins
#
c.state(linuxcnc.STATE_ESTOP)
c.wait_complete()
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete()
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()
set_custom_enable(True)
change_tool(2, "M6 after E-stop, custom kept")

print("all tests passed")
sys.exit(0)
