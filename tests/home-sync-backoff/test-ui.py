#!/usr/bin/env python3

import linuxcnc
import hal

import os
import subprocess
import time
import sys

# switch misalignment, see test.ini
MISALIGNMENT = 2.0

# allowed strain beyond misalignment / 2: latch-move overshoot plus
# polling slack
STRAIN_TOLERANCE = 0.05

failures = []


def print_errors():
    while True:
        error = e.poll()
        if not error:
            return
        print("linuxcnc error %d: %s" % (error[0], error[1]))
        if "following error" in error[1]:
            fail(error[1])


def wait_for(cond, timeout=10.0):
    start_time = time.time()
    while time.time() - start_time < timeout:
        s.poll()
        if cond():
            return True
        time.sleep(0.02)
    s.poll()
    return cond()


def fail(msg):
    print("FAIL " + msg)
    failures.append(msg)


def unhome():
    # a following error switches the machine off
    s.poll()
    if s.task_state != linuxcnc.STATE_ON:
        machine_on()
    c.teleop_enable(0)
    c.wait_complete()
    c.unhome(-1)
    c.wait_complete()
    if not wait_for(lambda: not (s.homed[0] or s.homed[1]), 5.0):
        print_errors()
        print("failed to unhome")
        sys.exit(1)


def motor_pos():
    # motor-pos-cmd is continuous through homing; joint_position jumps
    # when the switch position is set
    return [hal.get_value("joint.0.motor-pos-cmd"), hal.get_value("joint.1.motor-pos-cmd")]


def machine_on():
    c.state(linuxcnc.STATE_ON)
    c.wait_complete()
    c.mode(linuxcnc.MODE_MANUAL)
    c.wait_complete()


def set_switches(j0, j1):
    # relative to where the motors sit now; they sit together after every
    # homing since HOME is square
    for j, rel in ((0, j0), (1, j1)):
        pos = hal.get_value("joint.%d.motor-pos-cmd" % j) + rel
        subprocess.run(["halcmd", "sets", "j%dswpos" % j, str(pos)], check=True)
    time.sleep(0.1)


def home_and_watch(label, misalignment):
    # Home the pair and record the peak bridge strain (half the distance
    # the sides are driven apart). Touching both switches racks the machine
    # by their misalignment, so the floor is misalignment / 2.
    c.teleop_enable(0)
    c.wait_complete()
    c.home(0)
    c.wait_complete()
    if not wait_for(lambda: s.joint[0]["homing"] or s.joint[1]["homing"], 5.0):
        fail("%s: homing did not start" % label)
        return
    peak = 0.0
    t0 = time.time()
    while time.time() - t0 < 30.0:
        s.poll()
        peak = max(peak, abs(hal.get_value("bridge-strain")))
        if os.environ.get("TRACE"):
            print("  t=%.3f strain=%.3f defl=%.3f fe=%.3f,%.3f pos=%.3f,%.3f state=%d,%d" % (
                time.time() - t0, hal.get_value("bridge-strain"), hal.get_value("bridge-deflection"),
                hal.get_value("joint.0.f-error"), hal.get_value("joint.1.f-error"),
                hal.get_value("joint.0.motor-pos-cmd"), hal.get_value("joint.1.motor-pos-cmd"),
                hal.get_value("joint.0.home-state"), hal.get_value("joint.1.home-state")))
        if not (s.joint[0]["homing"] or s.joint[1]["homing"]):
            break
        time.sleep(0.002)
    time.sleep(0.2)
    s.poll()
    print_errors()
    limit = misalignment / 2 + STRAIN_TOLERANCE
    print("%-20s homed=%d,%d pos=%.3f,%.3f peak strain=%.3f (limit %.3f)" % (
        label, s.homed[0], s.homed[1], s.joint_position[0], s.joint_position[1], peak, limit))
    sys.stdout.flush()
    if not (s.homed[0] and s.homed[1]):
        fail("%s: joints not homed" % label)
    if peak > limit:
        fail("%s: the sides were driven %.3f apart, limit %.3f" % (label, 2 * peak, 2 * limit))


h = hal.component("test-ui")
h.ready()

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.wait_complete()
machine_on()

# The switches are 2 mm out of line (test.ini), so the floor for the
# peak strain is 1 mm in every scenario; what changes is where the gantry
# sits when homing starts.

# Far from both switches: joint 1 trips 2 mm before joint 0.
set_switches(-31, -29)
home_and_watch("far from switches", MISALIGNMENT)

# At HOME: joint 1 starts 1 mm into its switch, joint 0 1 mm short of
# its own.
unhome()
set_switches(-1, 1)
home_and_watch("joint 1 on switch", MISALIGNMENT)

# Both sides inside their switches.
unhome()
set_switches(1, 3)
home_and_watch("both on switch", MISALIGNMENT)

c.state(linuxcnc.STATE_ESTOP)
c.wait_complete()

if failures:
    print("%d failure(s):" % len(failures))
    for msg in failures:
        print("    " + msg)
    sys.exit(1)

print("success")
sys.exit(0)
