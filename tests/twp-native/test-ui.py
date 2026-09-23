#!/usr/bin/env python3
# The tilted work plane on the xyzacb nutating-head sim, natively: see README.

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "twp-native"))
from native import *

# --- the plane, and G53.1 with the table held ---------------------------
# no G53.2 has run yet, so the pose parameters must refuse to be read
c.mdi("G0 X#<_orient_x>")
done(30)
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
done(30)
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
done(30)
m = e.poll()
if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
    error("writing #5075 was accepted; the G53.2 pose is not read-only")
drain()
# the axes the type orients with, and the first of them, the table, where
# it stands; the numbered parameters past the rotaries read 0
c.mdi("(DEBUG,#<_kins_orient_1> #<_kins_orient_2> #<_kins_orient_3>"
      " #<_kins_orient_1_head> #<_kins_orient_2_head> #<_kins_orient_3_head>"
      " #<_orient_rot1> #5074 #5077 #5078 #5079)")
done(30)
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
    done(30)
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

# --- a plane refuses what would move the ground under it ---------------
# each refusal is an interpreter error, and the abort that follows cancels
# the plane, so it is defined afresh before every one
for cmd in ("G92 X1", "G52 A1", "G10 L2 P1 X1"):
    mdi("G68.2 P1 Q123 I30 J0 K0")
    c.mdi(cmd)
    done(30)
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
done(30)
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
done(30)
c.mode(linuxcnc.MODE_MDI)
done(30)
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

finish()
