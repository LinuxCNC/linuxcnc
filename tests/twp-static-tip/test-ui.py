#!/usr/bin/env python3
# G53.6 under the machine frame type, the tip held all along: see README.

import os
import sys
import hal
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "twp-static"))
from static import *

# G53.6 under G13.1 keeps the tool tip on the part the whole way, not only
# at its ends, as under the part's type: the slides swing round the tip
# while the rotaries turn, on the joints the part's type solves, and G13.1
# stays in force.  Status part_position, the part's type read from the
# commanded joints, is where the tip is.  Stopped part way, by an abort or
# a feed hold, the tip is still on its point: nothing was switched, so
# nothing has to be switched back.
def tip():
    s.poll()
    return [s.part_position[i] for i in range(3)]

def joints():
    s.poll()
    return [s.joint_position[i] for i in range(JOINTS)]

# The joints between servo cycles come from motion's interpolation, which
# puts the tip off the path by an amount that grows with the square of the
# speed: about 0.2 um at 180 deg/s, 6 um at the sim's full speed.  These
# moves run the rotaries at 180 deg/s, through the INI's HAL pins.
ROTARY = [k for k in range(JOINTS) if ini.find("JOINT_%d" % k, "TYPE") == "ANGULAR"]
def rotaries(slow):
    for k, letter in zip(ROTARY, (FIRST, SECOND)):
        for pin, section in (("ini.%d." % k, "JOINT_%d" % k), ("ini.%s." % letter.lower(), "AXIS_" + letter)):
            for name, low in (("max_velocity", 180), ("max_acceleration", 1800)):
                hal.set_p(pin + name, str(low if slow else ini.find(section, name.upper())))
    # task takes the pins up once a cycle
    s.poll()
    beat = s.taskbeat + 2
    while s.taskbeat < beat:
        time.sleep(0.002)
        s.poll()

def turn_from(j0, j1):
    # how far along from j0 to j1 the joints are, by the one moving most
    k = max(range(JOINTS), key=lambda i: abs(j1[i] - j0[i]))
    return lambda j: (j[k] - j0[k]) / (j1[k] - j0[k])

def watch(until, t0, act=None, at=0.3, rest=lambda: s.inpos and not s.queue):
    # the tip's worst distance from t0 until the move is at rest, and what
    # act() did once the move is at least 'at' of the way along
    worst, middle, acted = 0.0, 0, None
    deadline = time.time() + 30
    while time.time() < deadline:
        s.poll()
        p = [s.part_position[i] for i in range(3)]
        worst = max(worst, max(abs(a - b) for a, b in zip(p, t0)))
        f = until([s.joint_position[i] for i in range(JOINTS)])
        if 0.1 < f < 0.9:
            middle += 1
        if act and acted is None and f >= at:
            acted = act()
        if rest() and (acted is not None or act is None) and f > 0.0:
            break
        time.sleep(0.002)
    return worst, middle, acted

tip_setup = ["G43 H1", plane, "G0 X0 Y0 Z30"]
want = run(TCP, tip_setup + ["G53.6"])[-1]
start = run("G13.1", tip_setup)[-1]
held_type = s.kinematics_type
t0 = tip()
along = turn_from(start, want)
rotaries(True)
c.mdi("G53.6")
worst, middle, _ = watch(along, t0)
got = settled()
rotaries(False)
drain()
print("G53.6 under G13.1: the tip within %.6f of its point over %d samples part way" % (worst, middle))
if middle < 5:
    error("G53.6 under G13.1 was not seen part way: %d samples" % middle)
if worst > 1e-3:
    error("G53.6 under G13.1 took the tip %.4f off its point" % worst)
if not same(got, want):
    error("G53.6 under G13.1 ended at %s, the part's type at %s" % (show(got), show(want)))
if s.kinematics_type != held_type:
    error("G53.6 under G13.1 left kinematics type %d in force, not %d" % (s.kinematics_type, held_type))
# again where it stands: nothing to turn, no move, and no error
c.mdi("G53.6")
ended = done()
again = settled()
drain()
if ended != linuxcnc.RCS_DONE or not same(again, got):
    error("G53.6 under G13.1 again where it stood: %s, from %s to %s" % (ended, show(got), show(again)))

# an abort part way: the tip on its point, the rotaries part way round,
# G13.1 in force, and G53.6 again from there ends where the whole move does
def abort():
    c.abort()
    return True
start = run("G13.1", tip_setup)[-1]
rotaries(True)
c.mdi("G53.6")
worst, middle, _ = watch(along, t0, abort)
stopped = settled()
rotaries(False)
drain(expect_error=True)
f = along(stopped)
print("G53.6 under G13.1 aborted %.0f%% of the way: the tip within %.6f" % (100 * f, worst))
if not 0.05 < f < 0.95:
    error("the abort did not stop G53.6 part way: %.2f of the way" % f)
if worst > 1e-3 or max(abs(a - b) for a, b in zip(tip(), t0)) > 1e-3:
    error("G53.6 aborted part way took the tip %.4f off its point" % worst)
if s.kinematics_type != held_type:
    error("the abort left kinematics type %d in force, not %d" % (s.kinematics_type, held_type))
mdi("G69")
mdi(plane)
got = mdi("G53.6")
if max(abs(a - b) for a, b in zip(got, want)) > 1e-5:
    error("G53.6 after the abort ended at %s, the whole move at %s" % (show(got), show(want)))

# a feed hold part way: the joints stop with the tip on its point, and the
# resumed move ends where the whole move does
def hold():
    c.auto(linuxcnc.AUTO_PAUSE)
    return True
start = run("G13.1", tip_setup)[-1]
rotaries(True)
c.mdi("G53.6")
worst, middle, _ = watch(along, t0, hold, rest=lambda: s.paused)
held = still()
f = along(held)
paused = s.paused
c.auto(linuxcnc.AUTO_RESUME)
w2, _, _ = watch(along, t0)
got = settled()
rotaries(False)
drain()
print("G53.6 under G13.1 held %.0f%% of the way: the tip within %.6f" % (100 * f, max(worst, w2)))
if not paused or not 0.05 < f < 0.95:
    error("the feed hold did not stop G53.6 part way: paused %s, %.2f of the way, at %s"
          % (paused, f, show(held)))
if max(worst, w2) > 1e-3:
    error("G53.6 held part way took the tip %.4f off its point" % max(worst, w2))
if not same(got, want):
    error("G53.6 resumed after a hold ended at %s, the whole move at %s" % (show(got), show(want)))
mdi("G49")

# The joints' travel is followed along the move on the joints the part's
# type solves: a slide that swings past its limit part way round, with both
# ends inside, refuses G53.6 under G13.1 as under the part's type, and the
# machine does not move.  The tool turns from one side of the vertical to
# the other, so the slides peak half way.
swing_setup = ["G43 H1", "G0 X0 Y0 Z30", "G68.2 X0 Y0 Z0 I0 J-30 K0", "G53.6", "G69",
               "G68.2 X0 Y0 Z0 I0 J30 K0"]
start = run("G13.1", swing_setup)[-1]
rotaries(True)
c.mdi("G53.6")
path = []
deadline = time.time() + 30
while time.time() < deadline:
    j = joints()
    path.append(j)
    if s.inpos and not s.queue and not same(j, start):
        break
    time.sleep(0.002)
end = settled()
rotaries(False)
drain()
linear = [k for k in range(JOINTS) if ini.find("JOINT_%d" % k, "TYPE") == "LINEAR"]
bulge, k, side = max((max(p[k] for p in path) - max(start[k], end[k]), k, 1) for k in linear)
low = max((min(start[k], end[k]) - min(p[k] for p in path), k, -1) for k in linear)
if low[0] > bulge:
    bulge, k, side = low
print("G53.6 under G13.1 from %s to %s swings joint %d %.3f past its ends"
      % (show(start), show(end), k, bulge))
if bulge < 0.25:
    error("no slide swings past the ends of G53.6 under G13.1, the travel is not checked along it")
else:
    def set_limit(value):
        c.mode(linuxcnc.MODE_MANUAL)
        done()
        (c.set_max_limit if side > 0 else c.set_min_limit)(k, value)
        done()
        c.mode(linuxcnc.MODE_MDI)
        done()
    s.poll()
    was = s.joint[k]["max_position_limit" if side > 0 else "min_position_limit"]
    for select in (TCP, "G13.1"):
        before = run(select, swing_setup)[-1]
        set_limit(max(start[k], end[k]) + bulge / 2 if side > 0 else min(start[k], end[k]) - bulge / 2)
        c.mdi("G53.6")
        done()
        after = settled()
        if not drain(expect_error=True):
            error("G53.6 under %s that swings joint %d past its limit part way was not refused"
                  % (select, k))
        if not same(before, after):
            error("the refused G53.6 under %s moved the machine from %s to %s"
                  % (select, show(before), show(after)))
        set_limit(was)

finish()
