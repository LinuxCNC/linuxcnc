#!/usr/bin/env python3

import linuxcnc
import linuxcnc_util
import time
import sys

failures = 0


def wait_for(condition, what, timeout=60):
    start = time.time()
    while time.time() - start < timeout:
        s.poll()
        if condition():
            return True
        time.sleep(0.005)
    print('FAIL: timed out waiting for %s' % what)
    return False


def offset_is(expected, what):
    global failures
    if not wait_for(lambda: abs(s.tool_offset[2] - expected) < 1e-6, what, timeout=10):
        print('FAIL: %s: offset %f, expected %f' % (what, s.tool_offset[2], expected))
        failures += 1


l = linuxcnc_util.LinuxCNC()
c, s = l.command, l.status
l.wait_for_linuxcnc_startup()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_AUTO)
c.wait_complete()
c.program_open('test.ngc')
c.wait_complete()
c.auto(linuxcnc.AUTO_RUN, 0)
wait_for(lambda: s.interp_state != linuxcnc.INTERP_IDLE, 'the program to start', timeout=10)
wait_for(lambda: s.interp_state == linuxcnc.INTERP_IDLE, 'the program to end')
offset_is(13, 'G10 L1 after the remapped M6')

time.sleep(0.2)
while True:
    msg = e.poll()
    if not msg:
        break
    print('FAIL: error channel: %s' % (msg[1],))
    failures += 1
sys.exit(1 if failures else 0)
