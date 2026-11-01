#!/usr/bin/env python3
# M62-M68 with an output number that motion does not have (num_dio=4,
# num_aio=4) must be refused with an error, and a program containing
# such a line must stop there.

import linuxcnc
import hal
import os
import sys
import time

failures = []


def fail(msg):
    print("FAIL " + msg)
    failures.append(msg)


def errors():
    out = []
    time.sleep(0.2)
    while True:
        error = e.poll()
        if not error:
            return out
        print("  linuxcnc error: %s" % error[1].strip())
        out.append(error[1])


def wait_idle(timeout=10.0):
    start = time.time()
    while time.time() - start < timeout:
        s.poll()
        if s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0:
            return True
        time.sleep(0.05)
    return False


def dout(n):
    return hal.get_value("motion.digital-out-%02d" % n)


def aout(n):
    return hal.get_value("motion.analog-out-%02d" % n)


def mdi(cmd):
    print("MDI %s" % cmd)
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete()
    c.mdi(cmd)
    c.wait_complete()
    wait_idle()
    return errors()


def run_program(lines):
    print("program: %s" % " / ".join(lines))
    with open("test.ngc", "w") as f:
        f.write("\n".join(lines + ["M2", ""]))
    c.mode(linuxcnc.MODE_AUTO)
    c.wait_complete()
    c.program_open("test.ngc")
    c.auto(linuxcnc.AUTO_RUN, 0)
    c.wait_complete()
    if not wait_idle():
        fail("program did not stop")
    return errors()


def expect_error(errs, text, label):
    if not any(text in m for m in errs):
        fail("%s: expected an error containing '%s', got %s" % (label, text, errs))


def expect_no_error(errs, label):
    if errs:
        fail("%s: unexpected error %s" % (label, errs))


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete()
errors()

# existing outputs still work
expect_no_error(mdi("M64 P3"), "M64 P3")
if not dout(3):
    fail("M64 P3 did not set motion.digital-out-03")
expect_no_error(mdi("M68 E3 Q2.5"), "M68 E3")
if aout(3) != 2.5:
    fail("M68 E3 Q2.5 set motion.analog-out-03 to %s" % aout(3))

# outputs past num_dio / num_aio are refused
expect_error(mdi("M64 P4"), "no such digital output", "M64 P4")
expect_error(mdi("M65 P10"), "no such digital output", "M65 P10")
expect_error(mdi("M68 E4 Q1"), "no such analog output", "M68 E4")

# a program stops exactly at the bad line: the line before it runs,
# nothing after it does
for bad in ("M64 P10", "M62 P10", "M67 E7 Q1"):
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete()
    mdi("M65 P1")
    mdi("M65 P2")
    mdi("G0 X0")
    errs = run_program(["M64 P1", bad, "M64 P2", "G1 X1 F600"])
    expect_error(errs, "no such", bad)
    s.poll()
    if not dout(1):
        fail("%s: the line before it did not run (motion.digital-out-01 clear)" % bad)
    if dout(2):
        fail("%s: the line after it ran (motion.digital-out-02 set)" % bad)
    if abs(s.actual_position[0]) > 1e-6:
        fail("%s: the move after it ran (X=%.4f)" % (bad, s.actual_position[0]))

c.state(linuxcnc.STATE_OFF)
c.wait_complete()
for f in ("test.ngc", "sim.var", "sim.var.bak"):
    if os.path.exists(f):
        os.unlink(f)
print("%d failures" % len(failures))
sys.exit(1 if failures else 0)
