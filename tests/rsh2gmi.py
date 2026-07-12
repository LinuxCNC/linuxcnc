#!/usr/bin/env python3
# Minimal linuxcncrsh -> gmi translator. Reads the subset of linuxcncrsh 'set'
# commands used by the tool tests on stdin and drives the machine via the gmi
# REST client. Replaces piping the command stream into `nc localhost 5007`.
import sys
import time
import gmi
from gmi.constants import *

c = gmi.Command()
s = gmi.Stat()


def drain_mdi(timeout=30.0):
    """Wait for an MDI command to fully drain before the next is sent. gmi
    wait_complete lacks serial-number tracking, so back-to-back MDIs would
    otherwise overrun the MDI queue ("MDI queue full"). c.mdi() is a synchronous
    POST (the command is registered before it returns), so we only need to wait
    for the queue to empty and the interpreter to return to idle — which, since
    the sequencer blocks on any M-code handler, also means the handler finished."""
    start = time.time()
    while time.time() - start < timeout:
        s.poll()
        if s.queued_mdi_commands == 0 and s.interp_state == INTERP_IDLE:
            return
        time.sleep(0.005)
    raise SystemExit(
        "rsh2gmi: MDI did not drain within %gs (queued=%d interp_state=%d) — "
        "treating a hung interpreter as failure, not success"
        % (timeout, s.queued_mdi_commands, s.interp_state))


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
        # wait_complete returns 1 (RCS_DONE), 3 (RCS_ERROR), or -1 (timeout).
        # A timeout means the interpreter hung — fail loudly rather than silently
        # continue (which would read a hung run as success). RCS_ERROR is NOT fatal:
        # these tool tests deliberately issue commands that error (e.g. G10 L1 P0)
        # and introspect the resulting state afterwards.
        if c.wait_complete(30) == -1:
            raise SystemExit("rsh2gmi: wait_complete timed out (interpreter hung)")
    elif low.startswith('set mdi '):
        c.mdi(cmd[len('set mdi '):])
        time.sleep(0.02)  # let the command register before polling for drain
        drain_mdi(30)
    elif low.startswith('set home '):
        c.home(int(cmd.split()[-1]))
    elif low.startswith('set teleop_enable '):
        c.teleop_enable(cmd.split()[-1] in ('1', 'true', 'on'))
    elif low == 'shutdown':
        break
    # everything else (get ..., etc.) is ignored
