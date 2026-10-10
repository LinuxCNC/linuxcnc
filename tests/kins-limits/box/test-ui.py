#!/usr/bin/env python3
# The [AXIS_L] box is the machine frame envelope, the carriage and not the
# tool tip: see README.
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

def joints():
    s.poll()
    return [s.joint_position[i] for i in range(JOINTS)]

def drain():
    said = []
    while True:
        m = e.poll()
        if not m:
            return said
        said.append(m)

def settled():
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        now = joints()
        if s.inpos and not s.queue and now == last:
            return now
        last = now
        time.sleep(0.05)
    error("timed out waiting for the move")
    return last

def mdi(cmd):
    c.mdi(cmd)
    c.wait_complete(60)
    return settled()

def accepted(cmd, carriage_x):
    drain()
    now = mdi(cmd)
    said = [m for m in drain() if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR)]
    if said:
        error("%s was refused: %s" % (cmd, said[0][1].strip()))
    elif abs(now[0] - carriage_x) > 1e-3:
        error("%s left the carriage at X %.3f, not %.3f" % (cmd, now[0], carriage_x))
    else:
        print("accepted as expected: %-24s carriage X %.3f" % (cmd, now[0]))
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete(30)

def refused(cmd, needle):
    drain()
    before = joints()
    c.mdi(cmd)
    c.wait_complete(30)
    m = e.poll()
    if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
        settled()
        error("%s was accepted" % cmd)
    elif needle not in m[1]:
        error("%s said %r, nothing about %r" % (cmd, m[1].strip(), needle))
    else:
        print("refused as expected: %s" % m[1].strip())
    drain()
    settled()
    if max(abs(a - b) for a, b in zip(joints(), before)) > 1e-6:
        error("%s moved the joints" % cmd)
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete(30)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete(30)
c.home(-1)
c.wait_complete(60)
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
drain()

# the tool centre point kinematics with the tool on, the head laid over by
# a joint move: the tip is now 300 from the carriage along +X at C0
mdi("G12.1 P0")
mdi("G43 H1")
mdi("G53.7 G0 J0=0 J1=0 J2=0 J3=90 J4=0 J5=0")
j = joints()
s.poll()
print("carriage X %.3f, tip X %.3f" % (j[0], s.position[0]))
if abs(s.position[0] - j[0] - 300) > 1e-3:
    error("the tip is %.3f from the carriage, not 300" % (s.position[0] - j[0]))

# the tip past the box, the carriage inside it: accepted
accepted("G0 X600", 300)
# the tip inside the box, the carriage past it: refused
refused("G0 X-300", "limit")
# the carriage past the joint limit: refused whatever the box says
refused("G0 X-800", "limit")
# G53.5 names the carriage: past the box refused, inside it accepted even
# with the tip past the box
refused("G53.5 G0 X-600", "limit")
accepted("G53.5 G0 X400", 400)

mdi("G53.7 G0 J0=0 J3=0")
mdi("G49")
print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
