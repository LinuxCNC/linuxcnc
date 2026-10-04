#!/usr/bin/env python3

import linuxcnc
import hal

import time
import sys

HOME_OFFSET = 5.0
TOLERANCE = 1e-6

failures = []


def wait_for(cond, timeout=5.0):
    start_time = time.time()
    while time.time() - start_time < timeout:
        s.poll()
        if cond():
            return True
        time.sleep(0.05)
    s.poll()
    return cond()


def print_errors():
    while True:
        error = e.poll()
        if not error:
            return
        print("linuxcnc error %d: %s" % (error[0], error[1]))


def report(label):
    # give motion and task a few cycles to publish the new state
    time.sleep(0.2)
    s.poll()
    print_errors()
    print("%-40s homed=%d motor-pos-fb=%.6f motor-offset=%.6f pos-fb=%.6f "
          "joint input=%.6f output=%.6f axis x=%.6f" % (
              label,
              s.homed[0],
              hal.get_value("joint.0.motor-pos-fb"),
              hal.get_value("joint.0.motor-offset"),
              hal.get_value("joint.0.pos-fb"),
              s.joint[0]["input"],
              s.joint[0]["output"],
              s.actual_position[0]))
    sys.stdout.flush()


def check(label, expected):
    report(label)
    actual = s.joint[0]["input"]
    if abs(actual - expected) > TOLERANCE:
        msg = "%s: joint 0 position is %.6f, expected %.6f" % (label, actual, expected)
        print("FAIL " + msg)
        failures.append(msg)


def set_encoder(value):
    hal.set_p("joint.0.motor-pos-fb", str(value))


def home():
    # homing is only allowed in joint mode, and task switches to teleop
    # once all joints are homed
    c.teleop_enable(0)
    c.wait_complete()
    c.home(0)
    c.wait_complete()
    if not wait_for(lambda: s.homed[0]):
        print_errors()
        print("failed to home joint 0")
        sys.exit(1)


def unhome():
    c.teleop_enable(0)
    c.wait_complete()
    c.unhome(0)
    c.wait_complete()
    if not wait_for(lambda: not s.homed[0]):
        print_errors()
        print("failed to unhome joint 0")
        sys.exit(1)


def machine_on():
    c.state(linuxcnc.STATE_ON)
    c.wait_complete()
    c.mode(linuxcnc.MODE_MANUAL)
    c.wait_complete()


def machine_off():
    c.state(linuxcnc.STATE_OFF)
    c.wait_complete()


h = hal.component("test-ui")
h.ready()

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.wait_complete()
machine_on()

encoder = 10.0
set_encoder(encoder)
report("before first home")

home()
check("first home", encoder + HOME_OFFSET)

# Re-homing a homed absolute encoder joint should be silently ignored.
for i in range(3):
    home()
    check("home again while homed #%d" % (i + 1), encoder + HOME_OFFSET)

for i in range(3):
    unhome()
    report("unhomed #%d" % (i + 1))
    home()
    check("unhome + home #%d" % (i + 1), encoder + HOME_OFFSET)

# The machine moved while powered off: the encoder reports a new value.
machine_off()
encoder = 20.0
set_encoder(encoder)
machine_on()
report("encoder changed, still homed")
unhome()
home()
check("home after encoder changed", encoder + HOME_OFFSET)
home()
check("home again after encoder changed", encoder + HOME_OFFSET)

# Back to the original encoder value, so every run saves the same
# position on shutdown.
machine_off()
encoder = 10.0
set_encoder(encoder)
machine_on()
unhome()
home()
check("home after encoder changed back", encoder + HOME_OFFSET)

machine_off()
c.state(linuxcnc.STATE_ESTOP)
c.wait_complete()

if failures:
    print("%d failure(s):" % len(failures))
    for msg in failures:
        print("    " + msg)
    sys.exit(1)

print("success")
sys.exit(0)
