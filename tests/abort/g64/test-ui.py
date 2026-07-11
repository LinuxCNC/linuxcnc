#!/usr/bin/env python3

# Ported to the gomc REST/WS API: uses the `gmi` client instead of the removed
# NML `linuxcnc` module. Motion samples are captured by halsampler (started in
# test.sh) into motion-samples.log.

import gmi
from gmi.constants import *
import time, sys, os

SETTLE = 0.4


def process_samples(z_lev, expected_max_x):
    res = 0
    f = open('motion-samples.log', 'r')
    max_x = 0.0
    max_y = 0.0
    samples = 0
    for line in f:
        try:
            i, x, y, z, xvel, yvel, zvel, xacc, yacc, zacc = [
                float(n) for n in line.split(' ')[:10]]
        except ValueError:
            break
        if z < z_lev:
            continue
        if z > z_lev:
            break
        if x > max_x:
            max_x = x
            max_y = y
        samples += 1
    f.close()
    print("z=%.1f; max_x = %.6f; max_y = %.6f; samples = %d" % (
        z_lev, max_x, max_y, samples))
    if abs(max_x - expected_max_x) > 0.1:
        print("*** ERROR max X%.3f != expected X%.3f" % (max_x, expected_max_x))
        res += 1
    return res


def check_status(msg, expected_mode, expected_p, expected_q):
    res = 0
    s.poll()
    control_mode = None
    for i in s.gcodes:
        if i in (610, 611, 640):
            control_mode = i
    if len(s.settings) == 3:
        print("%s:  Control mode = %d" % (msg, control_mode))
        p, q = (expected_p, expected_q)
    else:
        print("%s:  Control mode = %d; tolerances = P%.3f Q%.3f" % (
            msg, control_mode, s.settings[3], s.settings[4]))
        p, q = (s.settings[3], s.settings[4])
    if control_mode != expected_mode:
        print("*** ERROR control mode %d != expected %d" % (control_mode, expected_mode))
        res += 1
    if expected_p is not None and abs(p - expected_p) > 0.001:
        print("*** ERROR blend tolerance P%.3f != expected P%.3f" % (p, expected_p))
        res += 1
    if expected_q is not None and abs(q - expected_q) > 0.001:
        print("*** ERROR naive CAM tolerance Q%.3f != expected Q%.3f" % (q, expected_q))
        res += 1
    return res


def run_and_abort(msg, z_lev, expected_max_x, expected_mode, expected_p, expected_q, *init_cmds):
    res = 0
    c.mode(MODE_MDI)
    c.mdi('F1200 G0X0Y0Z%.1f' % z_lev)
    c.wait_complete()
    for cmd in init_cmds:
        print("Running command '%s'" % cmd)
        c.mdi(cmd)
        c.wait_complete()
    time.sleep(SETTLE)
    res += check_status('%s post-command' % msg, expected_mode, expected_p, expected_q)

    print("Running program 'test.ngc'")
    c.mode(MODE_AUTO)
    c.program_open("test.ngc")
    c.auto(AUTO_RUN, 1)

    c.wait_complete()
    s.poll()
    while s.position[1] < 1.0:
        time.sleep(0.1)
        s.poll()
    res += check_status('%s pre-abort' % msg, expected_mode, expected_p, expected_q)
    c.abort()
    c.wait_complete()
    time.sleep(SETTLE)

    res += check_status('%s post-abort' % msg, expected_mode, expected_p, expected_q)
    time.sleep(SETTLE)
    res += process_samples(z_lev, expected_max_x)
    print()
    return res


c = gmi.Command()
s = gmi.Stat()
e = gmi.ErrorChannel()

c.state(STATE_ESTOP_RESET)
c.state(STATE_ON)
c.home(0)
c.home(1)
c.home(2)
c.wait_complete()
time.sleep(SETTLE)

res = 0
res += run_and_abort('G61',          0.0, 5.0, 610, None, None, 'G61')
res += run_and_abort('G61 redo',     0.5, 5.0, 610, None, None)
res += run_and_abort('G61.1',        1.0, 5.0, 611, None, None, 'G61.1')
res += run_and_abort('G61.1 redo',   1.5, 5.0, 611, None, None)
res += run_and_abort('G64P0.5',      2.0, 4.5, 640,  0.5,  0.0, 'G64P0.5Q0')
res += run_and_abort('G64P0.5 redo', 2.5, 4.5, 640,  0.5,  0.0)
res += run_and_abort('G64',          3.0, 3.7, 640,  0.0,  0.0, 'G64')
res += run_and_abort('G64 redo',     3.5, 3.7, 640,  0.0,  0.0)
res += run_and_abort('G64P0Q6',      4.0, 0.0, 640,  0.0,  6.0, 'G64Q6')
res += run_and_abort('G64P0Q6 redo', 4.5, 0.0, 640,  0.0,  6.0)

if res == 0:
    os.unlink('motion-samples.log')
for f in ('sim.var', 'sim.var.bak'):
    if os.path.exists(f):
        os.unlink(f)
print("Exiting with %d errors" % res)
sys.exit(res)
