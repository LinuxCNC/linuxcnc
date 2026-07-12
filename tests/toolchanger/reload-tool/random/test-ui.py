# --- gomc compatibility shim (prepended) --------------------------------------
# Makes the original NML-based driver body run against the gomc REST/WS API:
#   linuxcnc  -> gmi client (command/stat/error_channel) + gmi.constants
#   hal       -> halcmd-backed shim; h[sig] reads/writes the io signals the old
#                userspace test component was connected to.
import gmi as _gmi
from gmi.constants import *
import subprocess as _subprocess


class _LinuxcncCompat:
    command = staticmethod(_gmi.Command)
    stat = staticmethod(_gmi.Stat)
    error_channel = staticmethod(_gmi.ErrorChannel)
    ini = staticmethod(_gmi.IniFile)

    def __getattr__(self, name):
        return globals()[name]


linuxcnc = _LinuxcncCompat()


class _HalCompat:
    HAL_S32 = HAL_U32 = HAL_FLOAT = HAL_BIT = 0
    HAL_IN = HAL_OUT = HAL_IO = 0

    def component(self, *a, **k):
        return self

    def newpin(self, *a, **k):
        return None

    def ready(self, *a, **k):
        pass

    def connect(self, *a, **k):
        pass

    def __getitem__(self, name):
        v = _subprocess.check_output(["halcmd", "gets", name]).decode().strip().split()[-1]
        if v in ("TRUE", "FALSE"):
            return v == "TRUE"
        return float(v)

    def __setitem__(self, name, val):
        _subprocess.run(["halcmd", "sets", name, "1" if val else "0"], check=True)


hal = _HalCompat()
# --- end shim -----------------------------------------------------------------


class _LinuxcncUtil:
    class LinuxCNC:
        def __init__(self, command=None, status=None, error=None):
            self._s = status if status is not None else _gmi.Stat()

        def wait_for_linuxcnc_startup(self, timeout=10.0):
            import time as _t
            start = _t.time()
            while _t.time() - start < timeout:
                self._s.poll()
                if (self._s.task_state == STATE_ESTOP) and (self._s.exec_state == EXEC_DONE) and (self._s.interp_state == INTERP_IDLE):
                    return
                _t.sleep(0.1)
            raise RuntimeError("timeout waiting for linuxcnc startup")


linuxcnc_util = _LinuxcncUtil()
#!/usr/bin/env python3


import math
import time
import sys
import subprocess
import os
import signal
import glob
import re

xtool   = 0.111 # [EMCIO]TOOL_CHANGE_POSITION x
ytool   = 0.222 # [EMCIO]TOOL_CHANGE_POSITION y
ztool   = 0.333 # [EMCIO]TOOL_CHANGE_POSITION z
EPSILON = 1e-10

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
h.newpin("tool-from-pocket", hal.HAL_S32, hal.HAL_IN)

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
# At startup tool 3 is in the spindle and no tool prep or change is
# being requested.
#

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 3)
assert(h['tool-prep-number'] == 0)
assert(h['tool-prep-pocket'] == 0)
assert(h['tool-from-pocket'] == 0)

s.poll()
assert(s.tool_in_spindle == 3)
assert(s.tool_from_pocket == 0)
assert(s.pocket_prepped == -1)


#
# Prepare T2
#

c.mdi('t2')
wait_for_hal_pin('tool-prepare', True)

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == True)
assert(h['tool-number'] == 3)
assert(h['tool-prep-number'] == 2)
assert(h['tool-prep-pocket'] == 46)
assert(h['tool-from-pocket'] == 0)

s.poll()
assert(s.tool_in_spindle == 3)
assert(s.tool_from_pocket == 0)
assert(s.pocket_prepped == -1)

h['tool-prepared'] = True
wait_for_hal_pin('tool-prepare', False)
h['tool-prepared'] = False

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 3)
assert(h['tool-prep-number'] == 2)
assert(h['tool-prep-pocket'] == 46)
assert(h['tool-from-pocket'] == 0)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 3)
assert(s.tool_from_pocket == 0)
assert(s.pocket_prepped == 46)  # random tc gives you pocket, which is the same as tool-table-array index


#
# Change to T2
#

c.mdi('m6')
wait_for_hal_pin('tool-change', True)

assert(h['tool-change'] == True)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 3)
assert(h['tool-prep-number'] == 2)
assert(h['tool-prep-pocket'] == 46)
assert(h['tool-from-pocket'] == 0)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 3)
assert(s.tool_from_pocket == 0)
assert(s.pocket_prepped == 46)

assert(abs(s.joint_position[0] -xtool) < EPSILON)
assert(abs(s.joint_position[1] -ytool) < EPSILON)
assert(abs(s.joint_position[2] -ztool) < EPSILON)

h['tool-changed'] = True
wait_for_hal_pin('tool-change', False)
h['tool-changed'] = False

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 2)
assert(h['tool-prep-number'] == 0)
assert(h['tool-prep-pocket'] == 0)
assert(h['tool-from-pocket'] == 46)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.tool_from_pocket == 46)
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
assert(h['tool-from-pocket'] == 46)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.tool_from_pocket == 46)
assert(s.pocket_prepped == -1)

h['tool-prepared'] = True
wait_for_hal_pin('tool-prepare', False)
h['tool-prepared'] = False

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 2)
assert(h['tool-prep-number'] == 12)
assert(h['tool-prep-pocket'] == 9)
assert(h['tool-from-pocket'] == 46)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.tool_from_pocket == 46)
assert(s.pocket_prepped == 9)


#
# Prepare T2
# This tool is already in the spindle.
#

c.mdi('t2')
try:
    wait_for_hal_pin('tool-prepare', True, timeout=5)
except RuntimeError:
    # It *should* time out, no tool prep should be performed since the
    # requested tool is already in the spindle.
    pass

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 2)
assert(h['tool-prep-number'] == 2)
assert(h['tool-prep-pocket'] == 0)
assert(h['tool-from-pocket'] == 46)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.tool_from_pocket == 46)
assert(s.pocket_prepped == 0)


#
# Change to prepared tool (T2)
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
assert(h['tool-prep-pocket'] == 0)
assert(h['tool-from-pocket'] == 46)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.tool_from_pocket == 46)
assert(s.pocket_prepped == 0)

#
# Prepare T0
#

c.mdi('t0')
wait_for_hal_pin('tool-prepare', True)

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == True)
assert(h['tool-number'] == 2)
assert(h['tool-prep-number'] == 0)
assert(h['tool-prep-pocket'] == 2)
assert(h['tool-from-pocket'] == 46)

s.poll()
assert(s.tool_in_spindle == 2)
assert(s.tool_from_pocket == 46)
assert(s.pocket_prepped == 0)

h['tool-prepared'] = True
wait_for_hal_pin('tool-prepare', False)
h['tool-prepared'] = False

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 2)
assert(h['tool-prep-number'] == 0)
assert(h['tool-prep-pocket'] == 2)
assert(h['tool-from-pocket'] == 46)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.tool_from_pocket == 46)
assert(s.pocket_prepped == 2)  # random tc gives you pocket, which is the same as tool-table-array index


#
# Change to T0
#

c.mdi('m6')
wait_for_hal_pin('tool-change', True)

assert(h['tool-change'] == True)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 2)
assert(h['tool-prep-number'] == 0)
assert(h['tool-prep-pocket'] == 2)
assert(h['tool-from-pocket'] == 46)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 2)
assert(s.tool_from_pocket == 46)
assert(s.pocket_prepped == 2)

h['tool-changed'] = True
wait_for_hal_pin('tool-change', False)
h['tool-changed'] = False

assert(h['tool-change'] == False)
assert(h['tool-prepare'] == False)
assert(h['tool-number'] == 0)
assert(h['tool-prep-number'] == 0)
assert(h['tool-prep-pocket'] == 0)
assert(h['tool-from-pocket'] == 0)

time.sleep(stat_poll_wait)
s.poll()
assert(s.tool_in_spindle == 0)
assert(s.tool_from_pocket == 0)
assert(s.pocket_prepped == -1)

sys.exit(0)

