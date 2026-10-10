#!/usr/bin/env python3
# A move whose joints leave their travel between its ends is refused before
# it is sent: see README.
import linuxcnc
import os
import sys
import time

JOINTS = 6
PROGRAM = os.path.abspath("travel.ngc")
# the line of travel.ngc that swings the carriage past its Y travel
SWING_LINE = 6

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

errors = 0
# the furthest joint 1 was seen, every time the machine is polled
y_peak = -1e99

def error(what):
    global errors
    errors += 1
    print("*** ERROR %s" % what)

def joints():
    global y_peak
    s.poll()
    y_peak = max(y_peak, s.joint_position[1])
    return [s.joint_position[i] for i in range(JOINTS)]

def drain():
    said = []
    while True:
        m = e.poll()
        if not m:
            return said
        said.append(m)

def complaints(said):
    return [m for m in said if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR)]

def settled():
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        now = joints()
        if s.inpos and not s.queue and now == last:
            return now
        last = now
        time.sleep(0.01)
    error("timed out waiting for the move")
    return last

def mdi(cmd):
    c.mdi(cmd)
    c.wait_complete(60)
    return settled()

def back_to_mdi():
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete(30)

def accepted(cmd, joint, where):
    drain()
    now = mdi(cmd)
    said = complaints(drain())
    if said:
        error("%s was refused: %s" % (cmd, said[0][1].strip()))
    elif abs(now[joint] - where) > 1e-3:
        error("%s left joint %d at %.4f, not %.4f" % (cmd, joint, now[joint], where))
    else:
        print("accepted as expected: %-28s joint %d at %.4f" % (cmd, joint, now[joint]))
    back_to_mdi()

def refused(cmd, *needles):
    drain()
    before = joints()
    c.mdi(cmd)
    c.wait_complete(30)
    settled()
    said = complaints(drain())
    if not said:
        error("%s was accepted" % cmd)
    elif not all(n in said[0][1] for n in needles):
        error("%s said %r, not %r" % (cmd, said[0][1].strip(), needles))
    else:
        print("refused as expected: %s" % said[0][1].strip())
    if max(abs(a - b) for a, b in zip(joints(), before)) > 1e-6:
        error("%s moved the joints" % cmd)
    back_to_mdi()

def run(start_line):
    drain()
    c.mode(linuxcnc.MODE_AUTO)
    c.wait_complete(30)
    c.program_open(PROGRAM)
    c.wait_complete(30)
    c.auto(linuxcnc.AUTO_RUN, start_line)
    c.wait_complete(30)
    deadline = time.time() + 60
    while time.time() < deadline:
        joints()
        if s.interp_state == linuxcnc.INTERP_IDLE and not s.queue and s.inpos:
            break
        time.sleep(0.01)
    else:
        error("the program did not finish")
    now = settled()
    said = complaints(drain())
    back_to_mdi()
    return now, said

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete(30)
c.home(-1)
c.wait_complete(60)
back_to_mdi()
drain()

# the tool centre point kinematics, the head laid over by a joint move; the
# carriage at X-400 puts the tip at X0.  A turn of C holding the tip swings
# the carriage round it at the pivot length plus the tool length, C-90
# putting it at Y+ that radius, against joint 1's MAX_LIMIT of 400
mdi("G12.1 P0")
mdi("G53.7 G0 J0=-400 J1=0 J2=0 J3=90 J4=0 J5=0")

# a radius of 400.05: the swing from C-5 to C-175 peaks at C-90 between two
# samples, which are 1.2 inside the travel; the peak is 0.05 past it
mdi("G43.1 Z0.05")
accepted("G0 C-5", 4, -5)
refused("G0 C-175", "joint 1", "MAX_LIMIT", "50%")
# asked again, it is refused again, and a move inside the travel still goes
refused("G0 C-175", "joint 1", "MAX_LIMIT")
accepted("G0 C-60", 4, -60)
accepted("G0 C-5", 4, -5)
# turning the other way, C through +90, runs the carriage through
# Y-400.05, inside the travel
accepted("G0 C175", 4, 175)
accepted("G0 C-5", 4, -5)
# a move that ends past the travel is refused for its end
refused("G0 C-90", "joint 1", "end with", "MAX_LIMIT")

# a radius of 399.95: the same swing peaks 0.05 inside the travel
mdi("G43.1 Z-0.05")
accepted("G0 C-175", 4, -175)
accepted("G0 C-5", 4, -5)

# a radius of 500: the samples themselves are past the travel
mdi("G43.1 Z100")
refused("G0 C-175", "joint 1", "MAX_LIMIT")
mdi("G49")

# an arc with the head upright, its ends inside the travel and its top
# 54.4 past it; the other way round it runs below
mdi("G53.7 G0 J0=0 J1=0 J2=0 J3=0 J4=0 J5=0")
accepted("G0 X-100 Y380", 1, 380)
refused("G2 X100 Y380 I100 J-30 F3000", "joint 1", "MAX_LIMIT")
accepted("G3 X100 Y380 I100 J-30 F3000", 1, 380)
accepted("G0 X0 Y0", 1, 0)

# a program: the line that swings past the travel fails, the moves before
# it may have run, and no joint has passed its limit
mdi("G53.7 G0 J0=-400 J1=0 J2=0 J3=90 J4=0 J5=0")
y_peak = -1e99
now, said = run(0)
if not said:
    error("the program ran through line %d" % SWING_LINE)
elif "line %d" % SWING_LINE not in said[0][1] or "joint 1" not in said[0][1]:
    error("the program said %r" % said[0][1].strip())
else:
    print("refused as expected: %s" % said[0][1].strip())
if any("soft limit" in m[1] for m in said):
    error("a joint ran onto its soft limit")
if y_peak > 400 + 1e-6:
    error("joint 1 reached %.4f, past its MAX_LIMIT" % y_peak)

# run from the line after the swing: the line is only stepped over, its
# moves are thrown away, and the program runs
mdi("G49")
mdi("G53.7 G0 J0=-400 J1=0 J2=0 J3=90 J4=0 J5=0")
now, said = run(SWING_LINE + 1)
if said:
    error("run from line %d said %r" % (SWING_LINE + 1, said[0][1].strip()))
elif abs(now[4] + 5) > 1e-3:
    error("run from line %d left C at %.4f, not -5" % (SWING_LINE + 1, now[4]))
else:
    print("ran from line %d as expected, C at %.4f" % (SWING_LINE + 1, now[4]))

mdi("G49")
mdi("G53.7 G0 J0=0 J1=0 J2=0 J3=0 J4=0 J5=0")
print("joint 1 peaked at %.4f since the program started" % y_peak)
print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
