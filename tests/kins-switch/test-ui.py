#!/usr/bin/env python3
# G12.1 and G13.1 select a kinematics.  The interesting part is what
# happens to the block after the switch, so the program runs in AUTO,
# where the interpreter reads far ahead of the machine.
import hal
import linuxcnc
import os
import sys
import time

JOINTS = 6
CARRIED = (0, 1, 2, 3, 4)   # every joint except W, which is the one asked to move

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

def kins_type():
    return int(round(hal.get_value("motion.kins-type")))

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

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete(30)
c.home(-1)
c.wait_complete(60)
# start in identity every run, the abort's G13.1 may or may not have run yet
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
mdi("G13.1")
c.mode(linuxcnc.MODE_AUTO)
c.wait_complete(30)
drain()

# ---- the program, and what the joints do while it runs -------------------

c.program_open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "test.ngc"))
c.wait_complete(30)
last = kins_type()
c.auto(linuxcnc.AUTO_RUN, 0)

said = []
at_switch = None
holding = False
strayed = [0.0] * JOINTS
w_reached = 0.0
deadline = time.time() + 60
while time.time() < deadline:
    s.poll()
    now = joints()
    k = kins_type()
    # the hold starts at the program's own switch to identity, at its pose
    if k != last:
        last = k
        if k == 1 and at_switch is None:
            at_switch = now
            holding = True
        else:
            holding = False
    # the joints are held over the first stretch in identity, the one the
    # W stroke runs in; later on the program applies a tool length in the
    # tilted pose, which moves the joints on purpose
    if holding:
        for j in CARRIED:
            strayed[j] = max(strayed[j], abs(now[j] - at_switch[j]))
        w_reached = max(w_reached, now[5])
    said += drain()
    if s.interp_state == linuxcnc.INTERP_IDLE and time.time() > deadline - 58:
        break
    time.sleep(0.02)
else:
    error("the program did not finish")

said += drain()

if at_switch is None:
    error("motion.kins-type never reported the selection")
else:
    print("at the switch     %s" % " ".join("%.4f" % v for v in at_switch))
    # a switch between two kinematics that agree would prove nothing;
    # the pose is the programmed one (10, 10, -5), the joints may not be
    if max(abs(at_switch[j] - v) for j, v in zip((0, 1, 2), (10, 10, -5))) < 1:
        error("the two kinematics agree in this pose, so the test is empty")
    for j in CARRIED:
        if strayed[j] > 1e-6:
            error("joint %d moved %.6f while only W was asked for" % (j, strayed[j]))
    if abs(w_reached - 10) > 1e-3:
        error("W reached %.6f, not the 10 the program asks for" % w_reached)
    print("W ran to %.4f, the other joints held to %.2e"
          % (w_reached, max(strayed[j] for j in CARRIED)))

# the program walks 0, 1, 0, 1 and the line after each selection reports
# what the interpreter holds and what motion runs; the pin read there is
# the check that the selection went through before the next line, which
# no sampler on this side could make (a selection undone on the next
# line holds for one servo period)
want = [0, 1, 0, 1]
reported = [m[1].strip() for m in said if m[1].strip().startswith("KINSTYPE=")]
want_reported = ["KINSTYPE=%d.000000 PIN=%d.000000" % (k, k) for k in want]
if reported != want_reported:
    error("the lines after the selections reported %s" % (reported,))
else:
    print("the lines after the selections reported %s" % " | ".join(reported))

# G43.4 switches to the primary kinematics (0) and applies the offset,
# G49 cancels both, and a plain G43 touches the offset only; a G49 that
# cancels a plain G43, or comes after the program selected a kinematics
# itself, leaves the selection alone
g434 = [m[1].strip() for m in said if m[1].strip().startswith("G434")]
want_g434 = ["G434 KINSTYPE=0.000000 PIN=0.000000 TLOZ=12.500000",
             "G434 KINSTYPE=1.000000 PIN=1.000000 TLOZ=0.000000",
             "G434 KINSTYPE=1.000000 PIN=1.000000 TLOZ=12.500000",
             "G434 KINSTYPE=0.000000 PIN=0.000000 TLOZ=0.000000",
             "G434 KINSTYPE=0.000000 PIN=0.000000 TLOZ=0.000000",
             "G434 KINSTYPE=0.000000 PIN=0.000000 TLOZ=12.500000",
             "G434 KINSTYPE=1.000000 PIN=1.000000 TLOZ=0.000000"]
if g434 != want_g434:
    error("G43.4/G49 reported %s" % (g434,))
else:
    print("G43.4 switched to primary with the offset, G49 cancelled both")

# ---- a tool length under a tilt keeps the joints ------------------------
#
# Motion hands the module the offset G43 puts in effect, with nothing
# netted.  The head is tilted, so the length lies along the tool axis
# instead of along Z: the point the joints stand on moves by the tilt
# term of the length, and the joints stay where they are.  The
# interpreter works out the same point ahead of motion, so a move to
# where it thinks the machine is goes nowhere.

import math
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
drain()

def pose():
    s.poll()
    return list(s.position[:3])

def tool_length_check(what, before_joints, before_pose, after_joints, after_pose, want):
    if max(abs(after_joints[j] - before_joints[j]) for j in range(JOINTS)) > 1e-6:
        error("%s moved the joints by %s" % (what,
              " ".join("%.4f" % (after_joints[j] - before_joints[j]) for j in range(JOINTS))))
    got = [after_pose[j] - before_pose[j] for j in (0, 1, 2)]
    if max(abs(g - w) for g, w in zip(got, want)) > 1e-3:
        error("%s moved the point by %s, not %s" % (what,
              " ".join("%.4f" % v for v in got), " ".join("%.4f" % v for v in want)))
    said = [m[1].strip() for m in drain() if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR)]
    if said:
        error("%s raised %s" % (what, said))
    # the interpreter's point: a move of nothing from it goes nowhere
    mdi("G91")
    stayed = mdi("G0 X0 Y0 Z0")
    mdi("G90")
    if max(abs(stayed[j] - after_joints[j]) for j in range(JOINTS)) > 1e-6:
        error("after %s the interpreter has the point elsewhere: a move of nothing moved the joints by %s"
              % (what, " ".join("%.4f" % (stayed[j] - after_joints[j]) for j in range(JOINTS))))

mdi("G12.1 P0")
mdi("G0 X10 Y10 Z-5 B-22.5 C45")
mdi("G49")
errors_before = errors
before = mdi("G0 X10 Y10 Z-5")
before_pose = pose()
after = mdi("G43.4 H1")
pose_after = pose()
L = 12.5                                # tool 1 in tool.tbl
b, cc = math.radians(-22.5), math.radians(45)
# the length along the tool axis, less the length along Z the offset stands for
want = [L * math.sin(math.pi - b) * math.cos(cc),
        L * math.sin(math.pi - b) * math.sin(cc),
        L * (1 + math.cos(math.pi - b))]
tool_length_check("G43.4 in the tilted pose", before, before_pose, after, pose_after, want)
# G49 would drop to identity and hold the joints where they are; a zero
# offset without a switch takes the length back out
back = mdi("G43.1 Z0")
tool_length_check("a zero tool length", after, pose_after, back, pose(), [-w for w in want])
if errors == errors_before:
    print("a tool length under a tilt moved the point, not the joints")

# a point-to-point move ends on joints motion holds while the point
# stays; a tool length moves the point, and the joints are held on
errors_before = errors
p2p = mdi("G53.4 G0 X10 Y10 Z-5 B-22.5 C45")
p2p_pose = pose()
after = mdi("G43.1 Z%g" % L)
tool_length_check("a tool length after a point-to-point move", p2p, p2p_pose, after, pose(), want)
if errors == errors_before:
    print("a tool length after a point-to-point move keeps the joints too")
mdi("G43.1 Z0")
mdi("G49")

# ---- G43.5: the tool axis as a vector ------------------------------------
#
# Under G43.5 a G0 or G1 line gives the direction of the tool axis as I J K
# and the interpreter finds the rotaries.  The head's tool axis at B, C is
# (-sin B cos C, -sin B sin C, cos B), so the vector for the tilted pose
# above has to land on the joints and the point the rotary words reach.

def refused(cmd, needle):
    drain()
    c.mdi(cmd)
    c.wait_complete(30)
    m = e.poll()
    if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
        error("%s was accepted" % cmd)
    elif needle not in m[1]:
        error("%s said %r, nothing about %r" % (cmd, m[1].strip(), needle))
    else:
        print("refused as expected: %s" % m[1].strip())
    drain()

errors_before = errors
mdi("G12.1 P0")
mdi("G0 X0 Y0 Z0 B0 C0")
mdi("G43.5 H1")
s.poll()
if 435 not in s.gcodes:
    error("G43.5 is not among the active G-codes %s" % (s.gcodes,))
by_words = mdi("G0 X10 Y10 Z-5 B-22.5 C45")
by_words_pose = pose()
mdi("G0 X0 Y0 Z0 B0 C0")
vec = (-math.sin(b) * math.cos(cc), -math.sin(b) * math.sin(cc), math.cos(b))
by_vector = mdi("G0 X10 Y10 Z-5 I%.9f J%.9f K%.9f" % vec)
if max(abs(by_vector[j] - by_words[j]) for j in range(JOINTS)) > 1e-5:
    error("the vector put the joints at %s, the rotary words at %s"
          % (" ".join("%.4f" % v for v in by_vector), " ".join("%.4f" % v for v in by_words)))
if max(abs(p - q) for p, q in zip(pose(), by_words_pose)) > 1e-5:
    error("the vector put the point at %s, the rotary words at %s"
          % (" ".join("%.4f" % v for v in pose()), " ".join("%.4f" % v for v in by_words_pose)))
# a direction is not incremental
mdi("G91")
held = mdi("G1 X0 I%.9f J%.9f K%.9f F1000" % vec)
mdi("G90")
if max(abs(held[j] - by_vector[j]) for j in range(JOINTS)) > 1e-6:
    error("the same vector under G91 moved the joints by %s"
          % " ".join("%.4f" % (held[j] - by_vector[j]) for j in range(JOINTS)))
# the pole: the tool vertical leaves C where it is, and the point stays
pole = mdi("G0 K1")
if abs(pole[3]) > 1e-6 or abs(pole[4] - 45) > 1e-3:
    error("the tool vertical put B, C at %.4f, %.4f, not 0, 45" % (pole[3], pole[4]))
if max(abs(p - q) for p, q in zip(pose()[:3], by_words_pose[:3])) > 1e-5:
    error("the tool vertical moved the point to %s" % " ".join("%.4f" % v for v in pose()))
# a rotary offset renames the angles, the direction is the same: the same
# joints, called something else by the program
mdi("G10 L2 P1 C30")
mdi("G0 X0 Y0 Z0 B0 C0")
offset = mdi("G0 X10 Y10 Z-5 I%.9f J%.9f K%.9f" % vec)
mdi("G10 L2 P1 C0")
if max(abs(offset[j] - by_words[j]) for j in range(JOINTS)) > 1e-5:
    error("under a C offset the vector put the joints at %s, not %s"
          % (" ".join("%.4f" % v for v in offset), " ".join("%.4f" % v for v in by_words)))
if errors == errors_before:
    print("G43.5 turned the tool along the vector, onto the joints the rotary words reach")
refused("G0 X0 K1 B5", "twice")
refused("G0 X0 I0 J0 K0", "zero")
mdi("G13.1")
refused("G0 X0 K1", "G12.1 first")
mdi("G43.4 H1")
refused("G0 X0 K1", "K word with no")
mdi("G43.5 H1")
mdi("G49")
refused("G0 X0 K1", "K word with no")

# ---- the axes that orient the tool ---------------------------------------
#
# 5axiskins turns the tool with a head: C with its axis fixed, B carried by
# it.  The interpreter finds them from the tool frame and names them by the
# joint map, as axis numbers; the identity type orients with nothing.  Under
# G43.5 a word of an axis that does not orient, W here, goes with a vector.

def param(name):
    drain()
    c.mdi("(debug,#<%s>)" % name)
    c.wait_complete(30)
    deadline = time.time() + 5
    while time.time() < deadline:
        m = e.poll()
        if not m:
            time.sleep(0.01)
            continue
        if m[0] == linuxcnc.OPERATOR_DISPLAY:
            return float(m[1])
    error("#<%s> gave no value" % name)
    return None

ORIENT = ("_kins_orient_1", "_kins_orient_2", "_kins_orient_3",
          "_kins_orient_1_head", "_kins_orient_2_head", "_kins_orient_3_head")
errors_before = errors
mdi("G12.1 P0")
got = [param(n) for n in ORIENT]
if got != [5, 4, -1, 1, 1, -1]:
    error("on the 5-axis type the orienting axes read %s, not C then B, both heads" % (got,))
mdi("G13.1")
got = [param(n) for n in ORIENT]
if got != [-1, -1, -1, -1, -1, -1]:
    error("on the identity type the orienting axes read %s, not none" % (got,))
mdi("G12.1 P0")
mdi("G0 X0 Y0 Z0 B0 C0 W0")
mdi("G43.5 H1")
with_w = mdi("G0 X0 Y0 Z0 K1 W2")
if abs(with_w[5] - 2) > 1e-6:
    error("a vector with a W word left W at %.4f, not 2" % with_w[5])
mdi("G0 W0")
mdi("G49")
said = drain()
if any(m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR) for m in said):
    error("the orienting axes section reported %s" % (said,))
elif errors == errors_before:
    print("the orienting axes are C then B, heads, and W goes along with a vector")

# ---- a negative kinematics number is refused -----------------------------

drain()
c.mdi("G12.1 P-1")
c.wait_complete(30)
m = e.poll()
if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
    error("G12.1 P-1 was accepted")
elif "non-negative" not in m[1]:
    error("G12.1 P-1 said %r, which does not mention the P word" % m[1].strip())
else:
    print("refused as expected: %s" % m[1].strip())
drain()

# ---- a kinematics the module does not provide is refused at read time ----

c.mdi("G12.1 P7")
c.wait_complete(30)
m = e.poll()
if not m or m[0] not in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
    error("G12.1 P7 was accepted")
elif "does not name" not in m[1]:
    error("G12.1 P7 said %r, which does not name the problem" % m[1].strip())
else:
    print("refused as expected: %s" % m[1].strip())
drain()

# ---- G13.1 in an abort routine does not swallow the rest of it -----------
#
# The routine ends with G54, so leave the machine in G55 and see which one
# comes back.  The switch is a queue synchronisation point everywhere else,
# and asking for one here would end the routine on the spot.

for attempt in range(3):
    mdi("G55")
    s.poll()
    if s.g5x_index != 2:
        continue
    c.abort()
    c.wait_complete(30)
    deadline = time.time() + 10
    while time.time() < deadline:
        s.poll()
        if s.g5x_index == 1:
            break
        time.sleep(0.05)
    s.poll()
    if s.g5x_index != 1:
        error("the abort routine stopped at its G13.1, leaving G%d"
              % (53 + s.g5x_index))
    else:
        print("the abort routine ran past its G13.1")
    break
else:
    error("could not get the machine into G55 to test the abort routine")

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
