#!/usr/bin/env python3
"""halui on a two-joint YZ machine with two spindles, no home sequence and
no MDI commands.

Checks which pins exist for this configuration, that a second spindle is
driven independently, and how axis selection starts up when the first
configured axis is not X. Joint 0 drives Y and joint 1 drives Z.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '_lib'))
from halui_test import run, close  # noqa: E402

import linuxcnc as L  # noqa: E402

LETTERS = 'xyzabcuvw'
CONFIGURED = 'yz'
JOINT_OF = {'y': 0, 'z': 1}


def jv(h, j):
    return h.stat.joint[j]['velocity']


def home_each(h):
    # there is no home sequence, so home-all is not an option: one at a time
    h.machine_on()
    h.nml_mode(L.MODE_MANUAL)
    for j in (0, 1):
        if not h.poll().homed[j]:
            h.cmd.home(j)
            h.cmd.wait_complete()
            if not h.wait_for(lambda j=j: h.stat.homed[j]):
                raise RuntimeError('setup: could not home joint %d' % j)


def teleop(h):
    home_each(h)
    if h.poll().motion_mode != L.TRAJ_MODE_TELEOP:
        h.nml_teleop(1)


def pins(h):
    h.check('no home-all pin without [JOINT_n]HOME_SEQUENCE', not h.has('home-all'))
    h.check('no halui-mdi-is-running pin without [HALUI]MDI_COMMAND',
            not h.has('halui-mdi-is-running'))
    h.check('no mdi-command pins without [HALUI]MDI_COMMAND',
            not any(p.startswith('mdi-command-') for p in h.pins))
    h.check('joint pins exist for joints 0 and 1',
            all(h.has('joint.%d.%s' % (j, p)) for j in (0, 1) for p in ('home', 'is-homed', 'plus')))
    h.check('no pins for joint 2', not any(p.startswith('joint.2.') for p in h.pins))
    h.check('spindle pins exist for spindles 0 and 1',
            all(h.has('spindle.%d.%s' % (s, p)) for s in (0, 1)
                for p in ('start', 'is-on', 'brake-on', 'override.value')))
    h.check('no pins for spindle 2', not any(p.startswith('spindle.2.') for p in h.pins))
    wrong = [a for a in LETTERS
             for p in ('is-selected', 'pos-commanded', 'pos-feedback', 'pos-relative')
             if h.has('axis.%s.%s' % (a, p)) != (a in CONFIGURED)]
    h.check('axis status pins exist only for Y and Z', not wrong, 'wrong: %s' % wrong)
    h.check('axis input pins exist for all nine axes',
            all(h.has('axis.%s.plus' % a) for a in LETTERS))


def startup_selection(h):
    h.expect_pin('joint.0.is-selected is true at startup', 'joint.0.is-selected', True)
    h.expect_pin('axis.y.is-selected marks the first configured axis', 'axis.y.is-selected', True)
    h.expect_pin('axis.z.is-selected is false at startup', 'axis.z.is-selected', False)
    h.known_bug('axis.selected is 0 (X, not on this machine) at startup',
                h.get('axis.selected') == 0,
                'hal_init_pins() sets axis_selected to 0 while halui_hal_init() marks the '
                'first configured axis (Y, index 1) as selected; should be 1')

    teleop(h)
    h.set('axis.jog-speed', 60)
    h.set('axis.selected.plus', 1)
    h.known_bug('axis.selected.plus at startup does not jog the axis shown as selected',
                h.stays(lambda: close(jv(h, JOINT_OF['y']), 0.0, 1e-9), 0.8),
                'the jog goes to axis index 0 (X), which halui then drops as not configured')
    h.set('axis.selected.plus', 0)
    h.settle()

    h.pulse('axis.y.select')
    h.expect_pin('pressing axis.y.select makes axis.selected 1', 'axis.selected', 1)
    h.set('axis.selected.plus', 1)
    h.expect('after that, axis.selected.plus jogs Y',
             lambda: close(jv(h, JOINT_OF['y']), 1.0, 1e-3),
             detail=lambda: 'joint 0 velocity %+.4f' % jv(h, 0))
    h.set('axis.selected.plus', 0)
    h.expect('Y stops on release', lambda: close(jv(h, 0), 0.0, 1e-9) and h.stat.inpos)


def second_spindle(h):
    h.machine_on()
    h.nml_mode(L.MODE_MANUAL)

    def sp(s):
        return h.stat.spindle[s]

    h.pulse('spindle.1.start')
    h.expect('spindle.1.start turns spindle 1 on',
             lambda: sp(1)['enabled'] and sp(1)['speed'] == 1)
    h.check('spindle 0 stays off', not sp(0)['enabled'])
    h.expect_pin('spindle.1.is-on sets', 'spindle.1.is-on', True)
    h.expect_pin('spindle.1.runs-forward sets', 'spindle.1.runs-forward', True)
    h.expect_pin('spindle.0.is-on stays false', 'spindle.0.is-on', False)

    h.pulse('spindle.1.reverse')
    h.expect('spindle.1.reverse reverses spindle 1', lambda: sp(1)['direction'] == -1)
    h.expect_pin('spindle.1.runs-backward sets', 'spindle.1.runs-backward', True)
    h.pulse('spindle.1.stop')
    h.expect('spindle.1.stop stops spindle 1', lambda: not sp(1)['enabled'])
    h.expect_pin('spindle.1.is-on clears', 'spindle.1.is-on', False)

    brake0 = sp(0)['brake']
    h.pulse('spindle.1.brake-on')
    h.expect('spindle.1.brake-on engages brake 1', lambda: sp(1)['brake'] == 1)
    h.expect_pin('spindle.1.brake-is-on sets', 'spindle.1.brake-is-on', True)
    h.check('brake 0 is untouched', sp(0)['brake'] == brake0,
            'brake 0 was %d, now %d' % (brake0, sp(0)['brake']))
    h.pulse('spindle.1.brake-off')
    h.expect_pin('spindle.1.brake-is-on clears', 'spindle.1.brake-is-on', False)

    # no [DISPLAY] spindle override limits here: halui clamps to 0 .. 1.0
    h.pulse('spindle.1.override.decrease')
    h.expect('spindle.1.override.decrease lowers override 1',
             lambda: close(sp(1)['override'], 0.9))
    h.check('override 0 is untouched', close(sp(0)['override'], 1.0))
    h.expect_pin('spindle.1.override.value follows', 'spindle.1.override.value', 0.9)
    h.pulse('spindle.1.override.increase')
    h.pulse('spindle.1.override.increase')
    h.expect('spindle 1 override is clamped at the default maximum of 1.0',
             lambda: close(sp(1)['override'], 1.0))
    h.set('spindle.1.override.counts', -3)
    h.expect('spindle.1.override.counts moves override 1', lambda: close(sp(1)['override'], 0.7))
    h.pulse('spindle.1.override.reset')
    h.expect('spindle.1.override.reset puts it back to 1.0', lambda: close(sp(1)['override'], 1.0))


def motion(h):
    home_each(h)
    if h.poll().motion_mode != L.TRAJ_MODE_FREE:
        h.nml_teleop(0)
    h.set('joint.jog-speed', 60)
    h.set('joint.1.plus', 1)
    h.expect('joint.1.plus jogs joint 1', lambda: close(jv(h, 1), 1.0, 1e-3))
    h.set('joint.1.plus', 0)
    h.expect('joint 1 stops on release', lambda: close(jv(h, 1), 0.0, 1e-9) and h.stat.inpos)

    teleop(h)
    h.set('axis.jog-speed', 60)
    h.set('axis.z.plus', 1)
    h.expect('axis.z.plus moves joint 1 (Z)', lambda: close(jv(h, 1), 1.0, 1e-3))
    h.set('axis.z.plus', 0)
    h.expect('Z stops on release', lambda: close(jv(h, 1), 0.0, 1e-9) and h.stat.inpos)
    h.set('axis.y.minus', 1)
    h.expect('axis.y.minus moves joint 0 (Y) backwards', lambda: close(jv(h, 0), -1.0, 1e-3))
    h.set('axis.y.minus', 0)
    h.expect('Y stops on release', lambda: close(jv(h, 0), 0.0, 1e-9) and h.stat.inpos)

    h.set('axis.x.plus', 1)
    h.expect_steady('axis.x.plus moves nothing on a machine without X',
                    lambda: close(jv(h, 0), 0.0, 1e-9) and close(jv(h, 1), 0.0, 1e-9))
    h.set('axis.x.plus', 0)

    h.nml_mdi('G20 G90 G53 G0 Y1.5 Z-0.5')
    h.expect_pin('axis.y.pos-feedback shows Y', 'axis.y.pos-feedback', 1.5)
    h.expect_pin('axis.z.pos-feedback shows Z', 'axis.z.pos-feedback', -0.5)


run(pins, startup_selection, second_spindle, motion)
