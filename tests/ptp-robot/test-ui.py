#!/usr/bin/env python3
# A serial robot has no axis letter that names the joint it looks like, so
# the letter form of the point-to-point move is the machine frame, the flange
# in the base frame with no tool, and the joint form names the joints.
import hal
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

# the letter form is the machine frame: on a robot the arm kinematics with
# no tool, the flange in the base frame, so a letter moves that coordinate
# of the flange and every other one holds, the orientation included
def flange():
    s.poll()
    return list(s.position[:6])

def kins_type():
    return int(hal.get_value("motion.kins-type"))

def check_held(what, before, after, moved):
    for i, name in enumerate("XYZABC"):
        want = before[i] + moved.get(name, 0.0)
        off = after[i] - want
        if i >= 3:
            # the flange angles come back in (-180, 180]
            off = (off + 180.0) % 360.0 - 180.0
        if abs(off) > 1e-3:
            error("%s left %s at %.4f, not %.4f" % (what, name, after[i], want))

mdi("G53.7 G0 J0=0 J1=-30 J2=40 J3=0 J4=50 J5=0")
before = flange()
after_joints = mdi("G53.5 G0 Z%.6f" % (before[2] + 50))
after = flange()
print("G53.5 G0 Z+50  %s" % " ".join("%.4f" % v for v in after))
drain()
check_held("G53.5 Z", before, after, {"Z": 50})
if abs(after_joints[0]) > 1e-6:
    error("G53.5 Z turned the waist to %.6f" % after_joints[0])

before = flange()
mdi("G53.5 G0 X%.6f A%.6f" % (before[0] - 40, before[3] + 15))
after = flange()
print("G53.5 G0 X-40 A+15  %s" % " ".join("%.4f" % v for v in after))
drain()
check_held("G53.5 X A", before, after, {"X": -40, "A": 15})

# the same move asked from the identity kinematics lands the flange at the
# same place: the letters are the machine frame whatever type is in force,
# and G13.1 comes back to the arm kinematics, which is that frame
mdi("G53.7 G0 J0=0 J1=-30 J2=40 J3=0 J4=50 J5=0")
before = flange()
mdi("G12.1 P1")
if kins_type() != 1:
    error("G12.1 P1 left kins-type %d" % kins_type())
mdi("G53.5 G0 Z%.6f" % (before[2] + 50))
mdi("G13.1")
if kins_type() != 0:
    error("G13.1 left kins-type %d, the arm kinematics is type 0" % kins_type())
after = flange()
print("G53.5 G0 Z+50 from the identity  %s" % " ".join("%.4f" % v for v in after))
drain()
check_held("G53.5 Z from the identity", before, after, {"Z": 50})

# G43.4 and G49 leave a robot on its arm kinematics: there is no other
# frame to come back to
mdi("G43.4 H1")
if kins_type() != 0:
    error("G43.4 left kins-type %d" % kins_type())
mdi("G49")
if kins_type() != 0:
    error("G49 left kins-type %d" % kins_type())
drain()

# a letter of the wrong unit class is not a joint any more: A is the flange
# roll, in degrees, and Z is a length, so neither is refused
mdi("G53.5 G0 A0")
drain()

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

# the wrist turns the tool with three joints: the arm kinematics orients
# with the A B C of its pose, all three head
mdi("G13.1")
drain()
c.mdi("(DEBUG,#<_kins_orient_1> #<_kins_orient_2> #<_kins_orient_3>"
      " #<_kins_orient_1_head> #<_kins_orient_2_head> #<_kins_orient_3_head>)")
c.wait_complete(30)
said = None
deadline = time.time() + 5
while said is None and time.time() < deadline:
    m = e.poll()
    if not m:
        time.sleep(0.01)
    elif m[0] == linuxcnc.OPERATOR_DISPLAY:
        said = [float(v) for v in m[1].split()]
if said != [3, 4, 5, 1, 1, 1]:
    error("the arm kinematics orients with %s, not A B C, heads" % (said,))
print("orienting axes  %s" % said)

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
