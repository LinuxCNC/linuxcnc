#!/usr/bin/env python3
#
# Checks the D word start angle offset for spindle synchronized motion.
#
# The offset is defined against the spindle index pulse, and the simulated
# spindle encoder resets spindle.0.revs to zero at exactly that pulse.  So the
# angle the axis actually waited for can be read straight off the samples: find
# the index reset, then find the first sample where Z starts to move, and look
# at how far the spindle had turned in between.
#
# Detecting "Z started moving" costs a small, fixed amount of spindle rotation
# (the axis has to accelerate far enough to clear the threshold), so no single
# measurement is exact.  Every check below is therefore a *difference* between
# two passes that carry that same lag, which cancels it.

import linuxcnc
import linuxcnc_util
import hal

import time
import sys
import os

INTERPTIMEOUT = 5    # Max seconds to wait for the interpreter to start
GCODETIMEOUT = 240   # Max seconds for the gcode program to run

PITCH = 2.0          # K/P word of every synchronized move in the program, mm/rev
ZMOVED = 0.02        # mm of Z travel counted as "the axis has been released"
REVTOL = 0.08        # rev, tolerance on a measured angle difference (~29 deg)

# sampler field order, see sample.hal
LINE, MTYPE, REVS, ZPOS, XPOS, INDEX = range(6)

samples = []
failures = []


def fail(msg):
    print("FAIL: {}".format(msg))
    failures.append(msg)


def index_resets(data):
    """Split the samples at each index pulse.

    sim_spindle clears index-enable at the moment it zeroes its position, so a
    falling edge of that pin is the index reset.  Returns a list of sample
    lists, one per synchronized move that waited for an index.
    """
    groups = []
    start = None
    for i in range(1, len(data)):
        if data[i - 1][INDEX] and not data[i][INDEX]:
            if start is not None:
                groups.append(data[start:i])
            start = i
    if start is not None:
        groups.append(data[start:])
    return groups


def release_index(group):
    """Position in the group at which the tool began to move, or None.

    Either axis counts: a G33 pass is released into a Z move, but a G76 pass
    with an entry taper is released into a radial X feed.
    """
    z0, x0 = group[0][ZPOS], group[0][XPOS]
    for i, s in enumerate(group):
        if abs(s[ZPOS] - z0) > ZMOVED or abs(s[XPOS] - x0) > ZMOVED:
            return i
    return None


def release_angle(group):
    """Revolutions past the index at which the tool began to move, or None."""
    i = release_index(group)
    return None if i is None else group[i][REVS]


def cut_samples(group):
    """The synchronized cut itself: from release until Z stops advancing.

    A group runs to the next index pulse, so it also contains the rapids that
    reposition for the following pass.  Cutting it off where Z stops going
    negative keeps those out of the pitch check.
    """
    i = release_index(group)
    if i is None:
        return []
    cut = [group[i]]
    stalled = 0
    for s in group[i + 1:]:
        if s[ZPOS] > cut[-1][ZPOS] + 0.001:   # Z turned round: the cut is over
            break
        if s[ZPOS] > cut[-1][ZPOS] - 1e-9:
            # Z has stopped advancing.  The cut is over even though Z has not
            # moved back yet: the program dwells here, and the line number is
            # no help because a dwell keeps reporting the previous motion line.
            stalled += 1
            if stalled > 20:
                break
        else:
            stalled = 0
        cut.append(s)
    return cut[:len(cut) - stalled]


def cut_pitch(group, z1, z2):
    """Measured mm per revolution between two Z positions of a cut.

    Sampled well inside the thread so that neither the entry taper nor the
    acceleration at either end is included.
    """
    def revs_at(zt):
        prev = None
        for s in group:
            if prev and prev[ZPOS] > zt >= s[ZPOS]:
                span = prev[ZPOS] - s[ZPOS]
                f = (prev[ZPOS] - zt) / span if span else 0.0
                return prev[REVS] + f * (s[REVS] - prev[REVS])
            prev = s
        return None

    a, b = revs_at(z1), revs_at(z2)
    if a is None or b is None or b <= a:
        return None
    return (z1 - z2) / (b - a)


def pitch_error(group):
    """Largest departure from 'Z advances PITCH per revolution' during the cut."""
    cut = cut_samples(group)
    if len(cut) < 20:
        return None
    # drop the acceleration at each end, where Z is not yet tracking the spindle
    cut = cut[len(cut) // 4: -(len(cut) // 4)]
    if len(cut) < 5:
        return None
    zr, rr = cut[0][ZPOS], cut[0][REVS]
    return max(abs((zr - s[ZPOS]) - PITCH * (s[REVS] - rr)) for s in cut)


#
# Command line: -ngc <file>
#
ngcfile = None
for i in range(1, len(sys.argv) - 1):
    if "-ngc" == sys.argv[i]:
        ngcfile = sys.argv[i + 1]
        break

if not ngcfile:
    print("Missing NGC-file; run with: test-ui.py -ngc ngcfile.ngc")
    sys.exit(1)

if not os.path.exists(ngcfile):
    print("NGC-file '{}' does not exist".format(ngcfile))
    sys.exit(1)

#
# Connect to the sampler stream
#
h = hal.component("python-ui")
sampler = hal.stream(h, hal.sampler_base, "ssfffb")
h.ready()

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)

c.home(-1)
c.wait_complete()
l.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])

c.mode(linuxcnc.MODE_AUTO)
c.program_open(ngcfile)

hal.set_p("sampler.0.enable", "1")
c.auto(linuxcnc.AUTO_RUN, 1)

start = time.time()
while time.time() - start < INTERPTIMEOUT:
    s.poll()
    if s.interp_state != linuxcnc.INTERP_IDLE:
        break
    time.sleep(0.01)
if s.interp_state == linuxcnc.INTERP_IDLE:
    print("Timed out starting interpreter")
    sys.exit(1)


def drain():
    while sampler.readable:
        sample = sampler.read()
        if sample is None:
            print("Error: sampler read None")
            sys.exit(1)
        samples.append(sample)


start = time.time()
s.poll()
while s.interp_state != linuxcnc.INTERP_IDLE and time.time() - start < GCODETIMEOUT:
    drain()
    time.sleep(0.005)
    s.poll()

if s.interp_state != linuxcnc.INTERP_IDLE:
    print("Timed out running the GCode program")
    sys.exit(1)

hal.set_p("sampler.0.enable", "0")
time.sleep(0.05)
drain()

print("collected {} samples".format(len(samples)))

#
# Analysis
#
groups = index_resets(samples)
print("{} synchronized moves waited for an index pulse".format(len(groups)))

# The program is three G33 passes (no D, D180, D-180) followed by two G76
# cycles of identical geometry, the second one offset.  The pass count of a G76
# cycle depends on its depth degression, so the two cycles are not assumed to be
# any particular length -- only that they are the same length as each other.
if len(groups) < 5 or (len(groups) - 3) % 2:
    fail("expected 3 G33 passes and two G76 cycles of equal length, "
         "got {} synchronized moves".format(len(groups)))
    print("Test Failed")
    sys.exit(1)

angles = [release_angle(g) for g in groups]
for i, a in enumerate(angles):
    print("move {}: released {} rev past index".format(
        i, "never" if a is None else "{:.4f}".format(a)))

npasses = (len(groups) - 3) // 2
ref, d180, dneg180 = angles[0], angles[1], angles[2]
g76ref, g76d180 = angles[3], angles[3 + npasses]

for i, a in enumerate(angles):
    if a is None:
        fail("move {} never moved after its index pulse".format(i))
if failures:
    print("Test Failed")
    sys.exit(1)

# D180 holds for half a turn longer than no D at all.
if abs((d180 - ref) - 0.5) > REVTOL:
    fail("D180 released {:.4f} rev after the reference pass, expected 0.5"
         .format(d180 - ref))

# A negative D is used as its magnitude, so D-180 matches D180.  This is the
# check that a negative value is neither rejected nor silently dropped: if it
# were dropped this difference would be the full 0.5 rev.
if abs(dneg180 - d180) > REVTOL:
    fail("D-180 released {:.4f} rev past index, D180 released {:.4f}; "
         "a negative D should be used as its magnitude"
         .format(dneg180, d180))

# The reference pass has no offset, so it starts at the index pulse.  Only the
# detection lag should separate it from zero.
if ref > REVTOL:
    fail("pass without a D word released {:.4f} rev past index, expected ~0"
         .format(ref))

# G76 honours D too: the offset cycle waits half a turn longer than the
# otherwise identical reference cycle.
if abs((g76d180 - g76ref) - 0.5) > REVTOL:
    fail("G76 with D180 released {:.4f} rev past index and the reference "
         "cycle {:.4f}, a difference of {:.4f}; expected 0.5"
         .format(g76d180, g76ref, g76d180 - g76ref))

# Whatever the offset did, the cut itself must still advance one pitch per
# revolution -- a hold that released into the wrong tracking state would show
# up here rather than in the angles above.  Only the G33 passes are checked:
# a G76 pass cuts its entry taper at a different pitch by design.
for i, g in enumerate(groups[:3]):
    err = pitch_error(g)
    if err is None:
        continue
    print("move {}: pitch tracking error {:.4f} mm".format(i, err))
    if err > 0.5:
        fail("move {} drifted {:.4f} mm from {} mm/rev".format(i, err, PITCH))

# The same for the body of a G76 pass, measured between two Z positions well
# inside the thread so the entry taper is excluded.  Both cycles must cut the
# programmed pitch: an offset that resynchronised the spindle reference part way
# through a pass, or one that dropped the axis out of position tracking, would
# show up as a pitch that is not P.
for i, g in ((3, groups[3]), (3 + npasses, groups[3 + npasses])):
    measured = cut_pitch(g, -2.0, -8.0)
    if measured is None:
        fail("move {} never cut through the Z range the pitch is measured over"
             .format(i))
        continue
    print("move {}: cut {:.4f} mm/rev".format(i, measured))
    if abs(measured - PITCH) > 0.02:
        fail("move {} cut {:.4f} mm/rev, expected {}".format(i, measured, PITCH))

if failures:
    print("Test Failed")
    sys.exit(1)

print("Completed successfully")
sys.exit(0)
