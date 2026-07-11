#!/usr/bin/env python3

# Ported to the gomc REST/WS API (gmi client).  The classic NML linuxcnc module
# is command/stat-less now; drive via gmi.Command/Stat and read HAL pins with
# halcmd (there is no userspace hal.component anymore).

import gmi
from gmi.constants import *

import subprocess
import time
import sys


class _HAL:
    @staticmethod
    def get_value(pin):
        out = subprocess.check_output(["halcmd", "getp", pin])
        return float(out.split()[-1])


hal = _HAL()


def wait_for_linuxcnc_startup(status, timeout=10.0):
    """Poll Status until the interpreter is idle and settled."""
    start_time = time.time()
    while time.time() - start_time < timeout:
        status.poll()
        if (status.exec_state != EXEC_DONE) \
            or (status.interp_state != INTERP_IDLE) \
            or (status.task_state != STATE_ESTOP):
            time.sleep(0.1)
        else:
            return
    raise RuntimeError("timeout waiting for startup")


def settle():
    # gmi.Stat is a live WS stream that lags command completion slightly;
    # settle before reading back HAL/spindle state.
    time.sleep(0.5)
    s.poll()


c = gmi.Command()
s = gmi.Stat()
e = gmi.ErrorChannel()

# Wait for LinuxCNC to initialize itself so the Status buffer stabilizes.
wait_for_linuxcnc_startup(s)

c.state(STATE_ESTOP_RESET)
c.state(STATE_ON)
c.wait_complete()


# Check G-code / MDI spindle commands

c.mode(MODE_MDI)

c.mdi("M3 S10 $0")
c.mdi("M3 S10 $1")
c.mdi("M3 S10 $2")
c.wait_complete()
settle()

assert hal.get_value('spindle.0.speed-out') == 100
assert s.spindle[0]['speed'] == 100
assert hal.get_value('spindle.1.speed-out') == 200
assert s.spindle[1]['speed'] == 200
assert hal.get_value('spindle.2.speed-out') == 300
assert s.spindle[2]['speed'] == 300

c.mdi("M3 S10000 $0")
c.mdi("M3 S10000 $1")
c.mdi("M3 S10000 $2")
c.wait_complete()
settle()

assert hal.get_value('spindle.0.speed-out') == 1000
assert s.spindle[0]['speed'] == 1000
assert hal.get_value('spindle.1.speed-out') == 2000
assert s.spindle[1]['speed'] == 2000
assert hal.get_value('spindle.2.speed-out') == 3000
assert s.spindle[2]['speed'] == 3000

c.mdi("M4 S10 $0")
c.mdi("M4 S10 $1")
c.mdi("M4 S10 $2")
c.wait_complete()
settle()

assert hal.get_value('spindle.0.speed-out') == -500
assert s.spindle[0]['speed'] == -500
assert hal.get_value('spindle.1.speed-out') == -200
assert s.spindle[1]['speed'] == -200
assert hal.get_value('spindle.2.speed-out') == -300
assert s.spindle[2]['speed'] == -300

c.mdi("M4 S10000 $0")
c.mdi("M4 S10000 $1")
c.mdi("M4 S10000 $2")
c.wait_complete()
settle()

assert hal.get_value('spindle.0.speed-out') == -1500
assert s.spindle[0]['speed'] == -1500
assert hal.get_value('spindle.1.speed-out') == -2500
assert s.spindle[1]['speed'] == -2500
assert hal.get_value('spindle.2.speed-out') == -3000
assert s.spindle[2]['speed'] == -3000

# Check Python interface commands

c.spindle(SPINDLE_FORWARD, 10, 0, 0)
c.spindle(SPINDLE_FORWARD, 10, 1, 0)
c.spindle(SPINDLE_FORWARD, 10, 2, 0)
c.wait_complete()
settle()

assert hal.get_value('spindle.0.speed-out') == 100
assert s.spindle[0]['speed'] == 100
assert hal.get_value('spindle.1.speed-out') == 200
assert s.spindle[1]['speed'] == 200
assert hal.get_value('spindle.2.speed-out') == 300
assert s.spindle[2]['speed'] == 300

c.spindle(SPINDLE_FORWARD, 10000, 0, 0)
c.spindle(SPINDLE_FORWARD, 10000, 1, 0)
c.spindle(SPINDLE_FORWARD, 10000, 2, 0)
c.wait_complete()
settle()

assert hal.get_value('spindle.0.speed-out') == 1000
assert s.spindle[0]['speed'] == 1000
assert hal.get_value('spindle.1.speed-out') == 2000
assert s.spindle[1]['speed'] == 2000
assert hal.get_value('spindle.2.speed-out') == 3000
assert s.spindle[2]['speed'] == 3000

c.spindle(SPINDLE_REVERSE, 10, 0, 0)
c.spindle(SPINDLE_REVERSE, 10, 1, 0)
c.spindle(SPINDLE_REVERSE, 10, 2, 0)
c.wait_complete()
settle()

assert hal.get_value('spindle.0.speed-out') == -500
assert s.spindle[0]['speed'] == -500
assert hal.get_value('spindle.1.speed-out') == -200
assert s.spindle[1]['speed'] == -200
assert hal.get_value('spindle.2.speed-out') == -300
assert s.spindle[2]['speed'] == -300

c.spindle(SPINDLE_REVERSE, 10000, 0, 0)
c.spindle(SPINDLE_REVERSE, 10000, 1, 0)
c.spindle(SPINDLE_REVERSE, 10000, 2, 0)
c.wait_complete()
settle()

assert hal.get_value('spindle.0.speed-out') == -1500
assert s.spindle[0]['speed'] == -1500
assert hal.get_value('spindle.1.speed-out') == -2500
assert s.spindle[1]['speed'] == -2500
assert hal.get_value('spindle.2.speed-out') == -3000
assert s.spindle[2]['speed'] == -3000

sys.exit(0)
