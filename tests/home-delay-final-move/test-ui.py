#!/usr/bin/env python3

# HOME_FINAL_MOVE_START must wait HOME_DELAY (0.1 s in homing.c) after
# the joint has stopped before it plans the final move, and the final
# moves of a negative HOME_SEQUENCE pair must still start together.

import linuxcnc
import hal

import sys
import time

HOME_FINAL_MOVE_START = 20
HOME_DELAY = 0.100

failures = []


def fail(msg):
    print("FAIL " + msg)
    failures.append(msg)


def state(j):
    return hal.get_value("joint.%d.home-state" % j)


def watch(joints, timeout=30.0):
    # Poll home-state; return, per joint, when it entered and left
    # HOME_FINAL_MOVE_START and when its command last changed before it
    # left (the joint had stopped by then).
    enter = {}
    leave = {}
    stopped = {}
    last = {j: hal.get_value("joint.%d.motor-pos-cmd" % j) for j in joints}
    t0 = time.monotonic()
    while time.monotonic() - t0 < timeout:
        now = time.monotonic()
        for j in joints:
            st = state(j)
            p = hal.get_value("joint.%d.motor-pos-cmd" % j)
            if st == HOME_FINAL_MOVE_START:
                enter.setdefault(j, now)
                if p != last[j]:
                    stopped[j] = now
            elif j in enter and j not in leave:
                leave[j] = now
            last[j] = p
        s.poll()
        if all(s.homed[j] for j in joints):
            break
        time.sleep(0.0002)
    return enter, leave, stopped


h = hal.component("test-ui")
h.ready()

c = linuxcnc.command()
s = linuxcnc.stat()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete()
c.mode(linuxcnc.MODE_MANUAL)
c.wait_complete()
c.teleop_enable(0)
c.wait_complete()

# Joint 0 alone.
c.home(0)
c.wait_complete()
enter, leave, stopped = watch([0])
s.poll()
if not s.homed[0]:
    fail("joint 0 did not home")
elif 0 not in leave:
    fail("joint 0: HOME_FINAL_MOVE_START not seen")
else:
    wait = leave[0] - stopped.get(0, enter[0])
    print("joint 0: waited %.3f s after stopping before the final move" % wait)
    if wait < 0.8 * HOME_DELAY:
        fail("joint 0: final move started %.3f s after the stop, expected >= %.3f" % (wait, HOME_DELAY))

# Synchronized pair (HOME_SEQUENCE = -1), switches at different places.
c.home(1)
c.wait_complete()
enter, leave, stopped = watch([1, 2])
s.poll()
if not (s.homed[1] and s.homed[2]):
    fail("pair did not home")
elif 1 not in leave or 2 not in leave:
    fail("pair: HOME_FINAL_MOVE_START not seen")
else:
    for j in (1, 2):
        print("joint %d: in HOME_FINAL_MOVE_START %.3f s" % (j, leave[j] - enter[j]))
    skew = abs(leave[1] - leave[2])
    late = max(enter[1], enter[2])
    wait = min(leave[1], leave[2]) - late
    print("pair: final moves %.3f s apart, %.3f s after the later joint arrived" % (skew, wait))
    if skew > 0.03:
        fail("pair: final moves started %.3f s apart" % skew)
    if wait < 0.8 * HOME_DELAY:
        fail("pair: final move started %.3f s after the later joint arrived, expected >= %.3f" % (wait, HOME_DELAY))

c.state(linuxcnc.STATE_ESTOP)
c.wait_complete()

if failures:
    print("%d failure(s):" % len(failures))
    for msg in failures:
        print("    " + msg)
    sys.exit(1)

print("success")
sys.exit(0)
