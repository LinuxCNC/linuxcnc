#!/usr/bin/env python3

"""
Bare G28.2 (no P word) on a config that sets HOME_SEQUENCE.

The other tests here only exercise G28.2 Pn. Bare G28.2 goes through a
different motion path -- do_home_joint(-1) -> do_home_all() -> the
HOME_SEQUENCE state machine -- and needs HOME_SEQUENCE set, exactly like
the GUI Home All button. This config has three single-joint sequence
groups (0, 1, 2).

Checks:
 1. Bare G28.2 issued from MDI homes every joint and completes without a
    task error.
 2. The task-level mode is not disturbed by the FREE-mode dip.
 3. Homing walks the sequence groups in order (joint 0, then 1, then 2) --
    and the completion poll does not trip on the gap between groups, where
    all per-joint .homing flags read false while the machine is still
    homing. A false "did not complete" would abort the MDI command and
    leave the machine unhomed in FREE.
 4. Bare G28.2 from a program (AUTO) works the same way.
"""

import linuxcnc
import hal

import sys
import time

h = hal.component("python-ui")
h.ready()

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


def poll():
    s.poll()


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def drain_errors():
    msgs = []
    while True:
        err = e.poll()
        if not err:
            return msgs
        msgs.append(err[1])


def wait_idle(timeout=15.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        poll()
        if s.exec_state == linuxcnc.EXEC_DONE and s.interp_state == linuxcnc.INTERP_IDLE:
            return True
        time.sleep(0.01)
    return False


def wait_homed(expected, timeout=15.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        poll()
        if list(s.homed[:3]) == expected:
            return True
        time.sleep(0.01)
    return False


c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
poll()
if list(s.homed[:3]) != [0, 0, 0]:
    fail("machine came up homed: {}".format(list(s.homed[:3])))

c.mode(linuxcnc.MODE_MDI)
time.sleep(0.2)
poll()
mode_before = s.task_mode
drain_errors()

# --- 1: bare G28.2 from MDI homes everything -------------------------------
c.mdi("G28.2")

# Watch the homed flags flip on in sequence order while the cycle runs.
order = []
t0 = time.time()
while time.time() - t0 < 15.0:
    poll()
    for j in range(3):
        if s.homed[j] and j not in order:
            order.append(j)
    if list(s.homed[:3]) == [1, 1, 1]:
        break
    time.sleep(0.005)

if list(s.homed[:3]) != [1, 1, 1]:
    fail("bare G28.2 did not home all joints: homed={}".format(list(s.homed[:3])))
if order != [0, 1, 2]:
    fail("joints did not home in HOME_SEQUENCE order: saw {}".format(order))

if not wait_idle():
    fail("bare G28.2 (MDI) did not return to idle/DONE")

errs = drain_errors()
if any("did not complete" in m or "did not start" in m for m in errs):
    fail("bare G28.2 reported a spurious homing failure: {!r}".format(errs))
poll()
if s.exec_state == linuxcnc.EXEC_ERROR:
    fail("bare G28.2 left task in EXEC_ERROR (errors={!r})".format(errs))
if s.task_mode != mode_before:
    fail("task_mode changed across bare G28.2: {} -> {}".format(mode_before, s.task_mode))
print("PASS: bare G28.2 from MDI homes every joint in sequence order, mode untouched")

# --- 2: and again from a program -----------------------------------------
c.mode(linuxcnc.MODE_MANUAL)
c.teleop_enable(0)          # to joint mode, so the unhome is allowed
time.sleep(0.3)
c.unhome(-1)
if not wait_homed([0, 0, 0]):
    fail("could not unhome for the AUTO re-test: {}".format(list(s.homed[:3])))

c.mode(linuxcnc.MODE_AUTO)
time.sleep(0.2)
c.program_open("test.ngc")
time.sleep(0.2)
drain_errors()
c.auto(linuxcnc.AUTO_RUN, 0)

if not wait_homed([1, 1, 1]):
    fail("bare G28.2 from a program did not home all joints: {}".format(list(s.homed[:3])))
if not wait_idle():
    fail("program with bare G28.2 did not finish")
errs = drain_errors()
if errs:
    fail("program with bare G28.2 raised errors: {!r}".format(errs))
print("PASS: bare G28.2 from a program homes every joint")

print("done! it all worked")
sys.exit(0)
