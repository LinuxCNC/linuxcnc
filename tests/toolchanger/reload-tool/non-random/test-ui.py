#!/usr/bin/env python

import linuxcnc
import linuxcnc_util
import hal

import math
import time
import sys
import subprocess
import os
import signal
import glob
import re


def wait_for_hal_pin(name, value, timeout=10):
    start_time = time.time()
    while (time.time() - start_time) < timeout:
        if h[name] == value:
            return
        time.sleep(0.1)
    raise RuntimeError("hal pin %s didn't get to %s after %.3f seconds" % (name, value, timeout))


# After doing something that should change the stat buffer, wait this
# long before polling to let the change propagate through.
# FIXME: this is bogus
stat_poll_wait = 0.100


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

h = hal.component("test-ui")

h.newpin("tool-change", hal.HAL_BIT, hal.HAL_IN)
h.newpin("tool-changed", hal.HAL_BIT, hal.HAL_OUT)
h["tool-changed"] = False

h.newpin("tool-prepare", hal.HAL_BIT, hal.HAL_IN)
h.newpin("tool-prepared", hal.HAL_BIT, hal.HAL_OUT)
h["tool-prepared"] = False

h.newpin("tool-number", hal.HAL_S32, hal.HAL_IN)
h.newpin("tool-prep-number", hal.HAL_S32, hal.HAL_IN)
h.newpin("tool-prep-pocket", hal.HAL_S32, hal.HAL_IN)

h.ready()

os.system("halcmd source ./postgui.hal")


l = linuxcnc_util.LinuxCNC()
# Wait for LinuxCNC to initialize itself so the Status buffer stabilizes.
l.wait_for_linuxcnc_startup()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()

c.mode(linuxcnc.MODE_MDI)


#
# At startup there's no tool in the spindle and no tool prep or change
# is being requested.
#

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 0)
assert(h['tool-prep-number'] == 0)
assert(h['tool-prep-pocket'] == 0)

s.poll()
assert(s.tool_in_spindle == 0)
assert(s.pocket_prepped == -1)


#
# Prepare T2
#

c.mdi('t2')
wait_for_hal_pin('tool-prepare', True)

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == True)
assert(h['tool-number'] == 0)
assert(h['tool-prep-number'] == 2)
assert(h['tool-prep-pocket'] == 46)

s.poll()
assert(s.tool_in_spindle == 0)
assert(s.pocket_prepped == -1)

h['tool-prepared'] = True
wait_for_hal_pin('tool-prepare', False)
h['tool-prepared'] = False

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 0)
assert(h['tool-prep-number'] == 2)
assert(h['tool-prep-pocket'] == 46)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 0)
assert(s.pocket_prepped == 6)  # ugh, non-random tc gives you tool-table-array index, not pocket


#
# Change to T2
#

c.mdi('m6')
wait_for_hal_pin('tool-change', True)

assert(h['tool-change'] == True)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 0)
assert(h['tool-prep-number'] == 2)
assert(h['tool-prep-pocket'] == 46)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 0)
assert(s.pocket_prepped == 6)

h['tool-changed'] = True
wait_for_hal_pin('tool-change', False)
h['tool-changed'] = False

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 2)
assert(h['tool-prep-number'] == 0)
assert(h['tool-prep-pocket'] == 0)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.pocket_prepped == -1)


#
# Prepare T12
#

c.mdi('t12')
wait_for_hal_pin('tool-prepare', True)

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == True)
assert(h['tool-number'] == 2)
assert(h['tool-prep-number'] == 12)
assert(h['tool-prep-pocket'] == 9)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.pocket_prepped == -1)

h['tool-prepared'] = True
wait_for_hal_pin('tool-prepare', False)
h['tool-prepared'] = False

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 2)
assert(h['tool-prep-number'] == 12)
assert(h['tool-prep-pocket'] == 9)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.pocket_prepped == 4)


#
# Prepare T2
# This tool is already in the spindle.
#

c.mdi('t2')
wait_for_hal_pin('tool-prepare', True)

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == True)
assert(h['tool-number'] == 2)
assert(h['tool-prep-number'] == 2)
assert(h['tool-prep-pocket'] == 46)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.pocket_prepped == 4)

h['tool-prepared'] = True
wait_for_hal_pin('tool-prepare', False)
h['tool-prepared'] = False

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 2)
assert(h['tool-prep-number'] == 2)
assert(h['tool-prep-pocket'] == 46)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.pocket_prepped == 6)


#
# Change to prepared tool (T2 again)
#

c.mdi('m6')
try:
    wait_for_hal_pin('tool-change', True, timeout=5)
except RuntimeError:
    # It *should* time out, no tool change should be performed since
    # the prepared tool is already in the spindle.
    pass

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 2)
assert(h['tool-prep-number'] == 2)
assert(h['tool-prep-pocket'] == 46)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.pocket_prepped == 6)


sys.exit(0)
