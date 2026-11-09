#!/usr/bin/env python3
# The point-to-point moves on the xyzacb nutating-head sim: see README.

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "twp-native"))
from native import *

# from where twp-native's G53.1 Q1 leaves the machine: every rotary turned,
# the table too, under a plane
mdi("G12.1 P1", "G0 X0 Y0 Z0 A0 B0 C0", "G68.2 P1 Q123 I30 J0 K0", "G53.1 Q1")

# --- G53.4, G53.5 and G69 ------------------------------------------------
before = mdi("G69")
after, samples = sampled("G53.4 G0 X0 Y0 Z0 A0 B0 C0")
show("G53.4 G0", after)
drain()
joint_line("G53.4 G0", samples, before, after)
s.poll()
if s.g68_active:
    error("G69 left the plane active")
if not close(list(s.g68_rotation), [1, 0, 0, 0, 1, 0, 0, 0, 1], 1e-12):
    error("G69 left a rotation in status")
if not close(list(s.position[:3]), [0, 0, 0], 1e-6) or any(abs(after[j]) > 1e-6 for j in (TABLE, SECONDARY, PRIMARY)):
    error("G53.4 G0 did not bring the tool and the rotaries back to zero")

# G53.7 takes joint values by joint number: with the head tilted, J2=-5
# puts joint 2 at -5 whatever that does to the tool tip, and touches no
# other joint; the value is the joint's own, untouched by G20
before = mdi("G0 B30")
show("before G53.7", before)
after = mdi("G20 G53.7 G0 J2=-5")
mdi("G21")
show("G53.7 G0 J2=-5", after)
drain()
if abs(after[2] + 5) > 1e-6:
    error("G53.7 J2=-5 left joint 2 at %.6f" % after[2])
for j in (0, 1, 3, 4, 5):
    if abs(after[j] - before[j]) > 1e-6:
        error("G53.7 J2=-5 moved joint %d from %.9f to %.9f" % (j, before[j], after[j]))
after = mdi("G53.7 G0 J2=0 J[2+2]=0")
if abs(after[2]) > 1e-6 or abs(after[SECONDARY]) > 1e-6:
    error("G53.7 J2=0 J4=0 did not put joints 2 and 4 at zero")

# G53.5 takes the same destination by axis letter, in program units: Z-5
# is joint 2 in millimetres, and under G20 the same words are inches
before = mdi("G0 B30")
after = mdi("G53.5 G0 Z-5")
show("G53.5 G0 Z-5", after)
drain()
if abs(after[2] + 5) > 1e-6:
    error("G53.5 Z-5 left joint 2 at %.6f" % after[2])
for j in (0, 1, 3, 4, 5):
    if abs(after[j] - before[j]) > 1e-6:
        error("G53.5 Z-5 moved joint %d from %.9f to %.9f" % (j, before[j], after[j]))
after = mdi("G20 G53.5 G0 Z-1")
mdi("G21")
show("G20 G53.5 G0 Z-1", after)
if abs(after[2] + 25.4) > 1e-6:
    error("an inch of G53.5 Z left joint 2 at %.6f, not -25.4" % after[2])
after = mdi("G53.5 G0 Z0 B0")
if abs(after[2]) > 1e-6 or abs(after[SECONDARY]) > 1e-6:
    error("G53.5 Z0 B0 did not put joints 2 and 4 at zero")

# --- G28.5 and G30.5 ------------------------------------------------------
# the stored positions are saved on the identity kinematics, where the world
# is the slides: G28.1 at X10 Y20 Z-5 with the head straight, G30.1 at the
# same slides with the head at B10
mdi("G12.1 P0", "G0 X10 Y20 Z-5 A0 B0 C0", "G28.1", "G0 B10", "G30.1",
    "G0 X0 Y0 Z0 B0", "G12.1 P1")
# with the head tilted under TCP, G28.5 alone takes every slide to the
# stored position, on one joint-space move
before = mdi("G0 X0 Y0 Z0 A0 B-30 C0")
after, samples = sampled("G28.5")
show("G28.5", after)
drain()
joint_line("G28.5", samples, before, after)
if not close(after, [10, 20, -5, 0, 0, 0], 1e-6):
    error("G28.5 put the joints at %s, not at the stored slides" % (after,))
# an axis word is a slide to pass through first, and then only that letter
# goes to its stored value: Z3 on the way, then Z-5, the rest untouched
before = mdi("G0 X0 Y0 Z0 A0 B-30 C0")
after, samples = sampled("G30.5 Z3")
show("G30.5 Z3", after)
drain()
if abs(after[2] + 5) > 1e-6:
    error("G30.5 Z3 left joint 2 at %.6f, not at the stored -5" % after[2])
for j in (0, 1, 3, 4, 5):
    if abs(after[j] - before[j]) > 1e-6:
        error("G30.5 Z3 moved joint %d from %.9f to %.9f" % (j, before[j], after[j]))
# the first move ends on Z3 and the second starts there inside the same
# servo cycle, so the turn falls between two samples: joint 2 decelerates
# into it at its MAX_ACCELERATION, which over one servo cycle leaves it at
# most a * dt^2 / 2 short of it, and it never goes past it
cfg = linuxcnc.ini(os.environ["INI_FILE_NAME"])
dt = float(cfg.find("EMCMOT", "SERVO_PERIOD")) * 1e-9
short = float(cfg.find("JOINT_2", "MAX_ACCELERATION")) * dt * dt / 2
peak = max(j[2] for j, p in samples)
if peak > 3 + 1e-6 or peak < 3 - short - 1e-6:
    error("G30.5 Z3 did not pass through the Z3 slide (joint 2 peaked at %.6f)" % peak)
after = mdi("G30.5")
if not close(after, [10, 20, -5, 0, 10, 0], 1e-6):
    error("G30.5 put the joints at %s, not at the stored slides with B10" % (after,))
mdi("G0 X0 Y0 Z0 A0 B0 C0")

# a point-to-point feed takes the time the straight move would: 5 mm at
# F600 is half a second, and F120 in G93 is half a second too
def timed(cmd):
    c.mdi(cmd)
    first = last = None
    t0 = time.time()
    s.poll(); start = [s.joint_position[i] for i in range(JOINTS)]
    while time.time() - t0 < 60:
        s.poll()
        now = [s.joint_position[i] for i in range(JOINTS)]
        if now != start:
            if first is None:
                first = time.time()
            last = time.time()
            start = now
        elif (first is not None and s.interp_state == linuxcnc.INTERP_IDLE
              and s.inpos and not s.queue):
            break
        time.sleep(0.005)
    done()
    settled()
    return (last - first) if first else 0.0
mdi("G0 X0 Y0 Z0 A0 B0 C0")
took = timed("G53.4 G1 X5 F600")
print("G53.4 G1 X5 F600 took %.3f s" % took)
if not 0.35 < took < 0.75:
    error("a 5 mm point-to-point feed at F600 took %.3f s, not about half a second" % took)
took = timed("G93 G53.4 G1 X0 F120")
mdi("G94")
print("G93 G53.4 G1 X0 F120 took %.3f s" % took)
if not 0.35 < took < 0.75:
    error("a point-to-point feed at G93 F120 took %.3f s, not about half a second" % took)
drain()

# what the point-to-point codes refuse
for cmd in ("G53.4 G2 X1 I1", "G91 G53.7 G0 J0=1", "G53.7 G0 J9=1", "G53.7 G0 X1",
            "G53.7 G0 J1", "G53.7 G0", "G0 J0=1", "G53.7 G0 J0.5=1", "G53.7 G0 J0=1 J0=2",
            "G53.5 G0 J0=1", "G53.5 G0", "G91 G53.5 G0 X1", "G53.4 G1 F0 X1",
            "G91 G28.5 X1", "G30.5 G1 X1"):
    c.mdi(cmd)
    done(30)
    m = e.poll()
    if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
        error("%s was accepted" % cmd)
    else:
        print("refused as expected:", m[1])
    c.mode(linuxcnc.MODE_MDI)
mdi("G90 G94 G0 X0 Y0 Z0 A0 B0 C0")

# The config sets MACHINE_MOVES_IN_MACHINE_FRAME: under TCP G53 is G53.5, the
# slides at the words whatever the head is doing, on one joint-space move,
# where the tip's G53 would move every slide to keep the tip on machine Z
before = mdi("G12.1 P1", "G0 B30")
after, samples = sampled("G53 G0 Z-5")
show("G53 G0 Z-5 under TCP", after)
drain()
joint_line("G53 G0 Z-5", samples, before, after)
if abs(after[2] + 5) > 1e-6:
    error("G53 Z-5 under TCP left joint 2 at %.6f" % after[2])
for j in (0, 1, 3, 4, 5):
    if abs(after[j] - before[j]) > 1e-6:
        error("G53 Z-5 under TCP moved joint %d from %.9f to %.9f" % (j, before[j], after[j]))
# G28.1 and G30.1 store where the slides stand with the head tilted, and G28
# and G30 go back to them after a plain move has taken the slides along
mdi("G53.5 G0 X10 Y20 Z-5 B-30", "G28.1", "G30.1")
here = mdi("G0 X0 Y0 Z0 B0")
after, samples = sampled("G28")
show("G28 under TCP", after)
drain()
joint_line("G28", samples, here, after)
if not close(after, [10, 20, -5, 0, -30, 0], 1e-6):
    error("G28 under TCP put the joints at %s, not at the slides G28.1 stored" % (after,))
before = mdi("G0 X0 Y0 Z0 B0")
after = mdi("G30 Z3")
if abs(after[2] + 5) > 1e-6:
    error("G30 Z3 under TCP left joint 2 at %.6f, not at the stored -5" % after[2])
for j in (0, 1, 3, 4, 5):
    if abs(after[j] - before[j]) > 1e-6:
        error("G30 Z3 under TCP moved joint %d from %.9f to %.9f" % (j, before[j], after[j]))
# on the identity kinematics the world is the slides and G53 its straight line
mdi("G0 X0 Y0 Z0 B0", "G12.1 P0")
before = mdi("G53 G0 X10 Y0 Z-5")
if not close(before, [10, 0, -5, 0, 0, 0], 1e-6):
    error("G53 X10 Z-5 on the identity put the joints at %s" % (before,))
mdi("G28.1", "G0 X0 Z0", "G28", "G12.1 P1")
if not close(settled(), [10, 0, -5, 0, 0, 0], 1e-6):
    error("G28 on the identity did not go back to the G28.1 store")
mdi("G0 X0 Y0 Z0 A0 B0 C0")
drain()

finish()
