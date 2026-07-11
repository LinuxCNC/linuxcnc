#!/usr/bin/env python3

import linuxcnc
import gmi
import gmi.constants as _gk
for _n in dir(_gk):
    if not _n.startswith('_'):
        setattr(linuxcnc, _n, getattr(_gk, _n))

import math
import time
import sys
import subprocess
import os
import signal
import glob
import re


def wait_for_linuxcnc_startup(status, timeout=10.0):
    # Simplified for gmi.Stat (lacks cycle_time/max_acceleration/program_units).
    # test.sh already waits for milltask before this runs.
    start_time = time.time()
    while time.time() - start_time < timeout:
        status.poll()
        if (status.task_state == linuxcnc.STATE_ESTOP) \
            and (status.interp_state == linuxcnc.INTERP_IDLE) \
            and (status.exec_state == linuxcnc.EXEC_DONE):
            return
        time.sleep(0.1)
    raise RuntimeError


def wait_for_mdi_queue(queue_len, timeout=10):
    start_time = time.time()
    while (time.time() - start_time) < timeout:
        s.poll()
        if s.queued_mdi_commands == queue_len:
            return
        time.sleep(0.1)
    print("queued_mdi_commands at %d after %.3f seconds" % (s.queued_mdi_commands, timeout))
    sys.exit(1)


c = gmi.Command()
s = gmi.Stat()
e = gmi.ErrorChannel()


def _poke(v):
    subprocess.call(['halcmd', 'setp', 'motion.digital-in-00', '1' if v else '0'])


# Wait for LinuxCNC to initialize itself so the Status buffer stabilizes.
wait_for_linuxcnc_startup(s)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()

c.mode(linuxcnc.MODE_MDI)

# At startup there's nothing in the queue.
s.poll()
assert(s.queued_mdi_commands == 0)

# Block Motion from draining the queue, by asking it to wait for us to
# poke a synchronized digital input.  Wait up to 30 seconds for digital
# input 0 to go High.
c.mdi('m66 p0 l3 q30')

s.poll()
assert(s.queued_mdi_commands == 0)

# Put an MDI command on Task's MDI queue.
c.mdi('g4 p0')

s.poll()
assert(s.queued_mdi_commands == 1)

# Add another MDI command that we can control.
# Wait up to 30 seconds for digital input 0 to go Low.
c.mdi('m66 p0 l4 q30')

s.poll()
assert(s.queued_mdi_commands == 2)

c.mdi('g4 p0')

s.poll()
assert(s.queued_mdi_commands == 3)

_poke(True)
wait_for_mdi_queue(queue_len=1, timeout=10)

_poke(False)
wait_for_mdi_queue(queue_len=0, timeout=10)

sys.exit(0)

