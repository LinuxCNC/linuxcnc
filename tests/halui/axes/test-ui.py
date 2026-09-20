#!/usr/bin/env python3
"""halui: axis selection, teleop jogging and axis positions.

Pins: axis.L.select, axis.L.is-selected, axis.selected,
      axis.jog-speed, axis.jog-deadband,
      axis.L.plus, axis.L.minus, axis.L.analog,
      axis.L.increment, axis.L.increment-plus, axis.L.increment-minus,
      axis.selected.plus, axis.selected.minus, axis.selected.increment,
      axis.selected.increment-plus, axis.selected.increment-minus,
      axis.L.pos-commanded, axis.L.pos-feedback, axis.L.pos-relative

The machine is XYZ trivkins, so axis x/y/z is joint 0/1/2 one to one and
axis motion is measured through the joint velocities. Axis jog speed is in
units per minute; halui divides it by 60.
"""

import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '_lib'))
from halui_test import run, close  # noqa: E402

import linuxcnc as L  # noqa: E402

LETTERS = 'xyzabcuvw'
CONFIGURED = 'xyz'


def idx(a):
    return LETTERS.index(a)


def av(h, a):
    return h.stat.joint[idx(a)]['velocity']


def pos(h, a):
    return h.stat.actual_position[idx(a)]


def expect_velocity(h, a, v, why):
    return h.expect('axis %s moves at %+.2f %s' % (a, v, why),
                    lambda: close(av(h, a), v, 1e-3),
                    detail=lambda: 'velocity %+.4f' % av(h, a))


def expect_stopped(h, a, why):
    h.expect('axis %s stops %s' % (a, why), lambda: close(av(h, a), 0.0, 1e-9) and h.stat.inpos,
             detail=lambda: 'velocity %+.4f' % av(h, a))


def expect_not_moving(h, a, why):
    h.expect_steady('axis %s does not move %s' % (a, why),
                    lambda: close(av(h, a), 0.0, 1e-9),
                    detail=lambda: 'velocity %+.4f' % av(h, a))


def expect_at(h, a, p, why):
    return h.expect('axis %s ends at %+.3f %s' % (a, p, why),
                    lambda: close(pos(h, a), p, 1e-4) and h.stat.inpos,
                    timeout=10, detail=lambda: 'position %+.4f' % pos(h, a))


def teleop(h):
    h.machine_on()
    h.nml_mode(L.MODE_MANUAL)
    if not all(h.poll().homed[:3]):
        h.nml_home_all()
    if h.stat.motion_mode != L.TRAJ_MODE_TELEOP:
        h.nml_teleop(1)


def to_zero(h):
    h.machine_on()
    if not all(h.poll().homed[:3]):
        h.nml_home_all()
    h.nml_mdi('G20 G90 G53 G0 X0 Y0 Z0')
    teleop(h)


def expect_selected(h, a, why):
    h.expect_pin('axis.selected is %d (%s) %s' % (idx(a), a, why), 'axis.selected', idx(a))
    for b in CONFIGURED:
        h.expect_pin('axis.%s.is-selected is %s %s' % (b, a == b, why),
                     'axis.%s.is-selected' % b, a == b)


def pins_and_defaults(h):
    # input pins exist for all nine axes; status pins only for configured ones
    for a in LETTERS:
        for p in ('select', 'plus', 'minus', 'analog', 'increment',
                  'increment-plus', 'increment-minus'):
            if not h.has('axis.%s.%s' % (a, p)):
                h.check('axis.%s.%s exists' % (a, p), False)
        for p in ('is-selected', 'pos-commanded', 'pos-feedback', 'pos-relative'):
            if h.has('axis.%s.%s' % (a, p)) != (a in CONFIGURED):
                h.check('axis.%s.%s exists only if %s is configured' % (a, p, a), False)
    h.check('axis input pins exist for all nine axes', True)
    h.check('axis.jog-speed defaults to 0', close(h.default('axis.jog-speed'), 0.0))
    h.check('axis.jog-deadband defaults to 0.2', close(h.default('axis.jog-deadband'), 0.2))
    expect_selected(h, 'x', 'at startup')


def select(h):
    h.pulse('axis.z.select')
    expect_selected(h, 'z', 'after axis.z.select')
    h.pulse('axis.y.select')
    expect_selected(h, 'y', 'after axis.y.select')
    h.pulse('axis.a.select')
    expect_selected(h, 'y', 'after axis.a.select (A is not configured, ignored)')
    h.set('axis.x.select', 1)
    expect_selected(h, 'x', 'while axis.x.select is held')
    h.pulse('axis.z.select')
    expect_selected(h, 'z', 'after axis.z.select while axis.x.select is still held')
    h.set('axis.x.select', 0)
    h.settle()
    expect_selected(h, 'z', 'after releasing axis.x.select')


def continuous_jog(h):
    to_zero(h)
    h.set('axis.jog-speed', 60)
    for a in CONFIGURED:
        h.set('axis.%s.plus' % a, 1)
        expect_velocity(h, a, 1.0, 'while axis.%s.plus is held at jog-speed 60' % a)
        h.set('axis.%s.plus' % a, 0)
        expect_stopped(h, a, 'when axis.%s.plus is released' % a)
        h.set('axis.%s.minus' % a, 1)
        expect_velocity(h, a, -1.0, 'while axis.%s.minus is held' % a)
        h.set('axis.%s.minus' % a, 0)
        expect_stopped(h, a, 'when axis.%s.minus is released' % a)

    to_zero(h)
    h.set('axis.jog-speed', 60)
    h.set('axis.x.plus', 1)
    expect_velocity(h, 'x', 1.0, 'at jog-speed 60')
    h.set('axis.jog-speed', 120)
    expect_velocity(h, 'x', 2.0, 'after jog-speed changes to 120 mid-jog')
    h.set('axis.x.plus', 0)
    expect_stopped(h, 'x', 'on release')
    h.set('axis.jog-speed', 60)

    # pins for an axis the machine does not have do nothing
    h.set('axis.b.plus', 1)
    for a in CONFIGURED:
        expect_not_moving(h, a, 'while axis.b.plus is held')
    h.set('axis.b.plus', 0)


def jog_is_gated(h):
    to_zero(h)
    h.set('axis.jog-speed', 60)
    h.nml_teleop(0)
    h.set('axis.x.plus', 1)
    expect_not_moving(h, 'x', 'in joint (free) mode')
    h.set('axis.x.plus', 0)

    h.nml_state(L.STATE_OFF)
    h.set('axis.y.plus', 1)
    expect_not_moving(h, 'y', 'when the machine is off')
    h.set('axis.y.plus', 0)


def analog_jog(h):
    to_zero(h)
    h.set('axis.jog-speed', 60)
    h.set('axis.x.analog', 0.5)
    expect_velocity(h, 'x', 0.5, 'at analog 0.5')
    h.set('axis.x.analog', -0.8)
    expect_velocity(h, 'x', -0.8, 'at analog -0.8')
    h.set('axis.x.analog', 0.15)
    expect_stopped(h, 'x', 'at analog 0.15, inside the 0.2 deadband')
    h.set('axis.x.analog', 0.0)
    h.set('axis.jog-deadband', 0.6)
    h.set('axis.y.analog', 0.5)
    expect_not_moving(h, 'y', 'at analog 0.5 with a 0.6 deadband')
    h.set('axis.y.analog', 0.9)
    expect_velocity(h, 'y', 0.9, 'at analog 0.9 with a 0.6 deadband')
    h.set('axis.jog-speed', 30)
    expect_velocity(h, 'y', 0.45, 'at analog 0.9 after jog-speed changes to 30')
    h.set('axis.y.analog', 0.0)
    expect_stopped(h, 'y', 'at analog 0')
    h.set('axis.jog-deadband', 0.2)
    h.set('axis.jog-speed', 60)
    h.set('axis.z.analog', -0.4)
    expect_velocity(h, 'z', -0.4, 'at analog -0.4')
    h.set('axis.z.analog', 0.0)
    expect_stopped(h, 'z', 'at analog 0')


def incremental_jog(h):
    to_zero(h)
    h.set('axis.jog-speed', 60)
    for a in CONFIGURED:
        h.set('axis.%s.increment' % a, 0.25)
        h.pulse('axis.%s.increment-plus' % a)
        expect_at(h, a, 0.25, 'after increment-plus')
        h.pulse('axis.%s.increment-minus' % a)
        expect_at(h, a, 0.0, 'after increment-minus')

    h.pulse('axis.y.select')
    h.set('axis.selected.increment', 0.5)
    h.pulse('axis.selected.increment-plus')
    expect_at(h, 'y', 0.5, 'after axis.selected.increment-plus with y selected')
    h.pulse('axis.selected.increment-minus')
    expect_at(h, 'y', 0.0, 'after axis.selected.increment-minus')
    h.check('the selected increment moved only y',
            close(pos(h, 'x'), 0.0, 1e-4) and close(pos(h, 'z'), 0.0, 1e-4),
            'x=%+.4f z=%+.4f' % (pos(h, 'x'), pos(h, 'z')))


def selected_jog(h):
    to_zero(h)
    h.set('axis.jog-speed', 60)
    h.pulse('axis.x.select')
    h.set('axis.selected.plus', 1)
    expect_velocity(h, 'x', 1.0, 'while axis.selected.plus is held with x selected')
    h.set('axis.selected.plus', 0)
    expect_stopped(h, 'x', 'on release')
    h.set('axis.selected.minus', 1)
    expect_velocity(h, 'x', -1.0, 'while axis.selected.minus is held')
    h.set('axis.selected.minus', 0)
    expect_stopped(h, 'x', 'on release')

    # Changing the selection mid-jog should stop the old axis and start the
    # new one, as it does for joints. For axes the restart looks at
    # ajog_plus[num_axes] / ajog_minus[num_axes] -- the pins of the axis whose
    # index equals the number of configured axes (A on this XYZ machine) --
    # instead of the selected slot ajog_*[EMCMOT_MAX_AXIS].
    h.set('axis.selected.plus', 1)
    expect_velocity(h, 'x', 1.0, 'before the selection changes')
    h.pulse('axis.y.select')
    expect_stopped(h, 'x', 'when the selection moves to y mid-jog')
    h.known_bug('a selected jog does not carry over to the newly selected axis',
                h.stays(lambda: close(av(h, 'y'), 0.0, 1e-9), 0.8),
                'halui.cc check_hal_changes(): axis reselect reads ajog_plus[num_axes] '
                'instead of ajog_plus[EMCMOT_MAX_AXIS]; y should be jogging at +1')
    h.set('axis.selected.plus', 0)
    h.nml_abort()
    to_zero(h)

    # the same index error from the other side: A's pin (inert on this
    # machine) starts a jog on whichever axis gets selected
    h.pulse('axis.x.select')
    h.set('axis.a.plus', 1)
    expect_not_moving(h, 'x', 'while only axis.a.plus is held (A is not configured)')
    h.pulse('axis.z.select')
    h.known_bug('holding axis.a.plus makes a newly selected axis jog',
                h.wait_for(lambda: close(av(h, 'z'), 1.0, 1e-3), 1.0),
                'same ajog_plus[num_axes] index: num_axes is 3 here, which is axis A; '
                'z should stay still')
    h.set('axis.a.plus', 0)
    h.nml_abort()


def expected_relative(s):
    """halui's own formula (modify_hal_pins) from task's offsets."""
    ax, ay, az = s.actual_position[:3]
    g5x, g92, tlo, rot = s.g5x_offset, s.g92_offset, s.tool_offset, s.rotation_xy
    x = ax - g5x[0] - tlo[0]
    y = ay - g5x[1] - tlo[1]
    c, sn = math.cos(-rot * math.pi / 180), math.sin(-rot * math.pi / 180)
    return (x * c - y * sn - g92[0],
            y * c + x * sn - g92[1],
            az - g5x[2] - g92[2] - tlo[2])


def expect_positions(h, machine, relative, why):
    for i, a in enumerate(CONFIGURED):
        h.expect_pin('axis.%s.pos-commanded is %+.3f %s' % (a, machine[i], why),
                     'axis.%s.pos-commanded' % a, machine[i], tol=1e-6)
        h.expect_pin('axis.%s.pos-feedback is %+.3f %s' % (a, machine[i], why),
                     'axis.%s.pos-feedback' % a, machine[i], tol=1e-6)
        h.expect_pin('axis.%s.pos-relative is %+.3f %s' % (a, relative[i], why),
                     'axis.%s.pos-relative' % a, relative[i], tol=1e-6)


def positions(h):
    h.machine_on()
    h.nml_home_all()
    for code in ('G20 G90 G54', 'G92.1', 'G49', 'G10 L2 P1 X0 Y0 Z0 R0', 'M61 Q0',
                 'G53 G0 X1 Y2 Z0.5'):
        h.nml_mdi(code)
    expect_positions(h, (1, 2, 0.5), (1, 2, 0.5), 'with no offsets')

    for code in ('G10 L2 P1 X1 Y2 Z0.5 R30', 'M61 Q1', 'G43', 'G53 G0 X3 Y4 Z1',
                 'G92 X0.5 Y-0.5'):
        h.nml_mdi(code)
    s = h.poll()
    h.check('setup: task has a G54 offset, G92 offset, rotation and tool offset',
            close(s.g5x_offset[0], 1) and close(s.rotation_xy, 30) and close(s.tool_offset[2], 0.5)
            and not close(s.g92_offset[0], 0),
            'g5x=%s g92=%s rot=%s tlo=%s' % (s.g5x_offset[:3], s.g92_offset[:3],
                                             s.rotation_xy, s.tool_offset[:3]))
    rel = expected_relative(s)
    expect_positions(h, (3, 4, 1), rel, 'with G54 + G92 + R30 + G43')
    # G92 X0.5 Y-0.5 defines the current point as X0.5 Y-0.5 in program
    # coordinates, and Z is machine 1 - G54 0.5 - tool 0.5 = 0
    for a, want in (('x', 0.5), ('y', -0.5), ('z', 0.0)):
        h.expect_pin('axis.%s.pos-relative agrees with program coordinates (%+.2f)' % (a, want),
                     'axis.%s.pos-relative' % a, want, tol=1e-6)

    # the pins follow motion
    teleop(h)
    h.set('axis.jog-speed', 60)
    h.set('axis.x.increment', 0.5)
    h.pulse('axis.x.increment-plus')
    expect_at(h, 'x', 3.5, 'after an increment in teleop')
    rel = expected_relative(h.poll())
    expect_positions(h, (3.5, 4, 1), rel, 'after moving X by 0.5')

    for code in ('G92.1', 'G49', 'G10 L2 P1 X0 Y0 Z0 R0', 'M61 Q0'):
        h.nml_mdi(code)


run(pins_and_defaults,
    select,
    continuous_jog,
    jog_is_gated,
    analog_jog,
    incremental_jog,
    selected_jog,
    positions)
