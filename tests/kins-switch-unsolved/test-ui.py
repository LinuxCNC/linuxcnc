#!/usr/bin/env python3
# A kinematics switch the new forward cannot solve.  Motion must keep the
# position it knows, say so, abort, and take a switch back afterwards.
import hal
import linuxcnc
import sys
import time

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

errors = 0

def error(what):
    global errors
    errors += 1
    print("*** ERROR %s" % what)

def kins_type():
    return int(round(hal.get_value("motion.kins-type")))

def position():
    s.poll()
    return tuple(round(v, 6) for v in s.position[:6])

def drain():
    said = []
    while True:
        m = e.poll()
        if not m:
            return said
        said.append(m[1])

def wait_idle():
    deadline = time.time() + 30
    while time.time() < deadline:
        s.poll()
        if s.interp_state == linuxcnc.INTERP_IDLE and not s.queue:
            return
        time.sleep(0.05)
    error("timed out waiting for the interpreter")

def mdi(cmd):
    c.mdi(cmd)
    c.wait_complete(30)
    wait_idle()
    time.sleep(0.2)   # let the error channel and the status catch up

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete(30)
c.home(-1)
c.wait_complete(60)
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
drain()

if kins_type() != 0:
    error("starts in kinematics type %d, expected identity (0)" % kins_type())

def joints():
    s.poll()
    return tuple(round(v, 6) for v in s.joint_position[:6])

# a known position, in identity: joints and coordinates are the same thing
mdi("G0 X1 Y2 Z3 A4 B5 C6")
before = position()
if before != (1, 2, 3, 4, 5, 6):
    error("position before the switch is %s, expected (1, 2, 3, 4, 5, 6)" % (before,))
drain()

# the switch the hexapod forward cannot solve: six struts of a few units
# are no platform position.  Nothing may change but the report.
mdi("G12.1 P1")
said = drain()
if not any("kinematicsForward failed" in m for m in said):
    error("no report of the failed forward kinematics, got %s" % said)
if kins_type() != 0:
    error("kinematics type %d after the failed switch, expected 0 still" % kins_type())
if position() != before:
    error("position after the failed switch is %s, expected %s unchanged" % (position(), before))
if joints() != before:
    error("joints after the failed switch are %s, expected %s unchanged" % (joints(), before))
s.poll()
if s.task_state != linuxcnc.STATE_ON:
    error("task state %d after the failed switch, expected still ON" % s.task_state)

# motion goes on working in the kinematics it kept, and the module is
# back in it too: the joints follow the identity inverse, not the hexapod's
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
mdi("G0 X30 Y30 Z30 A30 B30 C30")
if joints() != (30, 30, 30, 30, 30, 30):
    error("joints after the move are %s, expected all 30" % (joints(),))
said = drain()
if said:
    error("unexpected messages after the failed switch: %s" % said)

# a switch to the kinematics in force is nothing to do
mdi("G13.1")
if kins_type() != 0:
    error("kinematics type %d after G13.1, expected identity (0)" % kins_type())
if position() != (30, 30, 30, 30, 30, 30):
    error("position after G13.1 is %s, expected all 30" % (position(),))
said = drain()
if said:
    error("unexpected messages after the switch back: %s" % said)

print("Exiting with %d errors" % errors)
c.state(linuxcnc.STATE_ESTOP)
sys.exit(1 if errors else 0)
