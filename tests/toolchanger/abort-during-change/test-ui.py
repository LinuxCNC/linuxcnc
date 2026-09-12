#!/usr/bin/env python3

# Aborting a program while a tool prepare or tool change is pending used to
# leave emcStatus->io.status at RCS_EXEC forever: emcIoAbort() drops the
# tool-prepare/tool-change output pins, so read_tool_inputs() can never see the
# handshake complete and can never set the status back to RCS_DONE.  Task then
# blocks every subsequent command in WAITING_FOR_IO.
#
# This test aborts in both phases and checks that the machine comes back to
# RCS_DONE without pretending the tool change happened.

import linuxcnc
import hal

import time
import sys
import os


timeout = 1.0


def introspect():
    os.system("halcmd show pin python-ui")
    os.system("halcmd show pin iocontrol")


def wait_for_pin_value(pin_name, value, timeout=1):
    print("waiting for %s to go to %f (timeout=%f)" % (pin_name, value, timeout))

    start = time.time()
    while (h[pin_name] != value) and ((time.time() - start) < timeout):
        time.sleep(0.010)

    if h[pin_name] != value:
        print("timeout!  pin %s is %f, didn't get to %f" % (pin_name, h[pin_name], value))
        introspect()
        sys.exit(1)

    print("pin %s went to %f!" % (pin_name, value))


def verify_pin_value(pin_name, value):
    if (h[pin_name] != value):
        print("pin %s is %f, not %f" % (pin_name, h[pin_name], value))
        introspect()
        sys.exit(1)

    print("pin %s is %f" % (pin_name, value))


def wait_for_rcs_done(what, timeout=2):
    # this is the actual regression check: without the fix in
    # Task::emcIoAbort() the state stays RCS_EXEC forever
    print("waiting for RCS_DONE after %s" % what)

    start = time.time()
    while (time.time() - start) < timeout:
        s.poll()
        if s.state == linuxcnc.RCS_DONE:
            print("state is RCS_DONE after %s" % what)
            return
        time.sleep(0.010)

    s.poll()
    print("ERROR: stuck after %s: stat.state=%d (RCS_DONE=%d), exec_state=%d, "
          "interp_state=%d"
          % (what, s.state, linuxcnc.RCS_DONE, s.exec_state, s.interp_state))
    introspect()
    sys.exit(1)


def verify_tool_in_spindle(tool_number):
    s.poll()
    if s.tool_in_spindle != tool_number:
        print("ERROR: stat buffer .tool_in_spindle is %d, should be %d"
              % (s.tool_in_spindle, tool_number))
        introspect()
        sys.exit(1)
    verify_pin_value('tool-number', tool_number)
    print("tool %d is in the spindle" % tool_number)


def verify_mdi_works(gcode, axis_index, expected):
    # proves task is not stuck in WAITING_FOR_IO / WAITING_FOR_MOTION_AND_IO
    print("*** verifying the machine still accepts MDI: %s" % gcode)
    c.mode(linuxcnc.MODE_MDI)
    c.mdi(gcode)
    c.wait_complete(5)

    s.poll()
    actual = s.position[axis_index]
    if abs(actual - expected) > 0.001:
        print("ERROR: '%s' did not run (axis %d is at %f, expected %f)"
              % (gcode, axis_index, actual, expected))
        introspect()
        sys.exit(1)
    print("'%s' ran, axis %d is at %f" % (gcode, axis_index, actual))


def do_tool_prepare_handshake(tool_number, pocket_number):
    wait_for_pin_value('tool-prepare', 1)
    verify_pin_value('tool-prep-number', tool_number)
    verify_pin_value('tool-prep-pocket', pocket_number)

    h['tool-prepared'] = 1
    wait_for_pin_value('tool-prepare', 0)
    h['tool-prepared'] = 0


def do_tool_change_handshake():
    wait_for_pin_value('tool-change', 1)
    h['tool-changed'] = 1
    wait_for_pin_value('tool-change', 0)
    h['tool-changed'] = 0


#
# set up pins
#

h = hal.component("python-ui")

h.newpin("tool-number", hal.HAL_S32, hal.HAL_IN)
h.newpin("tool-prep-number", hal.HAL_S32, hal.HAL_IN)
h.newpin("tool-prep-pocket", hal.HAL_S32, hal.HAL_IN)
h.newpin("tool-from-pocket", hal.HAL_S32, hal.HAL_IN)

h.newpin("tool-prepare", hal.HAL_BIT, hal.HAL_IN)
h.newpin("tool-prepared", hal.HAL_BIT, hal.HAL_OUT)

h.newpin("tool-change", hal.HAL_BIT, hal.HAL_IN)
h.newpin("tool-changed", hal.HAL_BIT, hal.HAL_OUT)

h.ready()

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

verify_tool_in_spindle(0)


#
# baseline: a complete tool change still works
#

print("*** baseline 'T1 M6'")

c.mdi('t1 m6')
do_tool_prepare_handshake(tool_number=1, pocket_number=1)
do_tool_change_handshake()
c.wait_complete(5)

verify_tool_in_spindle(1)


#
# abort while the tool PREPARE handshake is pending
#

print("*** aborting during tool prepare")

c.mdi('t10 m6')
wait_for_pin_value('tool-prepare', 1)

c.abort()

wait_for_pin_value('tool-prepare', 0)
wait_for_rcs_done("abort during tool prepare")
verify_tool_in_spindle(1)
verify_mdi_works('g0 x1', 0, 1.0)


#
# abort while the tool CHANGE handshake is pending
#

print("*** aborting during tool change")

c.mode(linuxcnc.MODE_MDI)
c.mdi('t10 m6')
do_tool_prepare_handshake(tool_number=10, pocket_number=3)
wait_for_pin_value('tool-change', 1)

c.abort()

wait_for_pin_value('tool-change', 0)
wait_for_rcs_done("abort during tool change")

# the change did NOT happen, so the spindle must still hold the old tool --
# faking the handshake instead of fixing emcIoAbort() would break this
verify_tool_in_spindle(1)
verify_mdi_works('g0 x2', 0, 2.0)


#
# and a complete tool change still works afterwards
#

print("*** 'T10 M6' after the aborts")

c.mode(linuxcnc.MODE_MDI)
c.mdi('t10 m6')
do_tool_prepare_handshake(tool_number=10, pocket_number=3)
do_tool_change_handshake()
c.wait_complete(5)

verify_tool_in_spindle(10)

print("*** all good")
sys.exit(0)
