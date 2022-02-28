#!/usr/bin/env python3

import linuxcnc
import hal

import math
import time
import sys
import subprocess
import os
import signal
import glob
import re


def wait_for_linuxcnc_startup(status, timeout=10.0):

    """Poll the Status buffer waiting for it to look initialized,
    rather than just allocated (all-zero).  Returns on success, throws
    RuntimeError on failure."""

    start_time = time.time()
    while time.time() - start_time < timeout:
        status.poll()
        if (status.angular_units == 0.0) \
            or (status.axes == 0) \
            or (status.axis_mask == 0) \
            or (status.cycle_time == 0.0) \
            or (status.exec_state != linuxcnc.EXEC_DONE) \
            or (status.interp_state != linuxcnc.INTERP_IDLE) \
            or (status.inpos == False) \
            or (status.linear_units == 0.0) \
            or (status.max_acceleration == 0.0) \
            or (status.max_velocity == 0.0) \
            or (status.program_units == 0.0) \
            or (status.rapidrate == 0.0) \
            or (status.state != linuxcnc.STATE_ESTOP) \
            or (status.task_state != linuxcnc.STATE_ESTOP):
            time.sleep(0.1)
        else:
            # looks good
            return

    # timeout, throw an exception
    raise RuntimeError


def verify_interp_vars(state, current_tool, current_pocket, selected_tool, selected_pocket):
    c.mdi('(debug,current_tool=#<_current_tool> current_pocket=#<_current_pocket> selected_tool=#<_selected_tool> selected_pocket=#<_selected_pocket>)')
    c.wait_complete()

    expected = "current_tool=%.6f current_pocket=%.6f selected_tool=%.6f selected_pocket=%.6f" % (current_tool, current_pocket, selected_tool, selected_pocket)

    while True:
        result = e.poll()
        if result == None:
            print("nothing from polling error channel")
            sys.exit(1)

        (type, msg) = result
        if type == linuxcnc.OPERATOR_DISPLAY:
            if msg == expected:
                # success!
                break
            print("state='%s', unexpected interp variables" % state)
            print("result: {}".format(msg))
            print("expected: {}".format(expected))
            sys.exit(1)
        else:
            print("state='%s', ignoring unexpected error type %d: %s" % (state, type, msg))

    print("state='%s', got expected interp variables:" % state)
    print("    current_tool=%.6f" % current_tool)
    print("    current_pocket=%.6f" % current_pocket)
    print("    selected_tool=%.6f" % selected_tool)
    print("    selected_pocket=%.6f" % selected_pocket)


def verify_io_pins(state, tool_number, tool_prep_number, tool_prep_pocket):
    if h['tool-number'] != tool_number:
        print("state=%s, expected io.tool-number=%d, got %d" % (state, tool_number, h['tool-number']))
        sys.exit(1)

    if h['tool-prep-number'] != tool_prep_number:
        print("state=%s, expected io.tool-prep-number=%d, got %d" % (state, tool_prep_number, h['tool-prep-number']))
        sys.exit(1)

    if h['tool-prep-pocket'] != tool_prep_pocket:
        print("state=%s, expected io.tool-prep-pocket=%d, got %d" % (state, tool_prep_pocket, h['tool-prep-pocket']))
        sys.exit(1)

    print("state='%s', got expected io pins:" % state)
    print("    tool-number=%d" % tool_number)
    print("    tool-prep-number=%d" % tool_prep_number)
    print("    tool-prep-pocket=%d" % tool_prep_pocket)


def verify_status_buffer(state, tool_in_spindle):
    deadline = time.time() + 1
    while time.time() < deadline:
        s.poll()
        if s.tool_in_spindle == tool_in_spindle:
            break
        time.sleep(.05)

    if s.tool_in_spindle != tool_in_spindle:
        print("state=%s, expected status.tool_in_spindle=%d, got %d" % (state, tool_in_spindle, s.tool_in_spindle))
        sys.exit(1)

    print("state='%s', got expected status buffer fields:" % state)
    print("    tool_in_spindle=%d" % tool_in_spindle)


def wait_for_hal_pin(pin_name, value, timeout=10):
    start = time.time()
    while time.time() < (start + timeout):
        if h[pin_name] == value:
            return
        time.sleep(0.1)
    print("timeout waiting for hal pin %s to go to %s!" % (pin_name, value))
    sys.exit(1)



c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


h = hal.component("test-ui")
h.newpin("tool-number", hal.HAL_S32, hal.HAL_IN)
h.newpin("tool-prep-number", hal.HAL_S32, hal.HAL_IN)
h.newpin("tool-prep-pocket", hal.HAL_S32, hal.HAL_IN)
h.newpin("tool-prepare", hal.HAL_BIT, hal.HAL_IN)
h.newpin("tool-prepared", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("tool-change", hal.HAL_BIT, hal.HAL_IN)
h.newpin("tool-changed", hal.HAL_BIT, hal.HAL_OUT)
h['tool-prepared'] = False
h['tool-changed'] = False
h.ready()

hal.connect('test-ui.tool-number', 'tool-number')
hal.connect('test-ui.tool-prep-number', 'tool-prep-number')
hal.connect('test-ui.tool-prep-pocket', 'tool-prep-pocket')

hal.connect('test-ui.tool-prepare', 'tool-prepare')
hal.connect('test-ui.tool-prepared', 'tool-prepared')
hal.connect('test-ui.tool-change', 'tool-change')
hal.connect('test-ui.tool-changed', 'tool-changed')


# Wait for LinuxCNC to initialize itself so the Status buffer stabilizes.
wait_for_linuxcnc_startup(s)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()

c.mode(linuxcnc.MODE_MDI)


#
# Starting state should be sane.
#

verify_status_buffer(state='init', tool_in_spindle=99999)
verify_io_pins(state='init', tool_number=99999, tool_prep_number=0, tool_prep_pocket=0)
verify_interp_vars(state='init', current_tool=99999, current_pocket=0, selected_tool=0, selected_pocket=-1)


#
# After "T1" prepares the tool.
#

c.mdi('T1')
wait_for_hal_pin('tool-prepare', True)
h['tool-prepared'] = True
wait_for_hal_pin('tool-prepare', False)
h['tool-prepared'] = False

verify_status_buffer(state='after T1', tool_in_spindle=99999)
verify_io_pins(state='after T1', tool_number=99999, tool_prep_number=1, tool_prep_pocket=3)
verify_interp_vars(state='after T1', current_tool=99999, current_pocket=0, selected_tool=1, selected_pocket=3)


#
# After "M6" changes to the prepared tool.
#

c.mdi('M6')
wait_for_hal_pin('tool-change', True)
h['tool-changed'] = True
wait_for_hal_pin('tool-change', False)
h['tool-changed'] = False

verify_status_buffer(state='after T1 M6', tool_in_spindle=1)
verify_io_pins(state='after T1 M6', tool_number=1, tool_prep_number=0, tool_prep_pocket=0)
verify_interp_vars(state='after T1 M6', current_tool=1, current_pocket=0, selected_tool=1, selected_pocket=-1)


#
# After "T10" prepares the tool.
#

c.mdi('T10')
wait_for_hal_pin('tool-prepare', True)
h['tool-prepared'] = True
wait_for_hal_pin('tool-prepare', False)
h['tool-prepared'] = False

verify_status_buffer(state='after T10', tool_in_spindle=1)
verify_io_pins(state='after T10', tool_number=1, tool_prep_number=10, tool_prep_pocket=50)
verify_interp_vars(state='after T10', current_tool=1, current_pocket=0, selected_tool=10, selected_pocket=50)


#
# After "M6" changes to the prepared tool.
#

c.mdi('M6')
wait_for_hal_pin('tool-change', True)
h['tool-changed'] = True
wait_for_hal_pin('tool-change', False)
h['tool-changed'] = False

verify_status_buffer(state='after T10 M6', tool_in_spindle=10)
verify_io_pins(state='after T10 M6', tool_number=10, tool_prep_number=0, tool_prep_pocket=0)
verify_interp_vars(state='after T10 M6', current_tool=10, current_pocket=0, selected_tool=10, selected_pocket=-1)


#
# After "T99999" prepares a tool.
#

c.mdi('T99999')
wait_for_hal_pin('tool-prepare', True)
h['tool-prepared'] = True
wait_for_hal_pin('tool-prepare', False)
h['tool-prepared'] = False

verify_status_buffer(state='after T99999', tool_in_spindle=10)
verify_io_pins(state='after T99999', tool_number=10, tool_prep_number=99999, tool_prep_pocket=3)
verify_interp_vars(state='after T99999', current_tool=10, current_pocket=0, selected_tool=99999, selected_pocket=3)


#
# After "M6" changes to the prepared tool.
#

c.mdi('M6')
wait_for_hal_pin('tool-change', True)
h['tool-changed'] = True
wait_for_hal_pin('tool-change', False)
h['tool-changed'] = False

verify_status_buffer(state='after T99999 M6', tool_in_spindle=99999)
verify_io_pins(state='after T99999 M6', tool_number=99999, tool_prep_number=0, tool_prep_pocket=0)
verify_interp_vars(state='after T99999 M6', current_tool=99999, current_pocket=0, selected_tool=99999, selected_pocket=-1)


sys.exit(0)

