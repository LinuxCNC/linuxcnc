#!/usr/bin/env python3

import linuxcnc
import time, sys, os


def process_samples(z_lev, expected_max_x):
    # Read motion-samples.log for samples at the specified Z level and
    # find the maximum X
    res = 0
    f = open('motion-samples.log','r')
    max_x = 0.0
    max_y = 0.0
    samples = 0
    for line in f:
        try:
            i, x, y, z, xvel, yvel, zvel, xacc, yacc, zacc = [
                float(n) for n in line.split(' ')[:10]]
        except ValueError:
            # halsampler is still writing, so an incomplete line may be read
            break
        if z < z_lev:
            # Ignore travel between Z levels
            continue
        if z > z_lev:
            break
        if x > max_x:
            # Record new maximum
            max_x = x
            max_y = y
        samples += 1
    f.close()
    print("z=%.1f; max_x = %.6f; max_y = %.6f; samples = %d" % (
        z_lev, max_x, max_y, samples))
    if abs(max_x - expected_max_x) > 0.1:
        print("*** ERROR max X%.3f != expected X%.3f" % (
            max_x, expected_max_x))
        res += 1
    return res

def check_status(msg, expected_mode, expected_p, expected_q):
    res = 0
    # Read status and print control mode and tolerances
    s.poll()
    control_mode = None
    for i in s.gcodes:
        if i in (610, 611, 640):
            control_mode = i
    if len(s.settings) == 3:
        # Pre-state-tags fix
        print("%s:  Control mode = %d" % (msg, control_mode))
        p, q = (expected_p, expected_q)  # Fake valid results
    else:
        # After state-tags fix
        print("%s:  Control mode = %d; tolerances = P%.3f Q%.3f" % (
            msg, control_mode, s.settings[3], s.settings[4]))
        p, q = (s.settings[3], s.settings[4])
    if control_mode != expected_mode:
        print("*** ERROR control mode %d != expected %d" % (
            control_mode, expected_mode))
        res += 1
    if expected_p is not None and abs(p - expected_p) > 0.001:
        print("*** ERROR blend tolerance P%.3f != expected P%.3f" % (
            p, expected_p))
        res += 1
    if expected_q is not None and abs(q - expected_q) > 0.001:
        print("*** ERROR naive CAM tolerance Q%.3f != expected Q%.3f" % (
            q, expected_q))
        res += 1
    return res

def run_and_abort(msg, z_lev, expected_max_x,
                  expected_mode, expected_p, expected_q, *init_cmds):
    res = 0
    # Move to specified Z level and run init commands; run test.ngc
    # and abort; print status and max X values during run

    # Run init commands
    c.mode(linuxcnc.MODE_MDI)
    c.mdi('F1200 G0X0Y0Z%.1f' % z_lev)
    c.wait_complete()
    for cmd in init_cmds:
        print("Running command '%s'" % cmd)
        c.mdi(cmd)
        c.wait_complete()
    res += check_status(
        '%s post-command' % msg, expected_mode, expected_p, expected_q)

    # Run program
    print("Running program 'test.ngc'")
    c.mode(linuxcnc.MODE_AUTO)
    c.program_open("test.ngc")
    c.auto(linuxcnc.AUTO_RUN, 1)

    # Wait for things to get underway, then abort
    c.wait_complete()
    # Let zig-zags run to a certain point
    while s.position[1] < 1.0:
        time.sleep(0.1)
        s.poll()
    res += check_status(
        '%s pre-abort' % msg, expected_mode, expected_p, expected_q)
    c.abort()
    c.wait_complete()

    # Check settings
    res += check_status(
        '%s post-abort' % msg, expected_mode, expected_p, expected_q)

    # Print max
    res += process_samples(z_lev, expected_max_x)
    print()

    return res

#
# INIT
#
c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()

#
# TESTS
#
# Set up with various G61/G61.1/G64 commands, run test.ngc, and check
# 1) whether stat buffer shows correct motion mode;
# 2) whether tolerance settings produce expected motion;
# 3) for G64, whether tolerance settings are correct in stat buffer
res = 0
# Z=0:  exact path mode:  X up to 5"
res += run_and_abort('G61',          0.0, 5.0, 610, None, None, 'G61')
res += run_and_abort('G61 redo',     0.5, 5.0, 610, None, None)
# Z=1:  exact stop mode:  X up to 5"
res += run_and_abort('G61.1',        1.0, 5.0, 611, None, None, 'G61.1')
res += run_and_abort('G61.1 redo',   1.5, 5.0, 611, None, None)
# Z=2:  path blending, 0.5" blend tolerance:  X up to 4.5"
res += run_and_abort('G64P0.5',      2.0, 4.5, 640,  0.5,  0.0, 'G64P0.5Q0')
res += run_and_abort('G64P0.5 redo', 2.5, 4.5, 640,  0.5,  0.0)
# Z=3:  path blending, best possible speed:  X up to 3.725"
res += run_and_abort('G64',          3.0, 3.7, 640,  0.0,  0.0, 'G64')
res += run_and_abort('G64 redo',     3.5, 3.7, 640,  0.0,  0.0)
# Z=4:  path blending, 6" naive cam tolerance:  X always 0" (zig-zags collapse)
res += run_and_abort('G64P0Q6',      4.0, 0.0, 640,  0.0,  6.0, 'G64Q6')
res += run_and_abort('G64P0Q6 redo', 4.5, 0.0, 640,  0.0,  6.0)

#
# CLEANUP
#
if res == 0:  # Leave samples in case of error
    os.unlink('motion-samples.log')
os.unlink('sim.var')
os.unlink('sim.var.bak')
print("Exiting with %d errors" % res)
sys.exit(res)

