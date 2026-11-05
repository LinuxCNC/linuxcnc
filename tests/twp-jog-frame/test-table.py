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

def settled():
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        s.poll()
        now = joints()
        if s.inpos and not s.queue and now == last \
           and not hal.get_value("motion.jog-is-active"):
            return now
        last = now
        time.sleep(0.05)
    error("timed out waiting for the machine to stop")
    return last

def fmt(v):
    return " ".join("%.6f" % p for p in v)

def mdi(*cmds):
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete(30)
    for cmd in cmds:
        c.mdi(cmd)
        c.wait_complete(30)
    settled()
    drain()
    c.mode(linuxcnc.MODE_MANUAL)
    c.wait_complete(30)
    c.teleop_enable(1)
    c.wait_complete(30)

def frame(f):
    c.jog_frame(f)
    c.wait_complete(30)

# an incremental jog of 5 on one letter, and the joints it moved
def jog(what, axis, want):
    before = joints()
    c.jog(linuxcnc.JOG_INCREMENT, False, axis, 50, 5)
    after = settled()
    moved = [after[j] - before[j] for j in range(5)]
    print("%s: joints moved %s, want %s" % (what, fmt(moved), fmt(want)))
    if max(abs(m - w) for m, w in zip(moved, want)) > 1e-5:
        error("%s moved the joints %s, not %s" % (what, fmt(moved), fmt(want)))
    drain()
    return moved

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete(30)
c.mode(linuxcnc.MODE_MANUAL)
c.wait_complete(30)
c.teleop_enable(0)
c.wait_complete(30)
c.home(-1)
c.wait_complete(30)
deadline = time.time() + 30
while time.time() < deadline:
    s.poll()
    if all(s.homed[:5]):
        break
    time.sleep(0.1)
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
c.jog(linuxcnc.JOG_INCREMENT, False, 0, 50, 5)
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
c.wait_complete(30)

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
