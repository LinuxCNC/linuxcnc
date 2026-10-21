#!/usr/bin/env python3

import linuxcnc
import linuxcnc_util
import time
import sys


l = linuxcnc_util.LinuxCNC()
c, s = l.command, l.status
l.wait_for_linuxcnc_startup()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_AUTO)
c.wait_complete()
c.program_open('test.ngc')
c.wait_complete()
s.poll()
if s.task_mode != linuxcnc.MODE_AUTO or not s.file.endswith('test.ngc'):
    print('FAIL: not in AUTO with test.ngc open (mode %d, file %r)' % (s.task_mode, s.file))
    sys.exit(1)
e = linuxcnc.error_channel()
c.auto(linuxcnc.AUTO_RUN, 0)

# Wait for the program to start (no fixed delay: a loaded machine may take a while).
start = time.time()
while time.time() - start < 10:
    s.poll()
    if s.interp_state != linuxcnc.INTERP_IDLE:
        break
    time.sleep(0.005)
else:
    print('FAIL: the program did not start within 10 s')
    sys.exit(1)

# While the G21 move executes, gcodes[5] (from the executing move) says G21.
# program_units must agree: before the fix it already showed the G20 that the
# interpreter had read ahead.
in_g21 = mismatch = 0
start = time.time()
while time.time() - start < 30:
    s.poll()
    if s.interp_state == linuxcnc.INTERP_IDLE:
        break
    if s.gcodes[5] == 210 and s.interp_state != linuxcnc.INTERP_IDLE:
        in_g21 += 1
        if s.program_units != 2:  # CANON_UNITS_MM (no Python constant for it)
            mismatch += 1
    time.sleep(0.005)

s.poll()
while True:
    msg = e.poll()
    if not msg:
        break
    print('error channel: %s' % (msg[1],))
if s.interp_state != linuxcnc.INTERP_IDLE:
    print('FAIL: the program did not finish within 30 s')
    sys.exit(1)
print('samples while G21 executed: %d, program_units not mm in them: %d, end gcodes[5]=%d program_units=%d'
      % (in_g21, mismatch, s.gcodes[5], s.program_units))
if in_g21 == 0:
    print('FAIL: the G21 move was not seen executing')
    sys.exit(1)
if mismatch:
    print('FAIL: program_units disagreed with gcodes while the G21 move executed')
    sys.exit(1)
if (s.gcodes[5], s.program_units) != (200, 1):
    print('FAIL: after the program, G20 should be active in both')
    sys.exit(1)
sys.exit(0)
