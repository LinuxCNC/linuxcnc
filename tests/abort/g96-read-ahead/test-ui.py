#!/usr/bin/env python3

import linuxcnc
import linuxcnc_util
import time
import sys


def wait_for(condition, what, timeout=30):
    start = time.time()
    while time.time() - start < timeout:
        s.poll()
        if condition():
            return
        time.sleep(0.005)
    print('FAIL: timed out waiting for %s' % what)
    sys.exit(1)


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

# The move on line 3 takes 10 s, the interpreter reads past it at once.
wait_for(lambda: s.motion_line == 3 and s.read_line >= 4, 'the G96 move with read-ahead past it')
if s.gcodes[13] != 960:
    print('FAIL: not in G96 during the move: %d' % s.gcodes[13])
    sys.exit(1)

c.abort()
c.wait_complete()
wait_for(lambda: s.interp_state == linuxcnc.INTERP_IDLE, 'the interpreter to stop')
wait_for(lambda: s.spindle[0]['speed'] == 0, 'the spindle to stop')

# Anything queued behind the abort would turn the spindle back on.
start = time.time()
while time.time() - start < 1:
    s.poll()
    if s.spindle[0]['speed'] != 0:
        print('FAIL: the spindle turned back on: %f' % s.spindle[0]['speed'])
        sys.exit(1)
    time.sleep(0.005)

print('after the abort: plane %d, spindle mode %d' % (s.gcodes[3], s.gcodes[13]))
if s.gcodes[3] != 180:
    print('FAIL: the abort left the plane the interpreter read ahead')
    sys.exit(1)

while True:
    msg = e.poll()
    if not msg:
        break
    print('FAIL: error channel: %s' % (msg[1],))
    sys.exit(1)
sys.exit(0)
