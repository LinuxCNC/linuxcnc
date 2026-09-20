#!/usr/bin/env python3
"""halui: joint selection, homing, joint status and joint jogging.

Pins: joint.N.select, joint.N.is-selected, joint.selected,
      joint.N.home, joint.N.unhome, joint.selected.home, joint.selected.unhome,
      home-all, joint.N.is-homed, joint.selected.is-homed,
      joint.N.on-{soft,hard}-{min,max}-limit, joint.N.has-fault,
      joint.N.override-limits and the joint.selected.* mirrors of those,
      joint.jog-speed, joint.jog-deadband,
      joint.N.plus, joint.N.minus, joint.N.analog,
      joint.N.increment, joint.N.increment-plus, joint.N.increment-minus,
      joint.selected.plus, joint.selected.minus, joint.selected.increment,
      joint.selected.increment-plus, joint.selected.increment-minus

Joint jog speed is in units per minute; halui divides it by 60.
"""

import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '_lib'))
from halui_test import run, close  # noqa: E402

import hal  # noqa: E402
import linuxcnc as L  # noqa: E402

JOINTS = 3


def jv(h, j):
    return h.stat.joint[j]['velocity']


def expect_velocity(h, j, v, why):
    return h.expect('joint %d moves at %+.2f %s' % (j, v, why),
                    lambda: close(jv(h, j), v, 1e-3),
                    detail=lambda: 'velocity %+.4f' % jv(h, j))


def expect_stopped(h, j, why):
    # per joint: the global in-position flag stays false while another joint
    # is still moving
    ok = h.wait_for(lambda: close(jv(h, j), 0.0, 1e-9)) and \
        h.stays(lambda: close(jv(h, j), 0.0, 1e-9), 0.2)
    h.check('joint %d stops %s' % (j, why), ok, 'velocity %+.4f' % jv(h, j))


def expect_not_moving(h, j, why):
    h.expect_steady('joint %d does not move %s' % (j, why),
                    lambda: close(jv(h, j), 0.0, 1e-9),
                    detail=lambda: 'velocity %+.4f' % jv(h, j))


def manual_joint_mode(h, homed=False):
    h.machine_on()
    h.nml_mode(L.MODE_MANUAL)
    if homed:
        h.nml_home_all()
    if h.poll().motion_mode != L.TRAJ_MODE_FREE:
        h.nml_teleop(0)


def to_zero(h):
    manual_joint_mode(h)
    h.nml_mdi('G20 G90 G53 G0 X0 Y0 Z0')
    h.nml_mode(L.MODE_MANUAL)
    if h.poll().motion_mode != L.TRAJ_MODE_FREE:
        h.nml_teleop(0)


def expect_selected(h, j, why):
    h.expect_pin('joint.selected is %d %s' % (j, why), 'joint.selected', j)
    for k in range(JOINTS):
        h.expect_pin('joint.%d.is-selected is %s %s' % (k, k == j, why),
                     'joint.%d.is-selected' % k, k == j)


def pins_and_defaults(h):
    for j in range(JOINTS):
        for p in ('select', 'is-selected', 'home', 'unhome', 'is-homed', 'plus', 'minus',
                  'analog', 'increment', 'increment-plus', 'increment-minus',
                  'on-soft-min-limit', 'on-soft-max-limit', 'on-hard-min-limit',
                  'on-hard-max-limit', 'has-fault', 'override-limits'):
            if not h.has('joint.%d.%s' % (j, p)):
                h.check('joint.%d.%s exists' % (j, p), False)
    h.check('there are no pins for a joint the machine does not have',
            not any(s.startswith('joint.%d.' % JOINTS) for s in h.pins))
    h.check('home-all exists because [JOINT_0]HOME_SEQUENCE is set', h.has('home-all'))
    h.check('joint.jog-speed defaults to 0', close(h.default('joint.jog-speed'), 0.0))
    h.check('joint.jog-deadband defaults to 0.2', close(h.default('joint.jog-deadband'), 0.2))
    expect_selected(h, 0, 'at startup')


def select(h):
    h.pulse('joint.2.select')
    expect_selected(h, 2, 'after joint.2.select')
    h.pulse('joint.1.select')
    expect_selected(h, 1, 'after joint.1.select')

    # selection follows the latest rising edge, even while another select is held
    h.set('joint.0.select', 1)
    expect_selected(h, 0, 'while joint.0.select is held')
    h.pulse('joint.2.select')
    expect_selected(h, 2, 'after joint.2.select while joint.0.select is still held')
    h.set('joint.0.select', 0)
    h.settle()
    expect_selected(h, 2, 'after releasing joint.0.select (falling edges do nothing)')


def homing(h):
    manual_joint_mode(h)
    h.nml_unhome_all()
    for j in range(JOINTS):
        h.expect_pin('joint.%d.is-homed is false after unhoming' % j, 'joint.%d.is-homed' % j, False)

    h.pulse('joint.1.home')
    h.expect('joint.1.home homes joint 1', lambda: h.stat.homed[1])
    h.expect_pin('joint.1.is-homed sets', 'joint.1.is-homed', True)
    h.pulse('joint.1.unhome')
    h.expect('joint.1.unhome unhomes joint 1', lambda: not h.stat.homed[1])
    h.expect_pin('joint.1.is-homed clears', 'joint.1.is-homed', False)

    h.pulse('joint.2.select')
    h.expect_pin('joint.selected.is-homed is false for unhomed joint 2',
                 'joint.selected.is-homed', False)
    h.pulse('joint.selected.home')
    h.expect('joint.selected.home homes the selected joint (2)', lambda: h.stat.homed[2])
    h.expect_pin('joint.selected.is-homed follows joint 2', 'joint.selected.is-homed', True)
    h.pulse('joint.selected.unhome')
    h.expect('joint.selected.unhome unhomes the selected joint (2)', lambda: not h.stat.homed[2])
    h.expect_pin('joint.selected.is-homed clears', 'joint.selected.is-homed', False)

    h.pulse('home-all')
    h.expect('home-all homes every joint', lambda: all(h.stat.homed[:JOINTS]), timeout=20)
    for j in range(JOINTS):
        h.expect_pin('joint.%d.is-homed is true after home-all' % j, 'joint.%d.is-homed' % j, True)
    h.expect_pin('joint.selected.is-homed is true after home-all', 'joint.selected.is-homed', True)
    # task puts an identity-kins machine into teleop once everything is homed
    h.expect_pin('mode.is-teleop reports the switch to teleop after homing', 'mode.is-teleop', True)

    # homing is refused while in ESTOP
    h.nml_unhome_all()
    h.machine_estop()
    h.pulse('joint.0.home')
    h.expect_steady('joint.0.home in ESTOP does not home', lambda: not h.stat.homed[0])
    h.pulse('home-all')
    h.expect_steady('home-all in ESTOP does not home', lambda: not any(h.stat.homed[:JOINTS]))


def hard_limits_and_fault(h):
    manual_joint_mode(h, homed=True)
    h.set('joint.jog-speed', 60)
    h.set('joint.0.increment', 0.1)
    for kind, motion_pin, key in (('min', 'neg-lim-sw-in', 'min_hard_limit'),
                                  ('max', 'pos-lim-sw-in', 'max_hard_limit')):
        manual_joint_mode(h)
        h.pulse('joint.0.select')
        pin = 'joint.0.on-hard-%s-limit' % kind
        sel = 'joint.selected.on-hard-%s-limit' % kind
        h.expect_pin('%s is false with the switch open' % pin, pin, False)
        hal.set_p('joint.0.' + motion_pin, '1')
        h.expect('task sees joint 0 on its hard %s limit' % kind,
                 lambda k=key: h.stat.joint[0][k])
        h.expect_pin('%s sets' % pin, pin, True)
        h.expect_pin('%s follows selected joint 0' % sel, sel, True)
        for other in (1, 2):
            h.expect_pin('joint.%d.on-hard-%s-limit stays false' % (other, kind),
                         'joint.%d.on-hard-%s-limit' % (other, kind), False)
        h.pulse('joint.1.select')
        h.expect_pin('%s is false while joint 1 is selected' % sel, sel, False)
        h.expect_pin('%s is still set for joint 0' % pin, pin, True)
        h.pulse('joint.0.select')
        h.expect_pin('%s follows joint 0 again once it is reselected' % sel, sel, True)

        # override-limits is a status pin in halui; the override comes over
        # NML. task reports one flag for all joints (taskintf.cc:
        # overrideLimits = !!overrideLimitMask), so every joint shows it.
        h.cmd.override_limits()
        h.cmd.wait_complete()
        h.expect('task reports the limits overridden', lambda: h.stat.joint[0]['override_limits'])
        for j in range(JOINTS):
            h.expect_pin('joint.%d.override-limits is set (one flag for all joints)' % j,
                         'joint.%d.override-limits' % j, True)
        h.expect_pin('joint.selected.override-limits is set', 'joint.selected.override-limits', True)

        hal.set_p('joint.0.' + motion_pin, '0')
        h.expect('the hard %s limit clears in task' % kind, lambda k=key: not h.stat.joint[0][k])
        h.expect_pin('%s clears' % pin, pin, False)
        h.expect_pin('%s clears' % sel, sel, False)

        # motion drops the override once a jog made while overridden finishes
        manual_joint_mode(h)
        h.expect_pin('override-limits is still set before any jog', 'joint.0.override-limits', True)
        h.pulse('joint.0.increment-plus' if kind == 'min' else 'joint.0.increment-minus')
        h.expect('the jog off the switch completes', lambda: h.stat.inpos and close(jv(h, 0), 0.0, 1e-9))
        for j in range(JOINTS):
            h.expect_pin('joint.%d.override-limits clears after that jog' % j,
                         'joint.%d.override-limits' % j, False)

    manual_joint_mode(h)
    h.pulse('joint.2.select')
    h.expect_pin('joint.2.has-fault is false', 'joint.2.has-fault', False)
    hal.set_p('joint.2.amp-fault-in', '1')
    h.expect('task sees the joint 2 amp fault', lambda: h.stat.joint[2]['fault'])
    h.expect_pin('joint.2.has-fault sets', 'joint.2.has-fault', True)
    h.expect_pin('joint.selected.has-fault follows joint 2', 'joint.selected.has-fault', True)
    h.expect_pin('joint.0.has-fault stays false', 'joint.0.has-fault', False)
    hal.set_p('joint.2.amp-fault-in', '0')
    manual_joint_mode(h)
    h.expect('the fault clears in task', lambda: not h.stat.joint[2]['fault'])
    h.expect_pin('joint.2.has-fault clears', 'joint.2.has-fault', False)
    h.expect_pin('joint.selected.has-fault clears', 'joint.selected.has-fault', False)


def soft_limit_pins_never_set(h):
    # task never reports per-joint soft limits: taskintf.cc hard-codes
    # minSoftLimit/maxSoftLimit to 0 ("soft limits are now applied to the
    # command, and should never happen"). motion does still detect a joint
    # beyond its soft limit and says so on motion.on-soft-limit.
    #
    # [JOINT_2] limits are -4 .. 4 and the joint sits at 0. Move the limit
    # past the joint through task's ini.N.* pins -- the case motion's own
    # comment names as the way this happens. motion then wants the joint
    # unhomed before it will move again, so this runs last.
    to_zero(h)
    manual_joint_mode(h, homed=True)
    h.pulse('joint.2.select')
    for kind, ini_pin, value in (('min', 'ini.2.min_limit', 1.0),
                                 ('max', 'ini.2.max_limit', -1.0)):
        pin = 'joint.2.on-soft-%s-limit' % kind
        sel = 'joint.selected.on-soft-%s-limit' % kind
        h.expect_pin('%s is false inside the limits' % pin, pin, False)
        old = hal.get_value(ini_pin)
        hal.set_p(ini_pin, str(value))
        h.expect('motion reports joint 2 beyond its soft %s limit' % kind,
                 lambda: hal.get_value('motion.on-soft-limit'))
        h.known_bug('%s stays false while joint 2 is past that limit' % pin,
                    h.stays(lambda p=pin, s=sel: not h.get(p) and not h.get(s), 0.5),
                    'task hard-codes minSoftLimit/maxSoftLimit to 0 (taskintf.cc), so '
                    'halui.joint.N.on-soft-*-limit and the selected mirrors can never be set')
        hal.set_p(ini_pin, str(old))
        h.expect('motion clears on-soft-limit once the limit is restored',
                 lambda: not hal.get_value('motion.on-soft-limit'))
        h.check('the ini limit is back to %g' % old, close(hal.get_value(ini_pin), old))
    h.nml_unhome_all()


def continuous_jog(h):
    to_zero(h)
    h.set('joint.jog-speed', 60)
    for j in range(JOINTS):
        h.set('joint.%d.plus' % j, 1)
        expect_velocity(h, j, 1.0, 'while joint.%d.plus is held at jog-speed 60' % j)
        h.set('joint.%d.plus' % j, 0)
        expect_stopped(h, j, 'when joint.%d.plus is released' % j)
        h.set('joint.%d.minus' % j, 1)
        expect_velocity(h, j, -1.0, 'while joint.%d.minus is held' % j)
        h.set('joint.%d.minus' % j, 0)
        expect_stopped(h, j, 'when joint.%d.minus is released' % j)

    to_zero(h)
    h.set('joint.0.plus', 1)
    expect_velocity(h, 0, 1.0, 'at jog-speed 60')
    h.set('joint.jog-speed', 120)
    expect_velocity(h, 0, 2.0, 'after jog-speed changes to 120 mid-jog')
    h.set('joint.jog-speed', 30)
    expect_velocity(h, 0, 0.5, 'after jog-speed changes to 30 mid-jog')
    h.set('joint.0.plus', 0)
    expect_stopped(h, 0, 'on release')

    h.set('joint.jog-speed', 60)
    h.set('joint.1.plus', 1)
    h.set('joint.2.minus', 1)
    expect_velocity(h, 1, 1.0, 'together with joint 2')
    expect_velocity(h, 2, -1.0, 'together with joint 1')
    h.set('joint.1.plus', 0)
    expect_stopped(h, 1, 'on release while joint 2 keeps going')
    expect_velocity(h, 2, -1.0, 'after joint 1 is released')
    h.set('joint.2.minus', 0)
    expect_stopped(h, 2, 'on release')

    h.set('joint.jog-speed', 0)
    h.set('joint.0.plus', 1)
    expect_not_moving(h, 0, 'at jog-speed 0')
    h.set('joint.0.plus', 0)
    h.set('joint.jog-speed', 60)


def jog_is_gated(h):
    to_zero(h)
    h.set('joint.jog-speed', 60)

    h.nml_state(L.STATE_OFF)
    h.set('joint.0.plus', 1)
    expect_not_moving(h, 0, 'when the machine is off')
    h.set('joint.0.plus', 0)

    manual_joint_mode(h, homed=True)
    h.nml_teleop(1)
    h.set('joint.0.plus', 1)
    expect_not_moving(h, 0, 'in teleop mode')
    h.set('joint.0.plus', 0)
    h.nml_teleop(0)

    # abort stops a jog; the held input does not restart it
    h.set('joint.0.plus', 1)
    expect_velocity(h, 0, 1.0, 'before abort')
    h.pulse('abort')
    expect_stopped(h, 0, 'after abort')
    expect_not_moving(h, 0, 'while joint.0.plus is still held after abort')
    h.set('joint.0.plus', 0)


def analog_jog(h):
    to_zero(h)
    h.set('joint.jog-speed', 60)
    h.set('joint.0.analog', 0.5)
    expect_velocity(h, 0, 0.5, 'at analog 0.5')
    h.set('joint.0.analog', -0.8)
    expect_velocity(h, 0, -0.8, 'at analog -0.8')
    h.set('joint.0.analog', 0.1)
    expect_stopped(h, 0, 'at analog 0.1, inside the 0.2 deadband')
    h.set('joint.0.analog', 0.3)
    expect_velocity(h, 0, 0.3, 'at analog 0.3')
    h.set('joint.jog-speed', 120)
    expect_velocity(h, 0, 0.6, 'at analog 0.3 after jog-speed changes to 120')
    h.set('joint.jog-speed', 60)
    h.set('joint.0.analog', 0.0)
    expect_stopped(h, 0, 'at analog 0')

    h.set('joint.jog-deadband', 0.6)
    h.set('joint.0.analog', 0.5)
    expect_not_moving(h, 0, 'at analog 0.5 with a 0.6 deadband')
    h.set('joint.0.analog', 0.7)
    expect_velocity(h, 0, 0.7, 'at analog 0.7 with a 0.6 deadband')
    h.set('joint.0.analog', -0.65)
    expect_velocity(h, 0, -0.65, 'at analog -0.65 with a 0.6 deadband')
    h.set('joint.0.analog', 0.0)
    expect_stopped(h, 0, 'at analog 0')
    h.set('joint.jog-deadband', 0.2)

    for j in (1, 2):
        h.set('joint.%d.analog' % j, -0.4)
        expect_velocity(h, j, -0.4, 'at analog -0.4')
        h.set('joint.%d.analog' % j, 0.0)
        expect_stopped(h, j, 'at analog 0')


def expect_at(h, j, pos, why):
    return h.expect('joint %d ends at %+.3f %s' % (j, pos, why),
                    lambda: close(h.joint_pos(j), pos, 1e-4) and h.stat.inpos,
                    timeout=10, detail=lambda: 'position %+.4f' % h.joint_pos(j))


def incremental_jog(h):
    to_zero(h)
    h.set('joint.jog-speed', 60)
    for j in range(JOINTS):
        h.set('joint.%d.increment' % j, 0.25)
        h.pulse('joint.%d.increment-plus' % j)
        expect_at(h, j, 0.25, 'after increment-plus')
        h.pulse('joint.%d.increment-minus' % j)
        expect_at(h, j, 0.0, 'after increment-minus')

    h.set('joint.0.increment-plus', 1)
    expect_at(h, 0, 0.25, 'while increment-plus is held')
    h.expect_steady('a held increment-plus moves only once',
                    lambda: close(h.joint_pos(0), 0.25, 1e-4), duration=0.6)
    h.set('joint.0.increment-plus', 0)
    h.settle()
    h.expect_steady('releasing increment-plus moves nothing',
                    lambda: close(h.joint_pos(0), 0.25, 1e-4))

    h.pulse('joint.1.select')
    h.set('joint.selected.increment', 0.5)
    h.pulse('joint.selected.increment-plus')
    expect_at(h, 1, 0.5, 'after joint.selected.increment-plus with joint 1 selected')
    h.pulse('joint.selected.increment-minus')
    expect_at(h, 1, 0.0, 'after joint.selected.increment-minus')
    h.check('the selected increment did not move joint 0',
            close(h.joint_pos(0), 0.25, 1e-4), 'joint 0 at %+.4f' % h.joint_pos(0))

    h.nml_state(L.STATE_OFF)
    h.pulse('joint.2.increment-plus')
    h.expect_steady('increment-plus does nothing when the machine is off',
                    lambda: close(h.joint_pos(2), 0.0, 1e-4))


def selected_jog(h):
    to_zero(h)
    h.set('joint.jog-speed', 60)
    h.pulse('joint.0.select')

    h.set('joint.selected.plus', 1)
    expect_velocity(h, 0, 1.0, 'while joint.selected.plus is held')
    h.pulse('joint.1.select')
    expect_stopped(h, 0, 'when the selection moves to joint 1 mid-jog')
    expect_velocity(h, 1, 1.0, 'after being selected mid-jog')
    h.set('joint.selected.plus', 0)
    expect_stopped(h, 1, 'when joint.selected.plus is released')

    h.set('joint.selected.minus', 1)
    expect_velocity(h, 1, -1.0, 'while joint.selected.minus is held')
    h.pulse('joint.2.select')
    expect_stopped(h, 1, 'when the selection moves to joint 2')
    expect_velocity(h, 2, -1.0, 'after being selected during a minus jog')
    h.set('joint.selected.minus', 0)
    expect_stopped(h, 2, 'on release')

    # a joint jogged on its own pin keeps going when the selection changes
    to_zero(h)
    h.pulse('joint.0.select')
    h.set('joint.2.plus', 1)
    h.set('joint.selected.plus', 1)
    expect_velocity(h, 0, 1.0, 'as the selected joint')
    expect_velocity(h, 2, 1.0, 'on its own pin')
    h.pulse('joint.1.select')
    expect_stopped(h, 0, 'after the selection moves to joint 1')
    expect_velocity(h, 1, 1.0, 'as the newly selected joint')
    expect_velocity(h, 2, 1.0, 'still, because its own pin is held')
    h.set('joint.selected.plus', 0)
    h.set('joint.2.plus', 0)
    expect_stopped(h, 1, 'on release')
    expect_stopped(h, 2, 'on release')

    # changing the selection when nothing is jogging moves nothing
    h.pulse('joint.0.select')
    for j in range(JOINTS):
        expect_not_moving(h, j, 'after a selection change with no jog active')


run(pins_and_defaults,
    select,
    homing,
    hard_limits_and_fault,
    continuous_jog,
    jog_is_gated,
    analog_jog,
    incremental_jog,
    selected_jog,
    soft_limit_pins_never_set)
