#!/usr/bin/env python3
"""halui: [HALUI]MDI_COMMAND buttons.

Pins: mdi-command-NN, halui-mdi-is-running

halui switches task to MDI, runs the command, and switches back to the mode
it found once task reports DONE. tests/halui/mdi covers the basic round trip;
this covers the running flag, queueing, and the paths where task refuses.

MDI_COMMAND 00: G0 X1       01: G0 X0
            02: G1 X2 F60   03: G1 X0 F60   (two seconds each)
"""

import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '_lib'))
from halui_test import run, close  # noqa: E402

import linuxcnc as L  # noqa: E402

MODES = {L.MODE_MANUAL: 'MANUAL', L.MODE_AUTO: 'AUTO', L.MODE_MDI: 'MDI'}


def x(h):
    return h.stat.actual_position[0]


def expect_x(h, want, why, timeout=10):
    return h.expect('X reaches %g %s' % (want, why),
                    lambda: close(x(h), want, 1e-4) and h.stat.inpos, timeout,
                    detail=lambda: 'X=%.4f' % x(h))


def expect_mode(h, mode, why, timeout=5):
    return h.expect('task is back in %s %s' % (MODES[mode], why),
                    lambda: h.stat.task_mode == mode, timeout,
                    detail=lambda: 'mode=%s' % MODES.get(h.stat.task_mode, h.stat.task_mode))


def at_x0(h, mode):
    h.machine_on()
    h.nml_mdi('G20 G90 G0 X0 Y0 Z0')
    h.nml_mode(mode)


def pins(h):
    for n in range(4):
        h.check('mdi-command-%02d exists' % n, h.has('mdi-command-%02d' % n))
    h.check('there is no pin beyond the configured commands', not h.has('mdi-command-04'))
    h.check('halui-mdi-is-running exists because MDI commands are configured',
            h.has('halui-mdi-is-running'))
    h.expect_pin('halui-mdi-is-running is false at startup', 'halui-mdi-is-running', False)


def round_trip_from_each_mode(h):
    for mode in (L.MODE_MANUAL, L.MODE_AUTO, L.MODE_MDI):
        at_x0(h, mode)
        h.pulse('mdi-command-00')
        expect_x(h, 1, 'after mdi-command-00 from %s' % MODES[mode])
        expect_mode(h, mode, 'after the command')
        h.expect_pin('halui-mdi-is-running clears after the command from %s' % MODES[mode],
                     'halui-mdi-is-running', False)


def running_flag(h):
    at_x0(h, L.MODE_MANUAL)
    h.pulse('mdi-command-02')
    h.expect('task switches to MDI for the command', lambda: h.stat.task_mode == L.MODE_MDI)
    h.expect_pin('halui-mdi-is-running sets while the command runs', 'halui-mdi-is-running', True)
    h.expect('the command is moving X', lambda: 0.2 < x(h) < 1.8)
    h.check('task is still in MDI mid-command', h.stat.task_mode == L.MODE_MDI)
    expect_x(h, 2, 'at the end of mdi-command-02')
    expect_mode(h, L.MODE_MANUAL, 'once the command is done')
    h.expect_pin('halui-mdi-is-running clears when it is done', 'halui-mdi-is-running', False)


def queued_commands(h):
    at_x0(h, L.MODE_MANUAL)
    h.pulse('mdi-command-02')
    h.expect('first command under way', lambda: x(h) > 0.2)
    h.pulse('mdi-command-03')
    furthest = 0.0
    end = time.time() + 10
    while time.time() < end:
        h.poll()
        furthest = max(furthest, x(h))
        if h.stat.task_mode != L.MODE_MDI:
            break
        time.sleep(0.01)
    h.check('both queued commands ran in order (X went to 2 and back to 0)',
            furthest > 1.99 and close(x(h), 0, 1e-4),
            'furthest X=%.4f, final X=%.4f' % (furthest, x(h)))
    h.check('the mode is restored only after the whole queue is done',
            close(x(h), 0, 1e-4), 'mode left MDI at X=%.4f' % x(h))
    expect_mode(h, L.MODE_MANUAL, 'after the queue')
    h.expect_pin('halui-mdi-is-running clears after the queue', 'halui-mdi-is-running', False)


def edge_only(h):
    at_x0(h, L.MODE_MANUAL)
    h.set('mdi-command-00', 1)
    expect_x(h, 1, 'while mdi-command-00 is held')
    expect_mode(h, L.MODE_MANUAL, 'while it is still held')
    h.nml_mdi('G0 X0')
    h.nml_mode(L.MODE_MANUAL)
    h.expect_steady('a held mdi-command-00 does not run again',
                    lambda: close(x(h), 0, 1e-4) and h.stat.task_mode == L.MODE_MANUAL,
                    duration=0.8)
    h.set('mdi-command-00', 0)


def refused_by_task(h):
    # In ESTOP task accepts the switch to MDI but refuses the command itself.
    at_x0(h, L.MODE_MANUAL)
    h.machine_estop()
    h.nml_mode(L.MODE_MANUAL)
    h.drain_errors()
    h.pulse('mdi-command-00')
    h.settle(1.0)
    s = h.poll()
    h.log('mdi-command-00 in ESTOP: mode=%s status=%d X=%.4f is-running=%s errors=%r'
          % (MODES.get(s.task_mode), s.state, x(h), h.get('halui-mdi-is-running'),
             h.drain_errors()))
    h.check('the command does not move anything in ESTOP', close(x(h), 0, 1e-4))
    h.check('task reports the refused command as an error', s.state == L.RCS_ERROR,
            'status=%d' % s.state)
    # halui restores the old mode only on RCS DONE (modify_hal_pins). The
    # refused command leaves task in ERROR, so nothing is restored and the
    # running flag never drops.
    h.known_bug('task is left in MDI after an MDI button is refused',
                h.stays(lambda: h.stat.task_mode == L.MODE_MDI, 1.0),
                'halui only restores the previous mode when task status is DONE; '
                'on ERROR it waits forever. Should be back in MANUAL')
    h.known_bug('halui-mdi-is-running stays true after the refused command',
                h.stays(lambda: h.get('halui-mdi-is-running'), 1.0),
                'halui_sent_mdi is cleared only on DONE; should be false')

    # The stale state resolves on the next command that finishes with DONE,
    # whatever it is. Here that is powering the machine on, which therefore
    # also switches task out of MDI -- long after the button press.
    h.machine_on()
    h.known_bug('turning the machine on later switches task back to MANUAL',
                h.wait_for(lambda: h.stat.task_mode == L.MODE_MANUAL),
                'the delayed restore from the refused MDI button fires on the next DONE, '
                'from an unrelated command')
    h.expect_pin('halui-mdi-is-running clears after that', 'halui-mdi-is-running', False)


def while_program_runs(h):
    # task refuses the switch to MDI while a program runs
    at_x0(h, L.MODE_AUTO)
    here = os.path.dirname(os.path.abspath(__file__))
    h.cmd.program_open(os.path.join(here, 'long.ngc'))
    h.cmd.wait_complete()
    h.cmd.auto(L.AUTO_RUN, 0)
    h.expect('the program is running', lambda: x(h) > 0.2)
    h.pulse('mdi-command-00')
    h.settle(0.5)
    running_flag = h.get('halui-mdi-is-running')
    h.log('mdi-command-00 while a program runs: mode=%s is-running=%s'
          % (MODES.get(h.poll().task_mode), running_flag))
    h.check('task stays in AUTO', h.stat.task_mode == L.MODE_AUTO)
    h.known_bug('halui-mdi-is-running is true although no MDI command was sent',
                running_flag,
                'sendMdiCommand() sets halui_sent_mdi before task has refused the '
                'switch to MDI; should stay false')
    h.check('the program keeps running', h.stat.interp_state != L.INTERP_IDLE)
    h.nml_abort()
    h.expect_pin('halui-mdi-is-running is clear once the program is gone',
                 'halui-mdi-is-running', False)
    h.check('task is still in AUTO afterwards', h.poll().task_mode == L.MODE_AUTO)


run(pins,
    round_trip_from_each_mode,
    running_flag,
    queued_commands,
    edge_only,
    refused_by_task,
    while_program_runs)
