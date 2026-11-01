#!/usr/bin/env python3
# One failed forward far from home.  Motion must find the platform where
# it is on the next cycle, not on another pose with the same struts.
# Every step waits for what it needs, not for a time: tests run in
# parallel and under load.
import hal
import linuxcnc
import sys
import time

c = linuxcnc.command()
s = linuxcnc.stat()

errors = 0

def error(what):
    global errors
    errors += 1
    print("*** ERROR %s" % what)

def wait_for(what, done, timeout=30):
    deadline = time.time() + timeout
    while time.time() < deadline:
        s.poll()
        if done():
            return True
        time.sleep(0.05)
    error("timed out waiting for %s" % what)
    return False

def fresh():
    # a status motion wrote after everything seen so far: the heartbeat
    # counts servo cycles, and a new value was copied after this call
    s.poll()
    beat = s.heartbeat
    wait_for("a new motion status", lambda: s.heartbeat != beat)

def off(pose, want):
    return max(abs(a - b) for a, b in zip(pose[:6], want))

def glitch():
    # no iteration allowed: the forward fails, then solves again from
    # whatever guess motion gives it
    hal.set_p("genhexkins.limit-iterations", "0")
    wait_for("the forward to fail", lambda: hal.get_value("genhexkins.fwd-kins-fail"))
    hal.set_p("genhexkins.limit-iterations", "120")
    wait_for("the forward to solve again", lambda: not hal.get_value("genhexkins.fwd-kins-fail"))
    fresh()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete(30)
wait_for("machine on", lambda: s.task_state == linuxcnc.STATE_ON)
c.home(-1)
wait_for("all joints homed", lambda: all(s.homed[:6]))
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
wait_for("MDI mode", lambda: s.task_mode == linuxcnc.MODE_MDI)

# far from home (0 0 20 0 0 0), yet on the way the forward follows the
# platform cycle by cycle; started from home, it converges to a platform
# pose with the same six struts near X-14.69 Y-14.81 Z31.67 A28.6 B18.2 C97.1
target = (-15, -15, 32, 30, 15, 90)
c.mdi("G0 X-15 Y-15 Z32 A30 B15 C90")
c.wait_complete(30)
wait_for("the move to end", lambda: s.interp_state == linuxcnc.INTERP_IDLE
         and not s.queue and s.inpos)
fresh()
if off(s.actual_position, target) > 1e-6:
    error("feedback position %s before the failure, expected %s" % (s.actual_position[:6], target))

glitch()
if off(s.actual_position, target) > 1e-6:
    error("feedback position %s after a failed forward, expected %s" % (s.actual_position[:6], target))

# in joint mode motion also runs the forward on the commanded joints
c.mode(linuxcnc.MODE_MANUAL)
c.wait_complete(30)
c.teleop_enable(0)
c.wait_complete(30)
wait_for("joint mode", lambda: s.motion_mode == linuxcnc.TRAJ_MODE_FREE)
glitch()
if off(s.position, target) > 1e-6:
    error("commanded position %s in joint mode after a failed forward, expected %s"
          % (s.position[:6], target))

print("Exiting with %d errors" % errors)
c.state(linuxcnc.STATE_ESTOP)
sys.exit(1 if errors else 0)
