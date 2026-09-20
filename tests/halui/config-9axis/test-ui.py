#!/usr/bin/env python3
"""halui on a nine-axis machine (trivkins XYZABCUVW, joint N drives axis N).

Every per-axis halui pin exists here, so this covers selection, jogging,
positions and tool length offsets for A, B, C, U, V and W too.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '_lib'))
from halui_test import run, close  # noqa: E402

import linuxcnc as L  # noqa: E402

LETTERS = 'xyzabcuvw'


def jv(h, j):
    return h.stat.joint[j]['velocity']


def teleop(h):
    h.machine_on()
    h.nml_mode(L.MODE_MANUAL)
    if not all(h.poll().homed[:9]):
        h.nml_home_all()
    if h.stat.motion_mode != L.TRAJ_MODE_TELEOP:
        h.nml_teleop(1)


def pins(h):
    missing = ['axis.%s.%s' % (a, p) for a in LETTERS
               for p in ('is-selected', 'pos-commanded', 'pos-feedback', 'pos-relative',
                         'select', 'plus', 'minus', 'analog', 'increment',
                         'increment-plus', 'increment-minus')
               if not h.has('axis.%s.%s' % (a, p))]
    h.check('every axis has its full set of pins', not missing, 'missing: %s' % missing)
    h.check('joint pins exist for all nine joints',
            all(h.has('joint.%d.is-homed' % j) for j in range(9)))


def select_every_axis(h):
    for i, a in enumerate(LETTERS):
        h.pulse('axis.%s.select' % a)
        h.expect_pin('axis.selected is %d after axis.%s.select' % (i, a), 'axis.selected', i)
        wrong = [b for b in LETTERS if h.get('axis.%s.is-selected' % b) != (a == b)]
        h.check('only axis.%s.is-selected is set' % a, not wrong, 'also set/unset: %s' % wrong)


def jog_every_axis(h):
    teleop(h)
    h.set('axis.jog-speed', 60)
    for i, a in enumerate(LETTERS):
        h.set('axis.%s.plus' % a, 1)
        h.expect('axis.%s.plus jogs joint %d' % (a, i), lambda i=i: close(jv(h, i), 1.0, 1e-3),
                 detail=lambda i=i: 'joint %d velocity %+.4f' % (i, jv(h, i)))
        others = [j for j in range(9) if j != i and not close(jv(h, j), 0.0, 1e-9)]
        h.check('only joint %d moves' % i, not others, 'also moving: %s' % others)
        h.set('axis.%s.plus' % a, 0)
        h.expect('axis %s stops on release' % a,
                 lambda i=i: close(jv(h, i), 0.0, 1e-9) and h.stat.inpos)


def reselect_carries_over(h):
    # On an XYZ machine this is a known bug (see tests/halui/axes): the
    # restart reads ajog_plus[num_axes]. Here num_axes == EMCMOT_MAX_AXIS == 9,
    # which happens to be the right slot, so the jog does carry over.
    teleop(h)
    h.set('axis.jog-speed', 60)
    h.pulse('axis.x.select')
    h.set('axis.selected.plus', 1)
    h.expect('x jogs as the selected axis', lambda: close(jv(h, 0), 1.0, 1e-3))
    h.pulse('axis.y.select')
    h.expect('x stops when the selection moves to y', lambda: close(jv(h, 0), 0.0, 1e-9))
    h.expect('y picks up the selected jog (correct only because num_axes is 9 here)',
             lambda: close(jv(h, 1), 1.0, 1e-3))
    h.set('axis.selected.plus', 0)
    h.expect('y stops on release', lambda: close(jv(h, 1), 0.0, 1e-9) and h.stat.inpos)


def expect_positions(h, machine, relative, why):
    for i, a in enumerate(LETTERS):
        h.expect_pin('axis.%s.pos-commanded is %g %s' % (a, machine[i], why),
                     'axis.%s.pos-commanded' % a, machine[i], tol=1e-6)
        h.expect_pin('axis.%s.pos-feedback is %g %s' % (a, machine[i], why),
                     'axis.%s.pos-feedback' % a, machine[i], tol=1e-6)
        h.expect_pin('axis.%s.pos-relative is %g %s' % (a, relative[i], why),
                     'axis.%s.pos-relative' % a, relative[i], tol=1e-6)


def expect_tool_offsets(h, offsets, why):
    for i, a in enumerate(LETTERS):
        h.expect_pin('tool.length_offset.%s is %g %s' % (a, offsets[i], why),
                     'tool.length_offset.' + a, offsets[i], tol=1e-6)


def positions_and_offsets(h):
    h.machine_on()
    h.nml_home_all()
    for code in ('G20 G90 G54 G49', 'G92.1', 'G10 L2 P1 X0 Y0 Z0 A0 B0 C0 U0 V0 W0 R0'):
        h.nml_mdi(code)

    machine = [i + 2 for i in range(9)]
    h.nml_mdi('G53 G0 X2 Y3 Z4 A5 B6 C7 U8 V9 W10')
    expect_positions(h, machine, machine, 'with no offsets')
    expect_tool_offsets(h, [0] * 9, 'with no tool offset')

    h.nml_mdi('G10 L2 P1 X1 Y2 Z3 A4 B5 C6 U7 V8 W9')
    work = [1 + i for i in range(9)]
    expect_positions(h, machine, [m - w for m, w in zip(machine, work)], 'with a G54 offset on every axis')

    tlo = [round(0.1 * (i + 1), 1) for i in range(9)]
    h.nml_mdi('G43.1 X0.1 Y0.2 Z0.3 A0.4 B0.5 C0.6 U0.7 V0.8 W0.9')
    expect_tool_offsets(h, tlo, 'after G43.1 on every axis')
    expect_positions(h, machine, [m - w - t for m, w, t in zip(machine, work, tlo)],
                     'with G54 and a tool offset on every axis')

    h.nml_mdi('G92 A0 U0 W0')
    s = h.poll()
    for i, a in ((3, 'a'), (6, 'u'), (8, 'w')):
        h.expect_pin('axis.%s.pos-relative is 0 after G92 %s0' % (a, a.upper()),
                     'axis.%s.pos-relative' % a, 0.0, tol=1e-6)
    h.check('setup: G92 left the other axes alone', close(s.g92_offset[4], 0.0))

    for code in ('G92.1', 'G49', 'G10 L2 P1 X0 Y0 Z0 A0 B0 C0 U0 V0 W0'):
        h.nml_mdi(code)


run(pins, select_every_axis, jog_every_axis, reselect_carries_over, positions_and_offsets)
