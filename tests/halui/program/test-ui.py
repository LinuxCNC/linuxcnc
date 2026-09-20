#!/usr/bin/env python3
"""halui: program control and program status.

Pins: program.run, program.pause, program.resume, program.step, program.stop,
      program.optional-stop.on, program.optional-stop.off,
      program.optional-stop.is-on,
      program.block-delete.on, program.block-delete.off,
      program.block-delete.is-on,
      program.is-idle, program.is-running, program.is-paused, abort

halui cannot open a program, so opening one is done over NML as setup.
"""

import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '_lib'))
from halui_test import run, close  # noqa: E402

import linuxcnc as L  # noqa: E402


def running(h):
    return h.stat.interp_state in (L.INTERP_READING, L.INTERP_WAITING)


def paused(h):
    return h.stat.interp_state == L.INTERP_PAUSED


def idle(h):
    return h.stat.interp_state == L.INTERP_IDLE


def x(h):
    return h.poll().actual_position[0]


def expect_program_pins(h, is_idle, is_running, is_paused, why):
    h.expect_pin('program.is-idle is %s %s' % (is_idle, why), 'program.is-idle', is_idle)
    h.expect_pin('program.is-running is %s %s' % (is_running, why), 'program.is-running', is_running)
    h.expect_pin('program.is-paused is %s %s' % (is_paused, why), 'program.is-paused', is_paused)


def to_origin(h):
    h.machine_on()
    h.nml_mdi('G20 G90 G0 X0 Y0 Z0')


def open_program(h, name):
    h.nml_mode(L.MODE_AUTO)
    h.cmd.program_open(os.path.join(HERE, name))
    h.cmd.wait_complete()
    if not h.wait_for(lambda: h.stat.file.endswith(name)):
        raise RuntimeError('setup: could not open ' + name)


def nml_run(h):
    # wait_complete() on a run only returns once the program has finished,
    # so for a short program just wait for task to take the command
    h.cmd.auto(L.AUTO_RUN, 0)
    if not h.wait_for(lambda: h.stat.echo_serial_number >= h.cmd.serial):
        raise RuntimeError('setup: task did not take the run command')


def expect_still(h, name):
    h.settle(0.3)
    a = x(h)
    h.settle(0.4)
    b = x(h)
    return h.check(name, close(a, b, 1e-6), 'X %.5f -> %.5f' % (a, b))


def run_without_a_program(h):
    h.machine_on()
    h.nml_mode(L.MODE_MANUAL)
    h.check('no program is open at startup', h.poll().file == '', 'file=%r' % h.stat.file)
    expect_program_pins(h, True, False, False, 'at startup')
    serial = h.poll().echo_serial_number
    h.pulse('program.run')
    h.settle(0.3)
    h.check('program.run with no program open sends nothing',
            h.poll().echo_serial_number == serial,
            'echo_serial_number %d -> %d' % (serial, h.stat.echo_serial_number))
    h.check('program.run with no program open leaves task in MANUAL',
            h.stat.task_mode == L.MODE_MANUAL, 'mode=%d' % h.stat.task_mode)


def run_pause_resume_stop(h):
    to_origin(h)
    open_program(h, 'long.ngc')
    h.nml_mode(L.MODE_MANUAL)

    h.pulse('program.run')
    h.expect('program.run switches task to AUTO by itself',
             lambda: h.stat.task_mode == L.MODE_AUTO)
    h.expect('program.run starts the program', lambda: running(h))
    h.expect('the program moves X', lambda: h.stat.actual_position[0] > 0.2)
    expect_program_pins(h, False, True, False, 'while running')

    h.pulse('program.pause')
    h.expect('program.pause pauses the program', lambda: paused(h))
    expect_program_pins(h, False, False, True, 'while paused')
    expect_still(h, 'motion stops while paused')

    h.pulse('program.pause')
    h.expect_steady('program.pause while paused keeps it paused', lambda: paused(h), duration=0.4)

    here = x(h)
    h.pulse('program.resume')
    h.expect('program.resume resumes the program', lambda: running(h))
    h.expect('motion continues after resume', lambda: h.stat.actual_position[0] > here + 0.05)
    expect_program_pins(h, False, True, False, 'after resume')

    h.pulse('program.resume')
    h.expect_steady('program.resume while running keeps it running', lambda: running(h), duration=0.4)

    h.pulse('program.stop')
    h.expect('program.stop ends the program', lambda: idle(h))
    expect_program_pins(h, True, False, False, 'after program.stop')
    expect_still(h, 'motion stops after program.stop')


def run_while_already_in_auto(h):
    to_origin(h)
    open_program(h, 'long.ngc')
    h.pulse('program.run')
    h.expect('program.run from AUTO starts the program', lambda: running(h))
    h.pulse('program.stop')
    h.expect('program.stop ends it', lambda: idle(h))


def pause_when_idle(h):
    # task accepts a pause with a program open but not running, and reports
    # the interpreter as PAUSED. That is task behaviour, not halui's; halui
    # passes the request on and its status pins report what task says.
    to_origin(h)
    open_program(h, 'long.ngc')
    h.pulse('program.pause')
    h.expect('program.pause with nothing running puts task in PAUSED', lambda: paused(h))
    expect_program_pins(h, False, False, True, 'after a pause while idle')
    h.pulse('program.stop')
    h.expect('program.stop leaves that paused state', lambda: idle(h))
    expect_program_pins(h, True, False, False, 'after stopping it')


def abort_pin(h):
    to_origin(h)
    open_program(h, 'long.ngc')
    nml_run(h)
    h.expect('the program runs (started over NML)', lambda: h.stat.actual_position[0] > 0.2)
    expect_program_pins(h, False, True, False, 'while a program started over NML runs')
    h.pulse('abort')
    h.expect('abort ends a running program', lambda: idle(h))
    expect_program_pins(h, True, False, False, 'after abort')
    expect_still(h, 'motion stops after abort')

    # abort also stops a paused program
    to_origin(h)
    open_program(h, 'long.ngc')
    nml_run(h)
    h.cmd.auto(L.AUTO_PAUSE)
    h.cmd.wait_complete()
    h.wait_for(lambda: paused(h))
    h.pulse('abort')
    h.expect('abort ends a paused program', lambda: idle(h))
    expect_program_pins(h, True, False, False, 'after aborting a paused program')

    # and an MDI move
    to_origin(h)
    h.nml_mode(L.MODE_MDI)
    h.cmd.mdi('G1 X3 F30')
    h.expect('an MDI move is under way', lambda: h.stat.actual_position[0] > 0.1)
    h.pulse('abort')
    h.expect('abort ends an MDI move', lambda: idle(h))
    expect_still(h, 'motion stops after aborting MDI')


def step_trace(h, do_step, steps=8):
    """Step through step.ngc, recording (X, interpreter state) after each step."""
    to_origin(h)
    open_program(h, 'step.ngc')
    trace = []
    for _ in range(steps):
        do_step()
        h.settle(0.6)
        s = h.poll()
        trace.append((round(s.actual_position[0], 4), s.interp_state))
    return trace


def step(h):
    # How far one step gets is task's business (it lags the presses while the
    # program starts up). halui's job is to turn each rising edge into exactly
    # one step, so compare it with stepping over NML.
    def nml_step():
        h.cmd.auto(L.AUTO_STEP)
        h.cmd.wait_complete()

    reference = step_trace(h, nml_step)
    h.nml_abort()
    via_pin = step_trace(h, lambda: h.pulse('program.step'))
    h.check('program.step steps exactly like an NML step, press for press',
            via_pin == reference, 'NML %s / pin %s' % (reference, via_pin))

    xs = [x for x, _ in via_pin]
    h.check('stepping walks through the whole program', 0.3 in xs, 'X after each step: %s' % xs)
    h.check('no step moves more than one line',
            all(b - a <= 0.1 + 1e-6 for a, b in zip([0.0] + xs, xs)), 'X after each step: %s' % xs)

    h.pulse('program.stop')
    h.expect('program.stop ends a stepped program', lambda: idle(h))


def optional_stop(h):
    h.machine_on()
    h.pulse('program.optional-stop.on')
    h.expect('optional-stop.on turns optional stop on in task', lambda: h.stat.optional_stop)
    h.expect_pin('optional-stop.is-on sets', 'program.optional-stop.is-on', True)

    to_origin(h)
    h.nml_mdi('G0 X0')
    open_program(h, 'optional-stop.ngc')
    nml_run(h)
    h.expect('with optional stop on, M1 pauses the program', lambda: paused(h))
    h.check('the move after M1 has not happened', close(x(h), 0.0, 1e-4), 'X=%.4f' % x(h))
    h.pulse('program.resume')
    h.expect('program.resume continues past M1',
             lambda: idle(h) and close(h.stat.actual_position[0], 0.5, 1e-4))

    h.pulse('program.optional-stop.off')
    h.expect('optional-stop.off turns optional stop off in task', lambda: not h.stat.optional_stop)
    h.expect_pin('optional-stop.is-on clears', 'program.optional-stop.is-on', False)

    to_origin(h)
    open_program(h, 'optional-stop.ngc')
    saw_pause = False
    nml_run(h)
    end = time.time() + 10
    while time.time() < end:
        h.poll()
        saw_pause = saw_pause or paused(h)
        if idle(h):
            break
        time.sleep(0.01)
    h.check('with optional stop off, M1 is skipped', not saw_pause and idle(h)
            and close(h.stat.actual_position[0], 0.5, 1e-4),
            'paused=%s X=%.4f' % (saw_pause, h.stat.actual_position[0]))

    h.cmd.set_optional_stop(1)
    h.expect_pin('optional-stop.is-on follows an NML change', 'program.optional-stop.is-on', True)
    h.cmd.set_optional_stop(0)
    h.expect_pin('optional-stop.is-on follows it back', 'program.optional-stop.is-on', False)


def block_delete(h):
    h.machine_on()
    h.pulse('program.block-delete.on')
    h.expect('block-delete.on turns block delete on in task', lambda: h.stat.block_delete)
    h.expect_pin('block-delete.is-on sets', 'program.block-delete.is-on', True)

    to_origin(h)
    open_program(h, 'block-delete.ngc')
    nml_run(h)
    h.wait_idle()
    h.check('with block delete on, the "/" line is skipped',
            close(h.poll().actual_position[2], 0.0, 1e-4), 'Z=%.4f' % h.stat.actual_position[2])

    h.pulse('program.block-delete.off')
    h.expect('block-delete.off turns block delete off in task', lambda: not h.stat.block_delete)
    h.expect_pin('block-delete.is-on clears', 'program.block-delete.is-on', False)

    to_origin(h)
    open_program(h, 'block-delete.ngc')
    nml_run(h)
    h.wait_idle()
    h.check('with block delete off, the "/" line runs',
            close(h.poll().actual_position[2], -1.0, 1e-4), 'Z=%.4f' % h.stat.actual_position[2])

    h.cmd.set_block_delete(1)
    h.expect_pin('block-delete.is-on follows an NML change', 'program.block-delete.is-on', True)
    h.cmd.set_block_delete(0)
    h.expect_pin('block-delete.is-on follows it back', 'program.block-delete.is-on', False)


run(run_without_a_program,
    run_pause_resume_stop,
    run_while_already_in_auto,
    pause_when_idle,
    abort_pin,
    step,
    optional_stop,
    block_delete)
