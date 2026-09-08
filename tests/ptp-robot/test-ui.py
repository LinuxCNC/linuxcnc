#!/usr/bin/env python3
# A serial robot has no axis letter that names the joint it looks like, so
# the letter form of the point-to-point move is refused here and the joint
# form is the one that works.
import linuxcnc
import sys
import time

JOINTS = 6

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

errors = 0

def error(what):
    global errors
    errors += 1
    print("*** ERROR %s" % what)

def settled():
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        s.poll()
        now = [s.joint_position[i] for i in range(JOINTS)]
        if s.inpos and not s.queue and now == last:
            return now
        last = now
        time.sleep(0.05)
    error("timed out waiting for the move")
    return last

def drain():
    while e.poll():
        pass

def mdi(cmd):
    c.mdi(cmd)
    c.wait_complete(60)
    return settled()

def refused(cmd, expect):
    c.mdi(cmd)
    c.wait_complete(30)
    m = e.poll()
    if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
        error("%s was accepted" % cmd)
        return
    if expect not in m[1]:
        error("%s said %r, which does not mention %r" % (cmd, m[1].strip(), expect))
    else:
        print("refused as expected: %s" % m[1].strip())
    drain()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete(30)
c.home(-1)
c.wait_complete(60)
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
drain()

# the joint form moves the joints it names and leaves the rest alone
before = mdi("G53.7 G0 J0=0 J1=0 J2=0 J3=0 J4=0 J5=0")
after = mdi("G53.7 G0 J1=-20 J4=35")
print("G53.7 G0 J1=-20 J4=35  %s" % " ".join("%.4f" % v for v in after))
drain()
if abs(after[1] + 20) > 1e-6 or abs(after[4] - 35) > 1e-6:
    error("G53.7 left joints 1 and 4 at %.6f and %.6f" % (after[1], after[4]))
for j in (0, 2, 3, 5):
    if abs(after[j] - before[j]) > 1e-6:
        error("G53.7 moved joint %d from %.9f to %.9f" % (j, before[j], after[j]))

# a wrist reaches the same point with the forearm turned half a revolution
# and the wrist joints reversed, so a joint driven through zero must not
# come back as the other set: the joints not named stay where they are
mdi("G53.7 G0 J0=0 J1=0 J2=0 J3=0 J4=0 J5=0")
held = mdi("G53.7 G0 J4=90")
for value in (-10, 90, -45):
    now = mdi("G53.7 G0 J4=%d" % value)
    print("G53.7 G0 J4=%-4d %s" % (value, " ".join("%.4f" % v for v in now)))
    drain()
    if abs(now[4] - value) > 1e-6:
        error("G53.7 J4=%d left joint 4 at %.6f" % (value, now[4]))
    for j in (0, 1, 2, 3, 5):
        if abs(now[j] - held[j]) > 1e-6:
            error("G53.7 J4=%d moved joint %d from %.6f to %.6f"
                  % (value, j, held[j], now[j]))

# the letter form is refused whichever letter is used, because X names the
# first rotary joint here; the message says so and points at G53.7
refused("G53.5 G0 X10", "joint 0")
refused("G53.5 G0 A10", "G53.7")
refused("G53.5 G0 Z0", "angular")

# and the code that takes a Cartesian target still works: the point the
# robot is standing on is reachable by definition, so ask for it
s.poll()
here = list(s.position[:3])
mdi("G53.7 G0 J1=0 J4=0")
# a serial robot reaches one point with more than one set of joints, so
# only the point is checked, not the pose it comes back in
back = mdi("G53.4 G0 X%.6f Y%.6f Z%.6f" % (here[0], here[1], here[2]))
drain()
s.poll()
if max(abs(a - b) for a, b in zip(s.position[:3], here)) > 1e-3:
    error("G53.4 landed at %s, not at %s"
          % (["%.4f" % v for v in s.position[:3]], ["%.4f" % v for v in here]))
print("G53.4 back to the same point  %s" % " ".join("%.4f" % v for v in back))

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
