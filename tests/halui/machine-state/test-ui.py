#!/usr/bin/env python3
"""halui: estop, machine power, task mode and motion mode.

Pins: estop.activate, estop.reset, estop.is-activated,
      machine.on, machine.off, machine.is-on, machine.units-per-mm,
      mode.manual, mode.auto, mode.mdi, mode.teleop, mode.joint,
      mode.is-manual, mode.is-auto, mode.is-mdi, mode.is-teleop, mode.is-joint
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '_lib'))
from halui_test import run, INCH_PER_MM  # noqa: E402

import linuxcnc as L  # noqa: E402

# task has no distinct OFF state: it reports ESTOP_RESET when the machine is
# neither estopped nor enabled (emctask.cc, emcTaskSetState)
POWERED_OFF = L.STATE_ESTOP_RESET

MODE_PINS = {L.MODE_MANUAL: 'mode.is-manual',
             L.MODE_AUTO: 'mode.is-auto',
             L.MODE_MDI: 'mode.is-mdi'}


def startup_status(h):
    h.expect('task starts in ESTOP', lambda: h.stat.task_state == L.STATE_ESTOP)
    h.expect_pin('estop.is-activated reflects startup ESTOP', 'estop.is-activated', True)
    h.expect_pin('machine.is-on is false at startup', 'machine.is-on', False)
    # linear units are units per mm; this is an inch machine
    h.expect_pin('machine.units-per-mm is published once task is up',
                 'machine.units-per-mm', INCH_PER_MM, tol=1e-9)


def estop_and_power(h):
    h.pulse('estop.reset')
    h.expect('estop.reset -> task ESTOP_RESET',
             lambda: h.stat.task_state == L.STATE_ESTOP_RESET)
    h.expect_pin('estop.is-activated clears on reset', 'estop.is-activated', False)
    h.expect_pin('machine.is-on stays false after reset', 'machine.is-on', False)

    h.pulse('machine.on')
    h.expect('machine.on -> task ON', lambda: h.stat.task_state == L.STATE_ON)
    h.expect_pin('machine.is-on sets', 'machine.is-on', True)

    h.pulse('machine.off')
    h.expect('machine.off powers down (task reads back ESTOP_RESET)',
             lambda: h.stat.task_state == POWERED_OFF)
    h.expect_pin('machine.is-on clears on OFF', 'machine.is-on', False)
    h.expect_pin('estop.is-activated stays clear on OFF', 'estop.is-activated', False)

    h.pulse('machine.on')
    h.expect('machine.on from OFF -> task ON', lambda: h.stat.task_state == L.STATE_ON)

    h.pulse('estop.activate')
    h.expect('estop.activate from ON -> task ESTOP', lambda: h.stat.task_state == L.STATE_ESTOP)
    h.expect_pin('estop.is-activated sets', 'estop.is-activated', True)
    h.expect_pin('machine.is-on clears on ESTOP', 'machine.is-on', False)


def power_on_refused_in_estop(h):
    # task refuses ON straight from ESTOP; halui passes the request on and
    # does not try to work around it
    h.machine_estop()
    h.pulse('machine.on')
    h.expect_steady('machine.on while in ESTOP is refused by task',
                    lambda: h.stat.task_state == L.STATE_ESTOP)
    h.expect_pin('machine.is-on stays false', 'machine.is-on', False)


def inputs_are_edge_triggered(h):
    h.machine_estop()
    h.nml_state(L.STATE_ESTOP_RESET)
    h.set('machine.on', 1)
    h.expect('rising edge on machine.on powers on', lambda: h.stat.task_state == L.STATE_ON)
    h.nml_state(L.STATE_OFF)
    h.expect_steady('a held machine.on does not power on again',
                    lambda: h.stat.task_state == POWERED_OFF, duration=0.6)
    h.set('machine.on', 0)
    h.expect_steady('the falling edge of machine.on does nothing',
                    lambda: h.stat.task_state == POWERED_OFF, duration=0.4)

    h.nml_state(L.STATE_ON)
    h.set('estop.activate', 1)
    h.expect('rising edge on estop.activate estops', lambda: h.stat.task_state == L.STATE_ESTOP)
    h.nml_state(L.STATE_ESTOP_RESET)
    h.expect_steady('a held estop.activate does not estop again',
                    lambda: h.stat.task_state == L.STATE_ESTOP_RESET, duration=0.6)
    h.set('estop.activate', 0)


def status_follows_other_clients(h):
    # output pins mirror task state no matter who changed it
    h.machine_estop()
    h.expect_pin('estop.is-activated follows an NML estop', 'estop.is-activated', True)
    h.machine_on()
    h.expect_pin('machine.is-on follows an NML power-on', 'machine.is-on', True)
    h.expect_pin('estop.is-activated follows an NML reset', 'estop.is-activated', False)


def expect_task_mode_pins(h, want, why):
    for mode, pin in MODE_PINS.items():
        h.expect_pin('%s is %s %s' % (pin, mode == want, why), pin, mode == want)


def task_modes(h):
    h.machine_on()
    h.nml_mode(L.MODE_MANUAL)
    for request, mode, label in (('mode.mdi', L.MODE_MDI, 'MDI'),
                                 ('mode.auto', L.MODE_AUTO, 'AUTO'),
                                 ('mode.manual', L.MODE_MANUAL, 'MANUAL')):
        h.pulse(request)
        h.expect('%s -> task %s' % (request, label), lambda m=mode: h.stat.task_mode == m)
        expect_task_mode_pins(h, mode, 'after ' + request)

    # asking for the mode task is already in sends nothing at all
    for request, mode in (('mode.manual', L.MODE_MANUAL),
                          ('mode.auto', L.MODE_AUTO),
                          ('mode.mdi', L.MODE_MDI)):
        h.nml_mode(mode)
        h.settle(0.1)
        serial = h.poll().echo_serial_number
        h.pulse(request)
        h.settle(0.3)
        h.check('%s while already in that mode sends no command' % request,
                h.poll().echo_serial_number == serial,
                'echo_serial_number %d -> %d' % (serial, h.stat.echo_serial_number))

    # status pins follow a mode change made by another client
    h.nml_mode(L.MODE_AUTO)
    expect_task_mode_pins(h, L.MODE_AUTO, 'after an NML mode change')
    h.nml_mode(L.MODE_MANUAL)


def task_mode_while_not_on(h):
    for state, label in ((L.STATE_ESTOP, 'ESTOP'), (L.STATE_OFF, 'OFF')):
        if state == L.STATE_ESTOP:
            h.machine_estop()
        else:
            h.machine_on()
            h.nml_state(L.STATE_OFF)
        h.nml_mode(L.MODE_MANUAL)
        h.pulse('mode.mdi')
        h.expect('mode.mdi is accepted while in %s' % label,
                 lambda: h.stat.task_mode == L.MODE_MDI)
        h.pulse('mode.manual')
        h.expect('mode.manual is accepted while in %s' % label,
                 lambda: h.stat.task_mode == L.MODE_MANUAL)


def motion_modes(h):
    h.machine_on()
    h.nml_mode(L.MODE_MANUAL)
    h.nml_home_all()

    h.pulse('mode.teleop')
    h.expect('mode.teleop -> motion TELEOP', lambda: h.stat.motion_mode == L.TRAJ_MODE_TELEOP)
    h.expect_pin('mode.is-teleop sets', 'mode.is-teleop', True)
    h.expect_pin('mode.is-joint clears in teleop', 'mode.is-joint', False)

    h.pulse('mode.joint')
    h.expect('mode.joint -> motion FREE', lambda: h.stat.motion_mode == L.TRAJ_MODE_FREE)
    h.expect_pin('mode.is-joint sets', 'mode.is-joint', True)
    h.expect_pin('mode.is-teleop clears in joint mode', 'mode.is-teleop', False)

    # in MDI, motion is in coordinated mode: neither teleop nor joint
    h.nml_mode(L.MODE_MDI)
    h.expect('MDI puts motion in COORD', lambda: h.stat.motion_mode == L.TRAJ_MODE_COORD)
    h.expect_pin('mode.is-joint is false in COORD', 'mode.is-joint', False)
    h.expect_pin('mode.is-teleop is false in COORD', 'mode.is-teleop', False)

    # status follows another client switching to teleop
    h.nml_mode(L.MODE_MANUAL)
    h.nml_teleop(1)
    h.expect_pin('mode.is-teleop follows an NML teleop enable', 'mode.is-teleop', True)
    h.nml_teleop(0)
    h.expect_pin('mode.is-joint follows an NML teleop disable', 'mode.is-joint', True)


run(startup_status,
    estop_and_power,
    power_on_refused_in_estop,
    inputs_are_edge_triggered,
    status_follows_other_clients,
    task_modes,
    task_mode_while_not_on,
    motion_modes)
