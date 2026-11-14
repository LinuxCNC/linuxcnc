#!/usr/bin/env python3
# Plane frame jogs on a machine not homed: see README.
import hal
import linuxcnc
import math
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

def drain():
    while True:
        m = e.poll()
        if not m:
            return
        print("channel:", m)
        if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            error("reported: %s" % m[1].strip())

# what done() waits for, task done with the last command sent,
# looked at every 2 ms rather than every 10
def done(timeout=60):
    deadline = time.time() + timeout
    while time.time() < deadline:
        s.poll()
        if s.echo_serial_number >= c.serial and s.state != linuxcnc.RCS_EXEC:
            return
        time.sleep(0.002)
    error("timed out waiting for task")

# the machine has stopped when task has nothing left to run, no jog is
# active and the position is the same over two status copies a servo cycle
# or more apart, the first written two task cycles after the command: two
# polls can read the same copy
def settled(jogging=False):
    deadline = time.time() + 60
    s.poll()
    beat = s.taskbeat + 2
    last = None
    last_hb = None
    while time.time() < deadline:
        s.poll()
        if s.taskbeat >= beat:
            now = list(s.position)
            if ((jogging or (s.interp_state == linuxcnc.INTERP_IDLE and s.inpos and not s.queue
                             and not hal.get_value("motion.jog-is-active")))
                    and now == last and s.heartbeat != last_hb):
                return now
            last = now
            last_hb = s.heartbeat
        time.sleep(0.002)
    error("timed out waiting for the machine to stop")
    return last

def fmt(v):
    return " ".join("%.6f" % p for p in v)

def check(what, before, after, want):
    moved = [after[i] - before[i] for i in range(3)]
    print("%s: moved %s, want %s" % (what, fmt(moved), fmt(want)))
    if max(abs(m - w) for m, w in zip(moved, want)) > 1e-5:
        error("%s moved %s, not %s" % (what, fmt(moved), fmt(want)))

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
done()
c.mode(linuxcnc.MODE_MDI)
done()
# the plane's X runs at 45 degrees between X and Y
c.mdi("G68.2 X0 Y0 Z0 I45 J0 K0")
done()
settled()
drain()

c.mode(linuxcnc.MODE_MANUAL)
done()
c.teleop_enable(1)
done()
deadline = time.time() + 10
while time.time() < deadline:
    s.poll()
    if s.motion_mode == linuxcnc.TRAJ_MODE_TELEOP:
        break
    time.sleep(0.002)
if s.motion_mode != linuxcnc.TRAJ_MODE_TELEOP:
    error("motion did not enter teleop mode")
if any(s.homed[:3]):
    error("a joint is homed, the test is empty")
c.jog_frame(linuxcnc.JOG_FRAME_PLANE)
done()

h = math.sqrt(0.5)
# [AXIS_X] and [AXIS_Y] are -10 to 10: homed, this jog would end outside
# them and be refused; not homed, an axis has one range, 20, either way
# of where it stands, as a plain jog of it does
s.poll()
before = list(s.position)
c.jog(linuxcnc.JOG_INCREMENT, 0, 0, 200, 20)
after = settled()
check("an incremental plane X jog of 20", before, after, [20 * h, 20 * h, 0])
drain()

# a continuous jog runs to one range from where it starts, X and Y alike
before = after
c.jog(linuxcnc.JOG_CONTINUOUS, 0, 0, 200)
done()
# at the end of its range the jog stands, still active
settled(jogging=True)
c.jog(linuxcnc.JOG_STOP, 0, 0)
after = settled()
check("a continuous plane X jog", before, after, [20, 20, 0])
drain()

c.jog_frame(linuxcnc.JOG_FRAME_MACHINE)
done()
c.state(linuxcnc.STATE_OFF)
done()

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
