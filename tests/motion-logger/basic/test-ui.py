#!/usr/bin/env python3

# Ported to the gomc REST/WS API (`gmi` client, not the removed NML `linuxcnc`
# module). The motion-logger is now an interceptor cmod sitting between milltask
# and the *real* motmod, so this drives a real motion config.
#
# Two differences from the classic (mock-motion) test:
#  - No `reopen-log` HAL pin (the interceptor runs no realtime function): each
#    sub-test's slice of out.motion-logger is captured by byte offset. The cmod
#    fflush()es every line, so once a program is idle the delta is complete.
#  - Real motion takes real time, and the programmed feeds (F1 etc.) would run
#    for minutes. We apply a large feed/rapid override so motion completes in
#    sub-seconds; the override is a separate scale command and does NOT change
#    the logged SET_LINE velocities. We then wait for INTERP_IDLE (true program
#    completion) before capturing, so the full command stream is always present.
#
# reset.ngc runs before every program to reset Task/Canon state; because it
# issues a real move whose motion id increments across the session, its `id=`
# field is normalized before comparison (the ids are session-incidental).

import gmi
from gmi.constants import *

import sys
import time
import glob
import re
import difflib

c = gmi.Command()
s = gmi.Stat()
e = gmi.ErrorChannel()

retval = 0
log_offset = 0

LOG = "out.motion-logger"


def wait_idle(timeout=30.0):
    '''Wait for an AUTO_RUN to finish. auto() returns as soon as the command is
    accepted (interp still shows IDLE for a moment), so first wait for the run
    to start (interp leaves IDLE), then wait for it to return to IDLE.'''
    start = time.time()
    while time.time() - start < 5.0:
        s.poll()
        if s.interp_state != INTERP_IDLE:
            break
        time.sleep(0.02)
    while time.time() - start < timeout:
        s.poll()
        if s.interp_state == INTERP_IDLE:
            return
        time.sleep(0.02)
    raise SystemExit("timeout waiting for interp idle")


def capture(name, compare=True):
    '''Capture the log slice produced since the last call. When compare is set,
    diff it against expected.<name>; otherwise just advance the offset (used for
    the reset program, whose stream is position-dependent under real motion).'''
    global log_offset, retval
    time.sleep(0.2)  # settle any trailing flushed lines onto disk
    with open(LOG, "rb") as f:
        f.seek(log_offset)
        chunk = f.read().decode()
        log_offset = f.tell()
    if not compare:
        return
    with open("result.%s" % name, "w") as f:
        f.write(chunk)
    expected = open("expected.%s" % name).read()
    if expected == chunk:
        print("sub-test %s ok" % name)
    else:
        print("unexpected output in logfile '%s'" % name)
        sys.stdout.writelines(difflib.unified_diff(
            expected.splitlines(True), chunk.splitlines(True),
            "expected.%s" % name, "result.%s" % name))
        retval = 1
    sys.stdout.flush()


#
# Come out of E-stop, turn the machine on, switch to Auto mode. The config sets
# [TRAJ]NO_FORCE_HOMING, so no homing is needed to run programs.
#

c.state(STATE_ESTOP_RESET)
c.state(STATE_ON)
c.wait_complete()
c.mode(MODE_AUTO)

# Make real motion complete quickly without altering the logged command stream.
# The programmed feeds (F0.1 etc.) would otherwise run for many minutes; a large
# feed override scales them up to the machine velocity cap. [DISPLAY]
# MAX_FEED_OVERRIDE is raised to permit this.
c.feedrate(10000.0)
c.rapidrate(1.0)
c.wait_complete()

capture("builtin-startup")


#
# Run each .ngc test file (reset.ngc is the between-tests reset program).
#

for ngc in sorted(glob.glob("*.ngc")):
    if ngc == "reset.ngc":
        continue
    basename = re.match(r"(.*)\.ngc", ngc).group(1)

    c.program_open("reset.ngc")
    c.auto(AUTO_RUN)
    wait_idle()
    capture("reset", compare=False)

    c.program_open(ngc)
    c.auto(AUTO_RUN)
    wait_idle()
    capture(basename)


sys.exit(retval)
