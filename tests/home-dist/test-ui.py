#!/usr/bin/env python3

import linuxcnc
import hal

import os
import subprocess
import sys
import time

# how close a joint must come back to where its move began
TOLERANCE = 0.01

failures = []


def fail(msg):
    print("FAIL " + msg)
    failures.append(msg)


def wait_for(cond, timeout=10.0):
    start_time = time.time()
    while time.time() - start_time < timeout:
        s.poll()
        if cond():
            return True
        time.sleep(0.02)
    s.poll()
    return cond()


def errors():
    out = []
    while True:
        error = e.poll()
        if not error:
            return out
        print("linuxcnc error %d: %s" % (error[0], error[1]))
        out.append(error[1])


def machine_on():
    c.state(linuxcnc.STATE_ON)
    c.wait_complete()
    c.mode(linuxcnc.MODE_MANUAL)
    c.wait_complete()


def sets(name, value):
    subprocess.run(["halcmd", "sets", name, str(value)], check=True)


def pos(j):
    return hal.get_value("joint.%d.motor-pos-cmd" % j)


def home(joint, label, expect):
    # Home 'joint', wait for the homing to end, check that the expected
    # message came and that the machine is still on and unhomed.
    start = [pos(j) for j in range(3)]
    errors()
    c.teleop_enable(0)
    c.wait_complete()
    c.home(joint)
    c.wait_complete()
    if not wait_for(lambda: any(j["homing"] for j in s.joint[:3]), 5.0):
        fail("%s: homing did not start" % label)
        return start
    farthest = list(start)
    t0 = time.time()
    while time.time() - t0 < 30.0:
        s.poll()
        for j in range(3):
            if abs(pos(j) - start[j]) > abs(farthest[j] - start[j]):
                farthest[j] = pos(j)
        if os.environ.get("TRACE"):
            print("  t=%.3f pos=%.3f,%.3f,%.3f state=%d,%d,%d sw=%d,%d,%d" % (
                time.time() - t0, pos(0), pos(1), pos(2),
                hal.get_value("joint.0.home-state"), hal.get_value("joint.1.home-state"),
                hal.get_value("joint.2.home-state"),
                hal.get_value("joint.0.home-sw-in"), hal.get_value("joint.1.home-sw-in"),
                hal.get_value("joint.2.home-sw-in")))
        if not any(j["homing"] for j in s.joint[:3]):
            break
        time.sleep(0.002)
    time.sleep(0.3)
    s.poll()
    msgs = errors()
    end = [pos(j) for j in range(3)]
    print("%-28s farthest=%s end=%s homed=%d,%d,%d state=%d" % (
        label, ["%.3f" % (f - st) for f, st in zip(farthest, start)],
        ["%.3f" % (en - st) for en, st in zip(end, start)],
        s.homed[0], s.homed[1], s.homed[2], s.task_state))
    sys.stdout.flush()
    if not any(expect in m for m in msgs):
        fail("%s: no error containing '%s'" % (label, expect))
    if any(s.homed[:3]):
        fail("%s: a joint reports homed" % label)
    if s.task_state != linuxcnc.STATE_ON:
        fail("%s: machine is no longer on" % label)
    return start, farthest, end


def check_return(label, j, start, end):
    if abs(end[j] - start[j]) > TOLERANCE:
        fail("%s: joint %d ended %.3f from where it started" % (label, j, end[j] - start[j]))


def check_moved(label, j, start, farthest, dist):
    if abs(abs(farthest[j] - start[j]) - dist) > TOLERANCE:
        fail("%s: joint %d moved %.3f, expected %.3f" % (label, j, abs(farthest[j] - start[j]), dist))


h = hal.component("test-ui")
h.ready()

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.wait_complete()
machine_on()

# Dead switch: the search runs HOME_SEARCH_DIST and stays there.
sets("j0swpos", -1000)
start, farthest, end = home(0, "dead switch", "HOME_SEARCH_DIST")
check_moved("dead switch", 0, start, farthest, 5.0)
if abs(end[0] - start[0] + 5.0) > TOLERANCE:
    fail("dead switch: joint 0 ended %.3f from the start, expected -5" % (end[0] - start[0]))

# Switch stuck on: the initial back-off runs HOME_LATCH_DIST the other
# way and comes back.
sets("j0swpos", 1000)
start, farthest, end = home(0, "switch stuck on", "did not clear")
check_moved("switch stuck on", 0, start, farthest, 2.0)
check_return("switch stuck on", 0, start, end)

# Intermittent switch: found by the search, dead from the back-off on,
# so the latch move runs HOME_LATCH_DIST past the point where the switch
# was seen and comes back to where the latch move began.
sets("j0swpos", pos(0) - 3.0)
sets("j0dead-reset", 0)
start, farthest, end = home(0, "intermittent switch", "HOME_LATCH_DIST")
if not -3.6 < end[0] - start[0] < -2.6:
    fail("intermittent switch: joint 0 ended %.3f from the start, expected about -3" % (end[0] - start[0]))
if not (farthest[0] - start[0]) < -4.9:
    fail("intermittent switch: joint 0 only reached %.3f" % (farthest[0] - start[0]))
sets("j0dead-reset", 1)

# Synchronized pair, both switches stuck on: joint 1 runs out of
# HOME_LATCH_DIST first, joint 2 stops with it, and both come back.
sets("j1swpos", 1000)
sets("j2swpos", 1000)
start, farthest, end = home(1, "pair, switches stuck on", "did not clear")
check_moved("pair, switches stuck on", 1, start, farthest, 2.0)
check_return("pair, switches stuck on", 1, start, end)
check_return("pair, switches stuck on", 2, start, end)
if abs(farthest[2] - start[2]) >= 3.0 - TOLERANCE:
    fail("pair, switches stuck on: joint 2 ran its own bound instead of stopping with joint 1")

# A working switch still homes.
sets("j0swpos", pos(0) - 3.0)
c.teleop_enable(0)
c.wait_complete()
c.home(0)
c.wait_complete()
if not wait_for(lambda: s.homed[0], 20.0):
    errors()
    fail("working switch: joint 0 did not home")
else:
    print("%-28s homed" % "working switch")

c.state(linuxcnc.STATE_ESTOP)
c.wait_complete()

if failures:
    print("%d failure(s):" % len(failures))
    for msg in failures:
        print("    " + msg)
    sys.exit(1)

print("success")
sys.exit(0)
