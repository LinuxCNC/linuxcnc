#!/usr/bin/env python3
"""halui: feed, rapid, spindle and max-velocity overrides.

For each override: counts (relative and direct-value), count-enable, scale,
increase, decrease, reset, value -- and the clamping halui applies.

Limits in halui.ini: [DISPLAY]MAX_FEED_OVERRIDE = 1.5,
MIN_SPINDLE_OVERRIDE = 0.5, MAX_SPINDLE_OVERRIDE = 1.2,
[TRAJ]MAX_LINEAR_VELOCITY = 4.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '_lib'))
from halui_test import run, close  # noqa: E402

import linuxcnc as L  # noqa: E402


class Model:
    """What halui should ask for: last value +/- steps, clamped."""

    def __init__(self, value, lo, hi):
        self.value, self.lo, self.hi, self.counts = value, lo, hi, 0

    def clamp(self, v):
        return max(self.lo, min(self.hi, v))

    def relative(self, counts, scale):
        self.value = self.clamp(self.value + (counts - self.counts) * scale)
        self.counts = counts

    def direct(self, counts, scale):
        self.value = self.clamp(counts * scale)
        self.counts = counts

    def ignored(self, counts):
        self.counts = counts

    def set(self, v):
        self.value = self.clamp(v)


def exercise(h, label, pfx, read, lo, hi, has_reset=True):
    def P(s):
        return pfx + '.' + s

    m = Model(read(h.poll()), lo, hi)

    def expect(why):
        # When a step leaves the value unchanged (already at a limit) the
        # check below passes at once, so give halui time to sample the input
        # before the next one arrives -- otherwise two steps look like one.
        h.settle()
        want = m.value
        h.expect('%s is %.3f %s' % (label, want, why),
                 lambda: close(read(h.stat), want, 1e-6),
                 detail=lambda: 'task has %.4f' % read(h.stat))
        h.expect_pin('%s.value is %.3f %s' % (pfx, want, why), P('value'), want, tol=1e-6)

    h.check('%s.scale defaults to 0.1' % pfx, close(h.default(P('scale')), 0.1),
            'default %r' % h.default(P('scale')))
    h.check('%s.count-enable defaults to on' % pfx, h.default(P('count-enable')) is True,
            'default %r' % h.default(P('count-enable')))
    expect('at startup')

    scale = 0.1
    for c in (1, 4, 20, 19, -40, -38):
        h.set(P('counts'), c)
        m.relative(c, scale)
        expect('after counts -> %d (relative)' % c)

    h.set(P('count-enable'), 0)
    h.set(P('counts'), -30)
    m.ignored(-30)
    h.expect_steady('%s ignores counts while count-enable is off' % pfx,
                    lambda: close(read(h.stat), m.value, 1e-6))
    h.set(P('count-enable'), 1)
    h.set(P('counts'), -29)
    m.relative(-29, scale)
    expect('after re-enabling: one count, no jump from the ignored ones')

    h.pulse(P('increase'))
    m.set(m.value + scale)
    expect('after increase')
    h.pulse(P('decrease'))
    m.set(m.value - scale)
    expect('after decrease')

    h.set(P('increase'), 1)
    h.settle(0.3)
    m.set(m.value + scale)
    expect('while increase is held (one step per edge)')
    h.set(P('increase'), 0)
    h.settle()

    scale = 0.25
    h.set(P('scale'), scale)
    h.pulse(P('increase'))
    m.set(m.value + scale)
    expect('after increase with scale 0.25')
    for _ in range(8):
        h.pulse(P('increase'))
        m.set(m.value + scale)
    expect('after increasing past the upper limit')
    for _ in range(12):
        h.pulse(P('decrease'))
        m.set(m.value - scale)
    expect('after decreasing past the lower limit')

    if has_reset:
        h.pulse(P('reset'))
        m.set(1.0)
        expect('after reset')
    else:
        h.check('%s has no reset pin' % pfx, not h.has(P('reset')))

    scale = 0.1
    h.set(P('scale'), scale)
    h.set(P('direct-value'), 1)
    for c in (7, 30, -1, 9):
        h.set(P('counts'), c)
        m.direct(c, scale)
        expect('after counts -> %d (direct-value)' % c)
    h.set(P('direct-value'), 0)
    h.set(P('counts'), 10)
    m.relative(10, scale)
    expect('after counts -> 10 (relative again)')
    return m


def feed_override(h):
    h.machine_on()
    exercise(h, 'task feed override', 'feed-override', lambda s: s.feedrate, 0.0, 1.5)
    h.cmd.feedrate(0.7)
    h.expect_pin('feed-override.value follows an NML change', 'feed-override.value', 0.7)


def rapid_override(h):
    h.machine_on()
    # halui hard-codes the rapid override ceiling at 1.0
    exercise(h, 'task rapid override', 'rapid-override', lambda s: s.rapidrate, 0.0, 1.0)
    h.cmd.rapidrate(0.3)
    h.expect_pin('rapid-override.value follows an NML change', 'rapid-override.value', 0.3)


def spindle_override(h):
    h.machine_on()
    exercise(h, 'task spindle 0 override', 'spindle.0.override',
             lambda s: s.spindle[0]['override'], 0.5, 1.2)
    h.cmd.spindleoverride(0.8, 0)
    h.expect_pin('spindle.0.override.value follows an NML change',
                 'spindle.0.override.value', 0.8)


def max_velocity(h):
    h.machine_on()
    start = h.poll().max_velocity
    h.check('task starts at [TRAJ]MAX_LINEAR_VELOCITY', close(start, 4.0), 'max_velocity=%.3f' % start)
    h.expect_pin('max-velocity.value shows it', 'max-velocity.value', 4.0)
    h.check('max-velocity.scale defaults to 0 (halui never initialises it)',
            close(h.default('max-velocity.scale'), 0.0), 'default %r' % h.default('max-velocity.scale'))
    h.check('max-velocity.count-enable defaults to on', h.default('max-velocity.count-enable') is True)

    h.set('max-velocity.scale', 0.5)
    h.pulse('max-velocity.decrease')
    h.known_bug('max-velocity.decrease from 4.0 lands on 1.0 instead of 3.5',
                h.wait_for(lambda: close(h.stat.max_velocity, 1.0)),
                'every request is clamped to 1.0: iniLoad() no longer reads '
                '[TRAJ]MAX_LINEAR_VELOCITY into maxMaxVelocity since 2f57090adb '
                '(new ini parser); intended ceiling here is 4.0')
    h.expect_pin('max-velocity.value follows the clamped value', 'max-velocity.value', 1.0)

    # inside [0, 1.0] the override behaves as intended
    m = Model(1.0, 0.0, 1.0)

    def expect(why):
        h.settle()
        want = m.value
        h.expect('task max velocity is %.3f %s' % (want, why),
                 lambda: close(h.stat.max_velocity, want, 1e-6),
                 detail=lambda: 'task has %.4f' % h.stat.max_velocity)
        h.expect_pin('max-velocity.value is %.3f %s' % (want, why), 'max-velocity.value', want)

    h.pulse('max-velocity.decrease')
    m.set(0.5)
    expect('after decrease')
    h.pulse('max-velocity.increase')
    m.set(1.0)
    expect('after increase')
    h.pulse('max-velocity.increase')
    expect('after increasing past the (buggy) 1.0 ceiling')
    for c in (-1, -5, -3):
        h.set('max-velocity.counts', c)
        m.relative(c, 0.5)
        expect('after counts -> %d (relative)' % c)

    h.set('max-velocity.count-enable', 0)
    h.set('max-velocity.counts', 5)
    m.ignored(5)
    h.expect_steady('max-velocity ignores counts while count-enable is off',
                    lambda: close(h.stat.max_velocity, m.value, 1e-6))
    h.set('max-velocity.count-enable', 1)

    h.set('max-velocity.direct-value', 1)
    h.set('max-velocity.counts', 1)
    m.direct(1, 0.5)
    expect('after counts -> 1 (direct-value)')
    h.set('max-velocity.counts', 6)
    h.known_bug('max-velocity direct-value 3.0 lands on 1.0',
                h.wait_for(lambda: close(h.stat.max_velocity, 1.0)),
                'same maxMaxVelocity clamp; intended result is 3.0')
    h.set('max-velocity.direct-value', 0)
    h.check('max-velocity has no reset pin', not h.has('max-velocity.reset'))

    h.cmd.maxvel(2.5)
    h.expect_pin('max-velocity.value follows an NML change', 'max-velocity.value', 2.5)


def not_on(h):
    # overrides are immediate commands: task accepts them even when not ON
    h.machine_estop()
    h.cmd.feedrate(1.0)
    h.cmd.wait_complete()
    h.pulse('feed-override.decrease')
    h.expect('feed override changes while in ESTOP', lambda: close(h.stat.feedrate, 0.9))


run(feed_override, rapid_override, spindle_override, max_velocity, not_on)
