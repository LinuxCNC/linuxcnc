#!/usr/bin/env python3

import linuxcnc
import linuxcnc_util
import re
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


def mdi(line):
    c.mdi(line)
    c.wait_complete()
    wait_for(lambda: s.interp_state == linuxcnc.INTERP_IDLE, 'MDI %s' % line)


def offset_is(expected, what):
    global failures
    if not wait_for(lambda: abs(s.tool_offset[2] - expected) < 1e-6, what, timeout=10):
        print('FAIL: %s: offset %f, expected %f' % (what, s.tool_offset[2], expected))
        failures += 1


def set_table_z(toolno, z):
    with open('simpockets.tbl') as f:
        text = f.read()
    text = re.sub(r'^(T%d\b.*?\bZ)[-+.0-9e]+' % toolno, r'\g<1>%f' % z, text, flags=re.M)
    with open('simpockets.tbl', 'w') as f:
        f.write(text)
    c.load_tool_table()
    c.wait_complete()


l = linuxcnc_util.LinuxCNC()
c, s = l.command, l.status
l.wait_for_linuxcnc_startup()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()

mdi('G20 G90')
mdi('M61 Q1 G43')
offset_is(1, 'M61 with G43')
mdi('M61 Q2')
offset_is(2, 'M61 of another tool')
mdi('G0 X0.1')
offset_is(2, 'the block after M61')
mdi('M61 Q0')
offset_is(0, 'M61 Q0')
mdi('G0 X0.2')
offset_is(0, 'the block after M61 Q0')

mdi('T1 M6 G43')
offset_is(1, 'M6 with G43')
mdi('T2 M6')
offset_is(2, 'M6 of another tool')
mdi('G0 X0.3')
offset_is(2, 'the block after M6')

mdi('G10 L1 P2 Z5')
mdi('G0 X0.4')
offset_is(5, 'G10 L1 of the loaded tool')
mdi('G10 L1 P1 Z7')
mdi('G0 X0.5')
offset_is(5, 'G10 L1 of another tool')

set_table_z(2, 6)
mdi('G0 X0.6')
offset_is(6, 'a reloaded tool table')

mdi('G0 Z1')
wait_for(lambda: s.current_vel == 0, 'the Z move to end')
z = s.position[2]
mdi('G21')
mdi('G0 X20')
offset_is(6, 'G21')
mdi('G20')
mdi('G0 X0.7')
offset_is(6, 'G20')
s.poll()
if abs(s.position[2] - z) > 1e-6:
    print('FAIL: a change of units moved Z from %f to %f' % (z, s.position[2]))
    failures += 1

mdi('G43 H3')
mdi('G0 X0.8')
offset_is(3, 'G43 H of another tool')
mdi('G49')
mdi('G0 X0.9')
offset_is(0, 'G49')
mdi('T1 M6')
mdi('G0 X1')
offset_is(0, 'M6 after G49')

mdi('G43')
offset_is(7, 'G43 of the loaded tool')
mdi('T4 M6')
mdi('G0 X1.1')
offset_is(0, 'M6 of a zero length tool')
mdi('T2 M6')
mdi('G0 X1.2')
offset_is(6, 'M6 after a zero length tool')
mdi('T0 M6')
mdi('G0 X1.3')
offset_is(0, 'T0 M6')
mdi('T2 M6')
mdi('G0 X1.4')
offset_is(0, 'M6 after T0 M6')
mdi('T1 M6')

c.mode(linuxcnc.MODE_AUTO)
c.wait_complete()
c.program_open('test.ngc')
c.wait_complete()
c.auto(linuxcnc.AUTO_RUN, 0)
wait_for(lambda: s.interp_state != linuxcnc.INTERP_IDLE, 'the program to start', timeout=10)
wait_for(lambda: s.interp_state == linuxcnc.INTERP_IDLE, 'the program to end')
offset_is(9, 'G10 L1 in a program')

c.program_open('lathe.ngc')
c.wait_complete()
c.auto(linuxcnc.AUTO_RUN, 0)
wait_for(lambda: s.interp_state != linuxcnc.INTERP_IDLE, 'the lathe program to start', timeout=10)
wait_for(lambda: s.interp_state == linuxcnc.INTERP_IDLE, 'the lathe program to end')
offset_is(2, 'a lathe program with G42')

time.sleep(0.2)
while True:
    msg = e.poll()
    if not msg:
        break
    print('FAIL: error channel: %s' % (msg[1],))
    failures += 1
sys.exit(1 if failures else 0)
