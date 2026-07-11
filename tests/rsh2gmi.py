#!/usr/bin/env python3
# Minimal linuxcncrsh -> gmi translator. Reads the subset of linuxcncrsh 'set'
# commands used by the tool tests on stdin and drives the machine via the gmi
# REST client. Replaces piping the command stream into `nc localhost 5007`.
import sys
import gmi
from gmi.constants import *

c = gmi.Command()
s = gmi.Stat()

for raw in sys.stdin:
    cmd = raw.strip()
    if not cmd:
        continue
    low = cmd.lower()
    if low.startswith('hello') or low.startswith('set enable') or low.startswith('set wait received'):
        continue
    elif low == 'set estop off':
        c.state(STATE_ESTOP_RESET)
    elif low == 'set estop on':
        c.state(STATE_ESTOP)
    elif low == 'set machine on':
        c.state(STATE_ON)
    elif low == 'set machine off':
        c.state(STATE_OFF)
    elif low == 'set mode mdi':
        c.mode(MODE_MDI)
    elif low == 'set mode manual':
        c.mode(MODE_MANUAL)
    elif low == 'set mode auto':
        c.mode(MODE_AUTO)
    elif low == 'set wait done':
        c.wait_complete(30)
    elif low.startswith('set mdi '):
        c.mdi(cmd[len('set mdi '):])
        c.wait_complete(30)
    elif low.startswith('set home '):
        c.home(int(cmd.split()[-1]))
    elif low.startswith('set teleop_enable '):
        c.teleop_enable(cmd.split()[-1] in ('1', 'true', 'on'))
    elif low == 'shutdown':
        break
    # everything else (get ..., etc.) is ignored
