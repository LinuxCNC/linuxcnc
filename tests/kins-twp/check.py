#!/usr/bin/env python3
# The python half of the tilted work plane cross-check.
#
# Imports the machine's remap_funcs_twp.py, the maths the tilted work
# plane remap orients the head with, and drives twpcheck, loaded after
# the kinematics module, to get the module's answers to the same
# questions.  Two comparisons:
#
#   frames    over a grid of primary angle, secondary angle, virtual
#             rotation and table angle, the module's tool frame in
#             machine coordinates against the python transformation
#             matrix Rp * Rs * Rtc
#   inverse   for a set of requested tool axes, with the table held as
#             the remap holds it, the joint angle pairs the module's
#             kinematicsToolFrameInverse() finds against the pairs the
#             python candidate search keeps, and the spin about the tool
#             the module reports for the python's horizontal tool x
#             against the python's own virtual rotation; then with nothing
#             held, where the module may turn the table, each side judged
#             by the other's maths
#
# Usage: check.py MACHINE CONFIGDIR INIFILE
#   MACHINE is xyzacb or xyzbca; CONFIGDIR holds remap_funcs_twp.py;
#   INIFILE is what that file reads its letters and limits from.

import os
import sys
import time
from math import radians, degrees, pi, sin, cos, atan2

import numpy as np
import hal

machine, cfgdir, inifile = sys.argv[1:4]
os.environ["INI_FILE_NAME"] = inifile
sys.path.insert(0, cfgdir)
import remap_funcs_twp as twp

# joint numbers: the table, the secondary and the primary rotary
TABLE, SECONDARY, PRIMARY = {"xyzacb": (3, 4, 5), "xyzbca": (4, 3, 5)}[machine]
PREROT = "%s_trsrn_kins.pre-rot" % machine
TOL = 1e-9
ANGLE_TOL = 1e-6  # degrees

failures = 0
def fail(what):
    global failures
    failures += 1
    print("kins-twp: FAIL %s: %s" % (machine, what))

class Log:
    def debug(self, *a, **k): pass
    def error(self, *a, **k): print("kins-twp: python error:", a[0] % tuple(a[1:]) if len(a) > 1 else a[0])
log = Log()

# ---- driving twpcheck

request = 0
def ask(j, axis=None, xdir=None, held=0):
    """set the joints and the request, wait for the answer"""
    global request
    for i, v in enumerate(j):
        hal.set_p("twpcheck.j-%d" % i, str(v))
    hal.set_p("twpcheck.held", str(held))
    for i, c in enumerate("xyz"):
        hal.set_p("twpcheck.axis-%s" % c, str(axis[i] if axis is not None else 0.0))
        hal.set_p("twpcheck.xdir-%s" % c, str(xdir[i] if xdir is not None else 0.0))
    hal.set_p("twpcheck.have-x", "1" if xdir is not None else "0")
    request += 1
    hal.set_p("twpcheck.request", str(request))
    deadline = time.time() + 5
    while hal.get_value("twpcheck.done") != request:
        if time.time() > deadline:
            print("kins-twp: FAIL twpcheck did not answer")
            sys.exit(1)
        time.sleep(0.002)

def read_matrix(name):
    return np.array([[hal.get_value("twpcheck.%s-%d%d" % (name, r, c)) for c in range(3)]
                     for r in range(3)])

def read_solutions():
    n = hal.get_value("twpcheck.nsol")
    sols = []
    for k in range(max(n, 0)):
        sols.append(([hal.get_value("twpcheck.sol-%d-%d" % (k, i)) for i in range(6)],
                     hal.get_value("twpcheck.spin-%d" % k),
                     hal.get_value("twpcheck.free-%d" % k)))
    return n, sols

def joints_at(table, secondary, primary):
    j = [10.0, 20.0, 30.0, 0.0, 0.0, 0.0]
    j[TABLE], j[SECONDARY], j[PRIMARY] = table, secondary, primary
    return j

# ---- the python's answers

def py_matrix(primary_deg, secondary_deg, tc):
    m = twp.kins_calc_transformation_matrix(radians(primary_deg), radians(secondary_deg), tc,
                                            np.asmatrix(np.identity(4)), 'inv')
    return np.array(m)[:3, :3]

def py_pairs(z):
    """the (primary, secondary) pairs in degrees the remap would keep for a
    tool axis, following remap.py: every combination of the candidate
    lists, kept where it reaches the axis"""
    t1, t2 = twp.kins_calc_possible_joint_angles(log, np.array(z), None)
    if t1 is None or t2 is None:
        return []
    pairs = []
    for a in set(t1):
        for b in set(t2):
            m = py_matrix(degrees(a), degrees(b), 0.0)
            if np.allclose(m[:, 2], z, atol=1e-6):
                pairs.append((degrees(a), degrees(b)))
    return pairs

def same_angle(a, b):
    d = (a - b + 180.0) % 360.0 - 180.0
    return abs(d) < ANGLE_TOL

def same_pair(p, q):
    return same_angle(p[0], q[0]) and same_angle(p[1], q[1])

def fmt(m):
    return np.array2string(m, precision=6, suppress_small=True)

# ---- frames
#
# The module's frame is the head's rotation from its joints alone, so it
# is compared with the python matrix at zero virtual rotation; whether the
# frame should carry the virtual rotation too is a convention question the
# test does not settle.

frames = 0
hal.set_p(PREROT, "0")
for table in (0.0, 20.0):
    for primary in (0.0, 30.0, -25.0, 90.0, 180.0, -135.0):
        for secondary in (0.0, 30.0, -25.0, 90.0, -90.0, 180.0):
            ask(joints_at(table, secondary, primary))
            if hal.get_value("twpcheck.frame-rc") != 0:
                fail("no frame at primary %g secondary %g" % (primary, secondary))
                continue
            tool = read_matrix("tool")
            want = py_matrix(primary, secondary, 0.0)
            frames += 1
            if not np.allclose(tool, want, atol=TOL):
                fail("tool frame differs at primary %g secondary %g table %g\n module:\n%s\n python:\n%s"
                     % (primary, secondary, table, fmt(tool), fmt(want)))

# ---- inverse, the table held
#
# The remap holds the table and orients the head, so ask the module the
# same: the joint pairs must then be the python's, and the spin about the
# tool for the python's horizontal tool x must be the python's virtual
# rotation.

def rz(a):
    return np.array([[cos(a), -sin(a), 0.0], [sin(a), cos(a), 0.0], [0.0, 0.0, 1.0]])

def frames_at(j):
    ask(j)
    return read_matrix("work"), read_matrix("tool")

def in_work(work, tool):
    return work.T @ tool

HOLD_TABLE = 1 << TABLE
REQUESTS = ((30.0, 30.0), (-25.0, 60.0), (120.0, -45.0), (180.0, 90.0),
            (0.0, 0.0), (90.0, 135.0), (45.0, 170.0), (-100.0, -20.0))

requests = 0
for primary, secondary in REQUESTS:
    z = py_matrix(primary, secondary, 0.0)[:, 2]
    pairs = py_pairs(list(z))
    seed = joints_at(0.0, 0.0, 0.0)
    where = "axis %s (from primary %g secondary %g)" % (fmt(z), primary, secondary)
    if not any(same_pair(p, (primary, secondary)) for p in pairs):
        fail("the python does not find the pair (%g, %g) the axis was made from" % (primary, secondary))

    ask(seed, axis=z, held=HOLD_TABLE)
    n, sols = read_solutions()
    requests += 1
    if n <= 0:
        fail("with the table held, the module finds no solution for " + where)
        continue
    found = [(s[0][PRIMARY], s[0][SECONDARY]) for s in sols]
    for s in sols:
        j, spin, free = s
        if abs(j[TABLE] - seed[TABLE]) > 1e-12:
            fail("the held table moved for " + where)
        if free != 0 and (primary, secondary) != (0.0, 0.0):
            fail("with the table held a solution is still a family for " + where)
    for p in pairs:
        if not any(same_pair(p, f) for f in found):
            fail("python pair (%.6f, %.6f) not among the module's %s for %s"
                 % (p[0], p[1], ["(%.6f, %.6f)" % f for f in found], where))
    for f in found:
        if not any(same_pair(p, f) for p in pairs):
            fail("module pair (%.6f, %.6f) not among the python's %s for %s"
                 % (f[0], f[1], ["(%.6f, %.6f)" % p for p in pairs], where))

    # tool x as the python's virtual rotation places it, horizontal: the
    # module, holding the table, must answer the same pair with that spin
    for p in pairs:
        tc = twp.kins_calc_virtual_rot_for_g683(radians(p[0]), radians(p[1]))
        full = py_matrix(p[0], p[1], tc)
        if abs(full[2, 0]) > 1e-9:
            fail("python virtual rotation %g leaves tool x off horizontal for pair (%.6f, %.6f)" % (tc, p[0], p[1]))
        ask(seed, axis=z, xdir=full[:, 0], held=HOLD_TABLE)
        n, sols = read_solutions()
        requests += 1
        match = [s for s in sols if same_pair((s[0][PRIMARY], s[0][SECONDARY]), p)]
        if not match:
            fail("with tool x given and the table held, pair (%.6f, %.6f) is gone from the module's answers" % p)
            continue
        spin = match[0][1]
        if abs((spin - tc + pi) % (2 * pi) - pi) > 1e-6:
            fail("module spin %.9f and python virtual rotation %.9f differ for pair (%.6f, %.6f)"
                 % (spin, tc, p[0], p[1]))

# ---- inverse, nothing held
#
# The module may now turn the table, since it turns the tool against the
# work as surely as the head does, and reports one member of the family
# that results.  Not the python's answer, so each is judged by the other's
# maths: a module solution must reach the axis through the python head
# matrix composed with the module's table frame, and with tool x given it
# must reach the whole frame.

for primary, secondary in REQUESTS:
    z = py_matrix(primary, secondary, 0.0)[:, 2]
    pairs = py_pairs(list(z))
    seed = joints_at(0.0, 0.0, 0.0)
    where = "axis %s (from primary %g secondary %g)" % (fmt(z), primary, secondary)

    ask(seed, axis=z)
    n, sols = read_solutions()
    requests += 1
    if n <= 0:
        fail("the module finds no solution for " + where)
        continue
    for s in sols:
        j, spin, free = s
        work, tool = frames_at(j)
        if not np.allclose(tool, py_matrix(j[PRIMARY], j[SECONDARY], 0.0), atol=TOL):
            fail("module frame at its own solution differs from the python head matrix for " + where)
        if not np.allclose(in_work(work, py_matrix(j[PRIMARY], j[SECONDARY], 0.0))[:, 2], z, atol=1e-6):
            fail("module solution %s does not reach %s by the python head matrix" % (fmt(np.array(j)), where))
        for i in (0, 1, 2):
            if abs(j[i] - seed[i]) > 1e-9:
                fail("solution moved linear joint %d for %s" % (i, where))

    for p in pairs:
        tc = twp.kins_calc_virtual_rot_for_g683(radians(p[0]), radians(p[1]))
        full = py_matrix(p[0], p[1], tc)
        ask(seed, axis=z, xdir=full[:, 0])
        n, sols = read_solutions()
        requests += 1
        if n <= 0:
            fail("the module finds no solution with tool x given for pair (%.6f, %.6f)" % p)
            continue
        for s in sols:
            j, spin, free = s
            work, tool = frames_at(j)
            achieved = in_work(work, py_matrix(j[PRIMARY], j[SECONDARY], 0.0) @ rz(spin))
            if not np.allclose(achieved, full, atol=1e-6):
                fail("module solution %s spin %.6f does not reach the python frame for pair (%.6f, %.6f)\n achieved:\n%s\n wanted:\n%s"
                     % (fmt(np.array(j)), spin, p[0], p[1], fmt(achieved), fmt(full)))

if failures:
    sys.exit(1)
print("kins-twp: %s agrees, %d frames, %d requests" % (machine, frames, requests))
