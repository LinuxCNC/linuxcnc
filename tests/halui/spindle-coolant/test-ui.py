#!/usr/bin/env python3
"""halui: spindle control, spindle brake, mist and flood.

Pins: spindle.0.start, stop, forward, reverse, increase, decrease,
      brake-on, brake-off, is-on, runs-forward, runs-backward, brake-is-on,
      mist.on, mist.off, mist.is-on, flood.on, flood.off, flood.is-on
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '_lib'))
from halui_test import run  # noqa: E402

import linuxcnc as L  # noqa: E402

SP = 'spindle.0.'


def sp(h):
    return h.stat.spindle[0]


def expect_spindle(h, speed, why):
    direction = (speed > 0) - (speed < 0)
    h.expect('spindle 0 runs at %g %s' % (speed, why),
             lambda: sp(h)['speed'] == speed and sp(h)['direction'] == direction
             and bool(sp(h)['enabled']) == (speed != 0),
             detail=lambda: 'speed=%g direction=%d enabled=%d'
             % (sp(h)['speed'], sp(h)['direction'], sp(h)['enabled']))
    h.expect_pin(SP + 'is-on is %s %s' % (speed != 0, why), SP + 'is-on', speed != 0)
    h.expect_pin(SP + 'runs-forward is %s %s' % (speed > 0, why), SP + 'runs-forward', speed > 0)
    h.expect_pin(SP + 'runs-backward is %s %s' % (speed < 0, why), SP + 'runs-backward', speed < 0)


def expect_speed_steady(h, speed, why):
    h.expect_steady('spindle 0 stays at %g %s' % (speed, why),
                    lambda: sp(h)['speed'] == speed,
                    detail=lambda: 'speed=%g' % sp(h)['speed'])


def without_s_word(h):
    h.machine_on()
    h.nml_mode(L.MODE_MANUAL)
    expect_spindle(h, 0, 'at startup')
    # with no S word active, halui asks for speed +/-1
    h.pulse(SP + 'start')
    expect_spindle(h, 1, 'after start with no S word')
    h.pulse(SP + 'stop')
    expect_spindle(h, 0, 'after stop')
    h.pulse(SP + 'reverse')
    expect_spindle(h, -1, 'after reverse with no S word')
    h.pulse(SP + 'forward')
    expect_spindle(h, 1, 'after forward with no S word')
    h.pulse(SP + 'stop')
    expect_spindle(h, 0, 'after stop')


def with_s_word(h):
    h.machine_on()
    h.nml_mdi('S500')
    h.nml_mode(L.MODE_MANUAL)
    h.pulse(SP + 'forward')
    expect_spindle(h, 500, 'after forward with S500')
    h.pulse(SP + 'reverse')
    expect_spindle(h, -500, 'after reverse with S500')
    h.pulse(SP + 'start')
    expect_spindle(h, 500, 'after start with S500 (start is forward)')

    # each rising edge steps by [SPINDLE_0]INCREMENT (default 100);
    # the falling edge sends SPINDLE_CONSTANT, which keeps the speed
    h.set(SP + 'increase', 1)
    expect_spindle(h, 600, 'on the rising edge of increase')
    expect_speed_steady(h, 600, 'while increase is held')
    h.set(SP + 'increase', 0)
    expect_speed_steady(h, 600, 'after increase is released')

    h.set(SP + 'decrease', 1)
    expect_spindle(h, 500, 'on the rising edge of decrease')
    h.set(SP + 'decrease', 0)
    expect_speed_steady(h, 500, 'after decrease is released')

    h.pulse(SP + 'reverse')
    expect_spindle(h, -500, 'after reverse')
    h.pulse(SP + 'increase')
    expect_spindle(h, -600, 'after increase while in reverse (magnitude grows)')
    h.pulse(SP + 'decrease')
    expect_spindle(h, -500, 'after decrease while in reverse')

    h.pulse(SP + 'stop')
    expect_spindle(h, 0, 'after stop')
    h.pulse(SP + 'increase')
    expect_spindle(h, 0, 'after increase while stopped (does nothing)')
    h.pulse(SP + 'decrease')
    expect_spindle(h, 0, 'after decrease while stopped (does nothing)')


def brake(h):
    h.machine_on()
    h.nml_mode(L.MODE_MANUAL)
    h.pulse(SP + 'brake-on')
    h.expect('brake-on engages the brake', lambda: sp(h)['brake'] == 1)
    h.expect_pin(SP + 'brake-is-on sets', SP + 'brake-is-on', True)
    h.pulse(SP + 'brake-off')
    h.expect('brake-off releases the brake', lambda: sp(h)['brake'] == 0)
    h.expect_pin(SP + 'brake-is-on clears', SP + 'brake-is-on', False)

    h.cmd.brake(L.BRAKE_ENGAGE, 0)
    h.expect_pin(SP + 'brake-is-on follows an NML brake engage', SP + 'brake-is-on', True)
    h.cmd.brake(L.BRAKE_RELEASE, 0)
    h.expect_pin(SP + 'brake-is-on follows an NML brake release', SP + 'brake-is-on', False)


def status_follows_other_clients(h):
    h.machine_on()
    h.nml_mode(L.MODE_MANUAL)
    h.cmd.spindle(L.SPINDLE_FORWARD, 300, 0)
    expect_spindle(h, 300, 'after an NML spindle forward')
    h.cmd.spindle(L.SPINDLE_REVERSE, 300, 0)
    expect_spindle(h, -300, 'after an NML spindle reverse')
    h.cmd.spindle(L.SPINDLE_OFF, 0)
    expect_spindle(h, 0, 'after an NML spindle off')


def spindle_when_not_on(h):
    h.machine_estop()
    h.pulse(SP + 'start')
    expect_speed_steady(h, 0, 'when start is pressed in ESTOP')
    h.expect_pin(SP + 'is-on stays false in ESTOP', SP + 'is-on', False)


def coolant(h):
    h.machine_on()
    h.nml_mode(L.MODE_MANUAL)
    for kind in ('mist', 'flood'):
        read = (lambda s: s.mist) if kind == 'mist' else (lambda s: s.flood)
        h.pulse(kind + '.on')
        h.expect('%s.on turns %s on' % (kind, kind), lambda r=read: r(h.stat))
        h.expect_pin('%s.is-on sets' % kind, kind + '.is-on', True)
        h.pulse(kind + '.off')
        h.expect('%s.off turns %s off' % (kind, kind), lambda r=read: not r(h.stat))
        h.expect_pin('%s.is-on clears' % kind, kind + '.is-on', False)

    h.cmd.mist(L.MIST_ON)
    h.expect_pin('mist.is-on follows an NML mist on', 'mist.is-on', True)
    h.cmd.flood(L.FLOOD_ON)
    h.expect_pin('flood.is-on follows an NML flood on', 'flood.is-on', True)
    h.cmd.mist(L.MIST_OFF)
    h.cmd.flood(L.FLOOD_OFF)
    h.expect_pin('mist.is-on follows an NML mist off', 'mist.is-on', False)
    h.expect_pin('flood.is-on follows an NML flood off', 'flood.is-on', False)


def machine_off_stops_flood(h):
    # task turns flood off whenever the machine is turned on or off
    # (emcTaskSetState); flood.is-on has to follow that
    h.machine_on()
    h.pulse('flood.on')
    h.expect_pin('flood is on', 'flood.is-on', True)
    h.pulse('machine.off')
    h.expect('machine.off turns flood off in task', lambda: not h.stat.flood)
    h.expect_pin('flood.is-on follows task turning flood off', 'flood.is-on', False)


run(without_s_word,
    with_s_word,
    brake,
    status_follows_other_clients,
    spindle_when_not_on,
    coolant,
    machine_off_stops_flood)
