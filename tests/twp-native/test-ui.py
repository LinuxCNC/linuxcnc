#!/usr/bin/env python3
# The tilted work plane on the xyzacb nutating-head sim, natively: see README.

import linuxcnc
import hal
import sys
import os
import time
import math
import numpy as np

TOPDIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(TOPDIR, "tests", "kins-twp", "xyzacb"))
import remap_funcs_twp as twp

JOINTS = 6
TABLE, SECONDARY, PRIMARY = 3, 4, 5      # A, B, C
I4 = np.asmatrix(np.identity(4))

class Log:
    def debug(self, *a): pass
    def error(self, *a): print("oracle:", a)
log = Log()

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()
c.mode(linuxcnc.MODE_MDI)

errors = 0

def error(msg):
    global errors
    errors += 1
    print("*** ERROR " + msg)

def drain():
    while True:
        m = e.poll()
        if not m:
            return
        print("channel:", m)
        if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            error("reported: %s" % m[1])

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

def mdi(*cmds):
    for cmd in cmds:
        c.mdi(cmd)
        c.wait_complete(60)
    return settled()

# the path of a move is read from the sampler log, every servo cycle of the
# joint commands and the world position, which halsampler writes to
# samples.log as it runs; status would give a snapshot per task cycle, which
# misses the turn of a move that stops and comes back
LOG = "samples.log"

def log_samples():
    with open(LOG) as f:
        lines = f.read().split("\n")
    out = []
    for line in lines[:-1]:       # the last piece is a line still being written
        v = line.split()
        if len(v) != 1 + JOINTS + 3:
            continue               # an "overrun" line, counted from the pin at the end
        v = [float(x) for x in v[1:]]
        out.append((v[:JOINTS], v[JOINTS:]))
    return out

# run one command and return the settled joints and the log of the way there:
# the log is block buffered, so it is read until it has caught up with the
# machine at rest
def sampled(cmd):
    n0 = len(log_samples())
    c.mdi(cmd)
    c.wait_complete(60)
    end = settled()
    deadline = time.time() + 10
    while True:
        samples = log_samples()
        if len(samples) > n0 and close(samples[-1][0], end, 2e-6):
            break
        if time.time() > deadline:
            error("%s: the sampler log did not catch up with the machine" % cmd)
            break
        time.sleep(0.02)
    return end, samples[n0:]

# a point-to-point move runs every joint on a straight line in joint space,
# all together: the fraction of the way each moving joint has gone is the
# same for all of them at every sample, never goes back, and reaches one;
# the log prints six decimals, which on a short move is 1e-5 of the way
def joint_line(what, samples, start, end):
    moving = [i for i in range(JOINTS) if abs(end[i] - start[i]) > 1e-6]
    if not moving:
        error("%s: no joint moved" % what)
        return
    last = 0.0
    for n, (j, p) in enumerate(samples):
        fs = [(j[i] - start[i]) / (end[i] - start[i]) for i in moving]
        f = sum(fs) / len(fs)
        if max(abs(x - f) for x in fs) > 1e-5:
            error("%s: joints out of step at sample %d of %d: %s" % (what, n, len(samples), fs))
            return
        if f < last - 1e-6:
            error("%s: the joint path ran backwards at sample %d of %d (%.4f after %.4f)" % (what, n, len(samples), f, last))
            return
        if f < -1e-6 or f > 1 + 1e-6:
            error("%s: a joint left its segment at sample %d of %d (fraction %.4f)" % (what, n, len(samples), f))
            return
        last = f
    if last < 1 - 1e-6:
        error("%s: the last sample is short of the end (fraction %.4f)" % (what, last))
    print("%s: %d joints on one line in joint space through %d samples" % (what, len(moving), len(samples)))

def show(what, j):
    print("%-26s %s" % (what, " ".join("%.4f" % v for v in j)))

def wrap(d):
    return (d + 180.0) % 360.0 - 180.0

def close(a, b, tol=1e-6):
    return all(abs(x - y) <= tol for x, y in zip(a, b))

# the pairs the oracle would keep for a tool axis, in machine coordinates,
# as (b, c) in degrees, and which is nearest to the head's present angles
def oracle_pairs(z):
    t1s, t2s = twp.kins_calc_possible_joint_angles(log, list(z), None)
    pairs = []
    for t1 in t1s or []:
        for t2 in t2s or []:
            m = twp.kins_calc_transformation_matrix(t1, t2, 0, I4, 'inv')
            got = [m[0, 2], m[1, 2], m[2, 2]]
            if close(got, z, 1e-6):
                pairs.append((math.degrees(t2), math.degrees(t1)))
    return pairs

def nearest_pair(pairs, b_now, c_now):
    return min(pairs, key=lambda p: abs(wrap(p[0] - b_now)) + abs(wrap(p[1] - c_now)))

def tool_axis(joints):
    # the tool axis in work coordinates: the head as the oracle models it,
    # brought into the table's frame the way the module reports the work
    m = twp.kins_calc_transformation_matrix(math.radians(joints[PRIMARY]),
                                            math.radians(joints[SECONDARY]), 0, I4, 'inv')
    zm = np.array([m[0, 2], m[1, 2], m[2, 2]])
    a = math.radians(joints[TABLE])
    W = np.array([[1, 0, 0], [0, math.cos(a), math.sin(a)], [0, -math.sin(a), math.cos(a)]])
    return W.T.dot(zm)

def plane_axes():
    s.poll()
    r = s.g68_rotation
    return ([r[0], r[3], r[6]], [r[1], r[4], r[7]], [r[2], r[5], r[8]])

def rot_x(d):
    r = math.radians(d)
    return np.array([[1, 0, 0], [0, math.cos(r), -math.sin(r)], [0, math.sin(r), math.cos(r)]])

def rot_y(d):
    r = math.radians(d)
    return np.array([[math.cos(r), 0, math.sin(r)], [0, 1, 0], [-math.sin(r), 0, math.cos(r)]])

# --- the plane, and G53.1 with the table held ---------------------------
# no G53.2 has run yet, so the pose parameters must refuse to be read
c.mdi("G0 X#<_orient_x>")
c.wait_complete(30)
m = e.poll()
if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
    error("reading #<_orient_x> before the first G53.2 was accepted")
drain()
start = mdi("G12.1 P1", "G0 X0 Y0 Z0 A0 B0 C0")
show("start", start)
R = rot_y(20).dot(rot_x(30))
mdi("G68.2 P1 Q123 I30 J20 K0")
after, samples = sampled("G53.1")
show("G53.1", after)
drain()
s.poll()
if not s.g68_active:
    error("G68.2 did not leave a plane active")
if not close(plane_axes()[2], list(R[:, 2]), 1e-9):
    error("status reports a different plane normal than the program defined")
pairs = oracle_pairs(list(R[:, 2]))
print("oracle pairs (b, c):", ["(%.4f, %.4f)" % p for p in pairs])
want = nearest_pair(pairs, start[SECONDARY], start[PRIMARY])
if not pairs or abs(wrap(after[SECONDARY] - want[0])) > 1e-3 or abs(wrap(after[PRIMARY] - want[1])) > 1e-3:
    error("G53.1 landed on (%.4f, %.4f), the oracle's nearest pair is (%.4f, %.4f)"
          % (after[SECONDARY], after[PRIMARY], want[0], want[1]))
if abs(after[TABLE] - start[TABLE]) > 1e-9:
    error("G53.1 moved the table with Q0")
worst = 0.0
where = (0, 0, start[0], len(samples))
for n, smp in enumerate(samples):
    for i in range(3):
        d = abs(smp[0][i] - start[i])
        if d > worst:
            worst = d
            where = (n, i, smp[0][i], len(samples))
print("linear joints moved at most %.9f through G53.1 (sample %d of %d, joint %d at %.9f); ended at %s"
      % ((worst,) + (where[0], where[3], where[1], where[2]) + (" ".join("%.9f" % v for v in after[:3]),)))
if worst > 1e-6:
    error("G53.1 moved a linear joint")
if not close(tool_axis(after), list(R[:, 2]), 1e-6):
    error("the tool axis after G53.1 is not the plane normal")
joint_line("G53.1", samples, start, after)

# P names the pose rather than its rank: P1 is the one with the secondary
# rotary positive and P2 the one with it negative, from wherever the
# machine is standing, while no P is the nearest and so does depend on it
poses = {}
for where in ("G0 A0 B0 C0", "G0 A0 B-40 C170"):
    for word in ("", "P1", "P2"):
        mdi("G69")
        mdi(where)
        mdi("G68.2 P1 Q123 I30 J20 K0")
        got = mdi("G53.1 %s" % word)
        poses.setdefault(word, []).append(got)
        drain()
for word, sign in (("P1", 1), ("P2", -1)):
    a, b = poses[word]
    rot = lambda j: [j[TABLE], j[SECONDARY], j[PRIMARY]]
    if max(abs(wrap(x - y)) for x, y in zip(rot(a), rot(b))) > 1e-4:
        error("G53.1 %s landed differently from two starting poses: %s and %s"
              % (word, rot(a), rot(b)))
    if sign * a[SECONDARY] <= 0:
        error("G53.1 %s put the secondary rotary at %.4f" % (word, a[SECONDARY]))
if abs(poses["P1"][0][SECONDARY] - poses["P2"][0][SECONDARY]) < 1e-6:
    error("G53.1 P1 and P2 chose the same pose")
if abs(poses[""][0][SECONDARY] - poses[""][1][SECONDARY]) < 1e-6:
    error("G53.1 with no P gave the same pose from both starts, so it is not the nearest")
print("G53.1 P1 %.4f, P2 %.4f, no P %.4f then %.4f (secondary rotary)"
      % (poses["P1"][0][SECONDARY], poses["P2"][0][SECONDARY],
         poses[""][0][SECONDARY], poses[""][1][SECONDARY]))
c.mdi("G53.1 P3")
c.wait_complete(30)
m = e.poll()
if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
    error("G53.1 P3 was accepted")
drain()
mdi("G69")
mdi("G0 X0 Y0 Z0 A0 B0 C0")
mdi("G68.2 P1 Q123 I30 J20 K0")
after = mdi("G53.1")

# --- moves in the plane go along the plane's axes in the world ----------
# G53.1 swung the tool tip, so it is somewhere in the plane; a move to X10
# travels along the plane's X by the difference
def in_plane():
    s.poll()
    return R.T.dot(np.array(s.position[:3]) - np.array(s.g68_offset[:3])), list(s.position[:3])
q0, p0 = in_plane()
mdi("G0 X10")
q1, p1 = in_plane()
d = [p1[i] - p0[i] for i in range(3)]
want = list((10 - q0[0]) * R[:, 0])
if not close(d, want, 1e-3) or abs(q1[0] - 10) > 1e-3:
    error("G0 X10 in the plane moved the tool by %s, expected %s" % (d, want))
mdi("G0 Z5")
q2, p2 = in_plane()
d = [p2[i] - p1[i] for i in range(3)]
want = list((5 - q1[2]) * R[:, 2])
if not close(d, want, 1e-3) or abs(q2[2] - 5) > 1e-3:
    error("G0 Z5 in the plane moved the tool by %s, expected %s" % (d, want))

# --- G53.6 keeps the tool centre point --------------------------------
mdi("G69")
R2 = rot_y(20).dot(rot_x(-30))
mdi("G68.2 P1 Q123 I-30 J20 K0")
s.poll(); before = list(s.position)
after, samples = sampled("G53.6")
show("G53.6", after)
drain()
worst = max(abs(smp[1][i] - before[i]) for smp in samples for i in range(3))
print("tool tip moved at most %.6f through G53.6" % worst)
if worst > 1e-3:
    error("G53.6 moved the tool tip")
if not close(tool_axis(after), list(R2[:, 2]), 1e-6):
    error("the tool axis after G53.6 is not the plane normal")

# --- G53.2 solves without moving, the pose lands on the parameters ----
# same plane as G53.6 above, but from a pose that is not the answer:
# G53.2 must not move, and the pose it publishes must put the tool on the
# plane normal, which is where G53.6 already stands, so the nearest
# solution is the present rotary position
stay = after
after2, samples = sampled("G53.2")
drain()
if not close(after2, stay, 1e-9):
    error("G53.2 moved the machine: %s became %s" % (stay, after2))
# read the pose back through the parameters: the rotaries come in the
# order the type orients with, the A table, then B and C, and a move to
# them is a move to the present B and C, with the table held
before3 = mdi("G0 B0 C0")
after3, samples = sampled("G0 B#<_orient_rot2> C#<_orient_rot3>")
show("G0 to #<_orient_rot2/rot3>", after3)
drain()
if abs(wrap(after3[SECONDARY] - stay[SECONDARY])) > 1e-3 or abs(wrap(after3[PRIMARY] - stay[PRIMARY])) > 1e-3:
    error("#<_orient_rot2> #<_orient_rot3> held (%.4f, %.4f), G53.6 had reached (%.4f, %.4f)"
          % (after3[SECONDARY], after3[PRIMARY], stay[SECONDARY], stay[PRIMARY]))
if not close(tool_axis(after3), list(R2[:, 2]), 1e-6):
    error("the tool axis at the pose G53.2 published is not the plane normal")
# the numbered parameters carry the same pose
before4 = mdi("G0 B0 C0")
after4, samples = sampled("G0 B#5075 C#5076")
show("G0 to #5075/#5076", after4)
drain()
if abs(wrap(after4[SECONDARY] - stay[SECONDARY])) > 1e-3 or abs(wrap(after4[PRIMARY] - stay[PRIMARY])) > 1e-3:
    error("#5075 #5076 held (%.4f, %.4f), G53.6 had reached (%.4f, %.4f)"
          % (after4[SECONDARY], after4[PRIMARY], stay[SECONDARY], stay[PRIMARY]))
c.mdi("#5075 = 0")
c.wait_complete(30)
m = e.poll()
if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
    error("writing #5075 was accepted; the G53.2 pose is not read-only")
drain()
# the axes the type orients with, and the first of them, the table, where
# it stands; the numbered parameters past the rotaries read 0
c.mdi("(DEBUG,#<_kins_orient_1> #<_kins_orient_2> #<_kins_orient_3>"
      " #<_kins_orient_1_head> #<_kins_orient_2_head> #<_kins_orient_3_head>"
      " #<_orient_rot1> #5074 #5077 #5078 #5079)")
c.wait_complete(30)
deadline = time.time() + 5
said = None
while said is None and time.time() < deadline:
    m = e.poll()
    if not m:
        time.sleep(0.01)
    elif m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
        error("reading the orienting axes after G53.2: %s" % m[1])
        break
    elif m[0] == linuxcnc.OPERATOR_DISPLAY:
        said = [float(v) for v in m[1].split()]
want = [3, 4, 5, 0, 1, 1, stay[TABLE], stay[TABLE], 0, 0, 0]
if said is None or len(said) != len(want) or not close(said, want, 1e-3):
    error("the orienting axes, rot1, #5074 and #5077-#5079 read %s, not %s" % (said, want))
drain()

# --- the orientation stays inside the rotary travel --------------------
# the primary rotary C travels -320 to 320: standing at C300, a plane whose
# nearest pose puts C on the turn past 320 is reached the long way round or
# by the other pose, whichever is nearer inside the travel, and never by
# running C out of it
C_MIN, C_MAX, B_MIN, B_MAX = -320.0, 320.0, -185.0, 185.0

def turn_near(v, now, lo=None, hi=None):
    turns = math.floor((now - v) / 360.0 + 0.5)
    if lo is not None:
        turns = min(max(turns, math.ceil((lo - v) / 360.0)), math.floor((hi - v) / 360.0))
    return v + 360.0 * turns

def pose_near(pairs, b_now, c_now, limited):
    best = None
    for b, cc in pairs:
        if limited:
            bb, cc = turn_near(b, b_now, B_MIN, B_MAX), turn_near(cc, c_now, C_MIN, C_MAX)
        else:
            bb, cc = turn_near(b, b_now), turn_near(cc, c_now)
        d = abs(bb - b_now) + abs(cc - c_now)
        if best is None or d < best[0]:
            best = (d, bb, cc)
    return best[1], best[2]

mdi("G69")
stand = mdi("G0 A0 B0 C300")
plane = None
for i in range(-60, 61, 5):
    for j in range(-60, 61, 5):
        z = list(rot_y(j).dot(rot_x(i))[:, 2])
        pairs = oracle_pairs(z)
        if pairs and pose_near(pairs, stand[SECONDARY], stand[PRIMARY], False)[1] > C_MAX:
            plane = (i, j, z, pose_near(pairs, stand[SECONDARY], stand[PRIMARY], True))
            break
    if plane:
        break
if not plane:
    error("no plane on the grid puts the nearest C past the travel")
else:
    i, j, z, want = plane
    mdi("G68.2 P1 Q123 I%d J%d K0" % (i, j))
    after, samples = sampled("G53.6")
    show("G53.6 at the C limit", after)
    drain()
    top = max(smp[0][PRIMARY] for smp in samples)
    bottom = min(smp[0][PRIMARY] for smp in samples)
    print("plane I%d J%d from C300: C went %.4f to %.4f, the nearest pose inside the travel is B %.4f C %.4f"
          % (i, j, bottom, top, want[0], want[1]))
    if top > C_MAX + 1e-6 or bottom < C_MIN - 1e-6:
        error("G53.6 ran C out of its travel")
    if abs(after[SECONDARY] - want[0]) > 1e-3 or abs(after[PRIMARY] - want[1]) > 1e-3:
        error("G53.6 ended at B %.4f C %.4f, not the nearest pose inside the travel"
              % (after[SECONDARY], after[PRIMARY]))
    if not close(tool_axis(after), z, 1e-6):
        error("the tool axis after G53.6 at the C limit is not the plane normal")

# --- G53.3 goes to a point in the plane with the tool oriented ----------
before = mdi("G69")
R3 = rot_y(-25).dot(rot_x(35))
mdi("G68.2 P1 Q123 I35 J-25 K0")
after, samples = sampled("G53.3 X5 Y5 Z5")
show("G53.3", after)
drain()
joint_line("G53.3", samples, before, after)
s.poll()
prog = R3.T.dot(np.array(s.position[:3]) - np.array(s.g68_offset[:3]))
if not close(list(prog), [5, 5, 5], 1e-3):
    error("G53.3 ended at %s in the plane, not 5 5 5" % list(prog))
if not close(tool_axis(after), list(R3[:, 2]), 1e-6):
    error("the tool axis after G53.3 is not the plane normal")

# --- G68.3 reads the plane back off the tool ---------------------------
mdi("G69", "G68.3 X1 Y2 Z3")
drain()
s.poll()
x, y, z = plane_axes()
if not s.g68_active or not close(list(s.g68_offset[:3]), [1, 2, 3], 1e-9):
    error("G68.3 did not set the origin asked for")
if not close(z, list(R3[:, 2]), 1e-6):
    error("G68.3's normal %s is not the tool axis %s" % (z, list(R3[:, 2])))
if abs(x[2]) > 1e-6:
    error("G68.3's X is not parallel to the machine XY plane: %s" % x)
mdi("G69", "G68.3 R90")
xr, yr, zr = plane_axes()
if not close(xr, y, 1e-6):
    error("G68.3 R90 did not turn the plane about its normal")

# the words a plane code does not use are refused, not ignored
for cmd in ("G68.3 I1 J0 K0", "G68.3 P1", "G68.3 Q123", "G69 R45", "G69 I1", "G69 P1"):
    mdi("G69")
    c.mdi(cmd)
    c.wait_complete(30)
    m = e.poll()
    if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
        error("%s was accepted" % cmd)
    else:
        print("refused as expected:", m[1])
    c.mode(linuxcnc.MODE_MDI)

# --- Q1 lets the table take part ---------------------------------------
mdi("G69", "G0 X0 Y0 Z0 A0 B0 C0")
R4 = rot_x(30)
mdi("G68.2 P1 Q123 I30 J0 K0")
after = mdi("G53.1 Q1")
show("G53.1 Q1", after)
drain()
if not close(tool_axis(after), list(R4[:, 2]), 1e-6):
    error("the tool axis after G53.1 Q1 is not the plane normal")
# with the plane X requested as well and the table free, three joints
# place three constraints: the plane's X is reached by the machine rather
# than by the frame
m = twp.kins_calc_transformation_matrix(math.radians(after[PRIMARY]),
                                        math.radians(after[SECONDARY]), 0, I4, 'inv')
a = math.radians(after[TABLE])
W = np.array([[1, 0, 0], [0, math.cos(a), math.sin(a)], [0, -math.sin(a), math.cos(a)]])
xm = np.array([m[0, 0], m[1, 0], m[2, 0]])
if not close(list(W.T.dot(xm)), list(R4[:, 0]), 1e-6):
    error("with Q1 the machine did not place the plane's X")

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
# into it at 700 mm/s^2, which over one 1 ms cycle is 3.5e-4 short of it,
# and it never goes past it
peak = max(j[2] for j, p in samples)
if peak > 3 + 1e-6 or peak < 3 - 1e-3:
    error("G30.5 Z3 did not pass through the Z3 slide (joint 2 peaked at %.6f)" % peak)
after = mdi("G30.5")
if not close(after, [10, 20, -5, 0, 10, 0], 1e-6):
    error("G30.5 put the joints at %s, not at the stored slides with B10" % (after,))
mdi("G0 X0 Y0 Z0 A0 B0 C0")

# a point-to-point feed takes the time the straight move would: 10 mm at
# F600 is one second, and F30 in G93 is two
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
        elif first is not None and s.inpos and not s.queue and time.time() - last > 0.3:
            break
        time.sleep(0.005)
    c.wait_complete(60)
    settled()
    return (last - first) if first else 0.0
mdi("G0 X0 Y0 Z0 A0 B0 C0")
took = timed("G53.4 G1 X10 F600")
print("G53.4 G1 X10 F600 took %.3f s" % took)
if not 0.7 < took < 1.5:
    error("a 10 mm point-to-point feed at F600 took %.3f s, not about one" % took)
took = timed("G93 G53.4 G1 X0 F30")
mdi("G94")
print("G93 G53.4 G1 X0 F30 took %.3f s" % took)
if not 1.6 < took < 2.6:
    error("a point-to-point feed at G93 F30 took %.3f s, not about two" % took)
drain()

# what the point-to-point codes refuse, and what MACHINE_MOVES_NEED_MACHINE_FRAME
# refuses while the kinematics is not the identity: G53, G28, G30 and the
# stores; the slide forms are the way to the machine's positions from here
for cmd in ("G53.4 G2 X1 I1", "G91 G53.7 G0 J0=1", "G53.7 G0 J9=1", "G53.7 G0 X1",
            "G53.7 G0 J1", "G53.7 G0", "G0 J0=1", "G53.7 G0 J0.5=1", "G53.7 G0 J0=1 J0=2",
            "G53.5 G0 J0=1", "G53.5 G0", "G91 G53.5 G0 X1", "G53.4 G1 F0 X1",
            "G91 G28.5 X1", "G30.5 G1 X1",
            "G53 G0 X0", "G28", "G30 Z1", "G28.1", "G30.1"):
    c.mdi(cmd)
    c.wait_complete(30)
    m = e.poll()
    if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
        error("%s was accepted" % cmd)
    else:
        print("refused as expected:", m[1])
    c.mode(linuxcnc.MODE_MDI)
mdi("G90 G94 G0 X0 Y0 Z0 A0 B0 C0")
# and on the identity kinematics the same lines are accepted
mdi("G12.1 P0", "G53 G0 X0", "G28", "G30 Z1", "G28.1", "G30.1", "G12.1 P1")
m = e.poll()
if m:
    error("on the identity kinematics %s" % (m[1],))

# --- a plane refuses what would move the ground under it ---------------
# each refusal is an interpreter error, and the abort that follows cancels
# the plane, so it is defined afresh before every one
for cmd in ("G92 X1", "G55", "G10 L2 P1 X1"):
    mdi("G68.2 P1 Q123 I30 J0 K0")
    c.mdi(cmd)
    c.wait_complete(30)
    m = e.poll()
    if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
        error("%s was accepted while a plane is active" % cmd)
    else:
        print("refused as expected:", m[1])
    c.mode(linuxcnc.MODE_MDI)
    s.poll()
    if s.g68_active:
        error("the abort after %s left the plane active" % cmd)
mdi("G69")

# --- stopping a program that has a plane ---------------------------------
# The read ahead runs the program's own G69 long before the machine gets
# there, so the cancel is sitting in the queue when the stop button throws
# the queue away.  The plane in status has to end up cancelled all the same,
# and a G69 typed afterwards has to be able to say so again.
c.mode(linuxcnc.MODE_AUTO)
c.wait_complete(30)
c.program_open("abort.ngc")
c.auto(linuxcnc.AUTO_RUN, 0)
deadline = time.time() + 30
while time.time() < deadline:
    s.poll()
    if s.g68_active and s.current_line >= 8:
        break
    time.sleep(0.02)
s.poll()
if not s.g68_active:
    error("the program never reported a plane to stop in the middle of")
c.abort()
c.wait_complete(30)
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
drain()
s.poll()
if s.g68_active:
    error("stopping the program left the plane active in status")
if not close(list(s.g68_rotation), [1, 0, 0, 0, 1, 0, 0, 0, 1], 1e-12):
    error("stopping the program left a rotation in status")
mdi("G69")
s.poll()
if s.g68_active:
    error("G69 after the stop did not clear the plane")
print("a stopped program leaves no plane behind")
mdi("G0 X0 Y0 Z0 A0 B0 C0")
drain()

for f in ("sim.var", "sim.var.bak"):
    try:
        os.unlink(f)
    except OSError:
        pass

overruns = int(hal.get_value("sampler.0.overruns"))
if overruns:
    error("the sampler lost %d samples" % overruns)
if not errors:
    os.unlink(LOG)
print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
