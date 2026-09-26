#!/usr/bin/env python3
# A LINEAR and V ANGULAR on a mm machine: units, feed and wrap through
# task, canon and motion.

import linuxcnc
import sys
import time

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

A, V = 3, 7


def fail(msg):
    print("FAIL: " + msg)
    c.state(linuxcnc.STATE_ESTOP)
    sys.exit(1)


def wait_ready(timeout=20.0):
    end = time.time() + timeout
    while time.time() < end:
        s.poll()
        if s.task_state == linuxcnc.STATE_ESTOP and s.interp_state == linuxcnc.INTERP_IDLE \
           and s.axis_mask != 0 and s.linear_units != 0.0:
            return
        time.sleep(0.1)
    fail("linuxcnc did not come up")


def errors():
    err = e.poll()
    if err:
        fail("error: %s" % err[1])


def mdi(cmd, timeout=20.0):
    """Run one MDI line to the end; return the largest current_vel seen
    and how long the machine moved."""
    c.mdi(cmd)
    peak = 0.0
    first = last = None
    end = time.time() + timeout
    time.sleep(0.05)
    while time.time() < end:
        s.poll()
        errors()
        if s.current_vel > 1e-6:
            now = time.time()
            first = first or now
            last = now
            peak = max(peak, s.current_vel)
        if s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0 and s.inpos \
           and s.state == linuxcnc.RCS_DONE:
            break
        time.sleep(0.001)
    else:
        fail("%s did not finish" % cmd)
    errors()
    return peak, (last - first) if first else 0.0


def near(what, got, want, tol):
    print("%s: %.6g (want %.6g)" % (what, got, want))
    if abs(got - want) > tol:
        fail("%s is %.6g, not %.6g" % (what, got, want))


wait_ready()
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()

# G20: an A word is inches, a V word degrees
mdi("G20 G90 G94 G0 A1 V10")
s.poll()
near("A after G20 A1 (mm)", s.position[A], 25.4, 1e-6)
near("V after G20 V10 (deg)", s.position[V], 10.0, 1e-6)

# V alone: F is degrees per minute, G20 or not
peak, _ = mdi("G20 G1 V90 F3600")
near("V alone at F3600, deg/s", peak, 60.0, 0.6)

# X with A: F along X, the feed axis (F comes before G21 in a block, so
# G21 goes on its own line)
mdi("G21")
peak, _ = mdi("G1 X10 A35.4 F600")
near("X with A at F600, mm/s", peak, 10.0, 0.1)

# A with V: F along A, the linear axis that moves; motion measures the
# line along V, 8 times longer, so it runs at 8 times the rate and the
# move still takes the time of 10 mm at 10 mm/s
peak, secs = mdi("G1 A45.4 V170 F600")
near("A with V at F600, motion rate", peak, 80.0, 0.8)
near("A with V at F600, seconds", secs, 1.0, 0.1)
s.poll()
near("A after the move", s.position[A], 45.4, 1e-6)
near("V after the move", s.position[V], 170.0, 1e-6)

# V wraps: 350 then 10 goes on to 370
mdi("G0 V350")
mdi("G0 V10")
s.poll()
near("V wrapped", s.position[V], 370.0, 1e-6)

c.state(linuxcnc.STATE_ESTOP)
print("PASS")
sys.exit(0)
