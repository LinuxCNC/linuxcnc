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
