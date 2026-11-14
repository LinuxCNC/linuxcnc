#!/usr/bin/env python3
# Machine frame jogs on a trunnion under the tool centre point: see README.
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

def drain():
    while True:
        m = e.poll()
        if not m:
            return
        print("channel:", m)
        if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            error("reported: %s" % m[1].strip())

def joints():
    s.poll()
    return [s.joint[j]["output"] for j in range(5)]

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
            now = joints()
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

def mdi(*cmds):
    c.mode(linuxcnc.MODE_MDI)
    done()
    for cmd in cmds:
        c.mdi(cmd)
        done()
    settled()
    drain()
    c.mode(linuxcnc.MODE_MANUAL)
    done()
    c.teleop_enable(1)
    done()

def frame(f):
    c.jog_frame(f)
    done()

# an incremental jog of 5 on one letter, and the joints it moved
def jog(what, axis, want):
    before = joints()
    c.jog(linuxcnc.JOG_INCREMENT, False, axis, 200, 5)
    after = settled()
    moved = [after[j] - before[j] for j in range(5)]
    print("%s: joints moved %s, want %s" % (what, fmt(moved), fmt(want)))
    if max(abs(m - w) for m, w in zip(moved, want)) > 1e-5:
        error("%s moved the joints %s, not %s" % (what, fmt(moved), fmt(want)))
    drain()
    return moved

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
done()
c.mode(linuxcnc.MODE_MANUAL)
done()
c.teleop_enable(0)
done()
c.home(-1)
done()
deadline = time.time() + 30
while time.time() < deadline:
    s.poll()
    if all(s.homed[:5]):
        break
    time.sleep(0.002)
else:
    error("the joints did not home")

# the tool centre point, the table turned a quarter turn
mdi("G12.1 P1", "G0 X0 Y0 Z0 A0 C90")

# the machine axes: the slides, however the table stands
frame(linuxcnc.JOG_FRAME_MACHINE)
jog("machine X at C90", 0, [5, 0, 0, 0, 0])
jog("machine Y at C90", 1, [0, 5, 0, 0, 0])
jog("machine Z at C90", 2, [0, 0, 5, 0, 0])

# the plane frame with no plane: the part's axes, turned with the table,
# so its X is a slide other than X
frame(linuxcnc.JOG_FRAME_PLANE)
before = joints()
c.jog(linuxcnc.JOG_INCREMENT, False, 0, 200, 5)
after = settled()
moved = [after[j] - before[j] for j in range(5)]
print("plane X at C90, no plane: joints moved %s" % fmt(moved))
if abs(moved[0]) > 1e-5 or abs(abs(moved[1]) - 5) > 1e-5 or abs(moved[2]) > 1e-5:
    error("plane X at C90 moved the joints %s, not Y alone" % fmt(moved))
drain()

# the tool frame: a trunnion's tool stands still, Z is the Z slide
frame(linuxcnc.JOG_FRAME_TOOL)
jog("tool Z at C90", 2, [0, 0, 5, 0, 0])

# the table tilted as well
mdi("G0 A30")
frame(linuxcnc.JOG_FRAME_MACHINE)
jog("machine X at A30 C90", 0, [5, 0, 0, 0, 0])
jog("machine Z at A30 C90", 2, [0, 0, 5, 0, 0])

# the machine frame type: the slides too
mdi("G13.1")
jog("machine X on the identity type", 0, [5, 0, 0, 0, 0])

frame(linuxcnc.JOG_FRAME_MACHINE)
c.state(linuxcnc.STATE_OFF)
done()

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
