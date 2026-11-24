#!/usr/bin/env python3

import linuxcnc
import linuxcnc_util
import time
import sys


l = linuxcnc_util.LinuxCNC()
c, s = l.command, l.status
l.wait_for_linuxcnc_startup()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()
c.mdi('G55 G18')
c.wait_complete()
l.wait_for_interp_state(linuxcnc.INTERP_IDLE)


def modes():
    s.poll()
    return s.gcodes[3], s.gcodes[8]  # plane, coordinate system


if modes() != (180, 550):
    print('FAIL: MDI did not set G55 G18: %s' % (modes(),))
    sys.exit(1)

c.mode(linuxcnc.MODE_AUTO)
c.wait_complete()
c.program_open('test.ngc')
c.wait_complete()
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

errors = []
start = time.time()
while time.time() - start < 30:
    s.poll()
    msg = e.poll()
    if msg:
        errors.append(msg[1])
    if s.interp_state == linuxcnc.INTERP_IDLE:
        break
    time.sleep(0.005)
else:
    print('FAIL: the program did not stop within 30 s')
    sys.exit(1)

time.sleep(0.5)
while True:
    msg = e.poll()
    if not msg:
        break
    errors.append(msg[1])
for m in errors:
    print('error channel: %s' % (m,))

if not any('would exceed' in m for m in errors):
    print('FAIL: motion did not reject the move beyond the limit')
    sys.exit(1)

print('after the error: plane %d, coordinate system %d' % modes())
if modes() != (180, 550):
    print('FAIL: the error left the modes the interpreter read ahead')
    sys.exit(1)
sys.exit(0)
