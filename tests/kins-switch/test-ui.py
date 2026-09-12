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
c.mode(linuxcnc.MODE_AUTO)
c.wait_complete(30)
drain()

# ---- the program, and what the joints do while it runs -------------------

c.program_open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "test.ngc"))
c.wait_complete(30)
c.auto(linuxcnc.AUTO_RUN, 0)

said = []
seen = [kins_type()]
at_switch = None
strayed = [0.0] * JOINTS
w_reached = 0.0
deadline = time.time() + 60
while time.time() < deadline:
    s.poll()
    now = joints()
    k = kins_type()
    if k != seen[-1]:
        seen.append(k)
        if k == 1 and at_switch is None:
            at_switch = now
    if k == 1 and at_switch is not None:
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

# the abort routine's G13.1 fires at estop reset, so the machine may stand
# in identity (1) already when the program starts; either way the program
# itself walks 0, 1, 0, 1
want = [0, 1, 0, 1]
if seen[-4:] != want:
    error("motion.kins-type went %s, not ...%s" % (seen, want))

reported = [m[1].strip() for m in said if m[1].strip().startswith("KINSTYPE=")]
want_reported = ["KINSTYPE=%d.000000" % k for k in want]
if reported != want_reported:
    error("#<_kins_type> reported %s" % (reported,))
else:
    print("#<_kins_type> reported %s" % " ".join(reported))

# ---- a negative kinematics number is refused -----------------------------

c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
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
