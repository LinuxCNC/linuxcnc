#!/usr/bin/env python3
"""halui pins wired from a regular HALFILE.

The startup script runs HAL files only after halui is ready, and configs
rely on that ordering. This config nets halui pins in a HALFILE (see
halui-nets.hal); if halui's pins did not exist yet, linuxcnc would not get
as far as running this script.
"""

import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '_lib'))
from halui_test import run, HOLD  # noqa: E402

import hal  # noqa: E402
import linuxcnc as L  # noqa: E402

NETTED = {'machine.is-on', 'estop.activate', 'estop.reset', 'mode.mdi'}


def pulse_signal(h, sig, pin):
    h.touched.add(pin)
    hal.set_s(sig, True)
    time.sleep(HOLD)
    hal.set_s(sig, False)
    time.sleep(HOLD)


def wiring(h):
    h.check('linuxcnc started with halui pins netted from a HALFILE', True)
    h.check('exactly the netted pins are wired outside the harness',
            h.external == NETTED, 'external: %s' % sorted(h.external))


def netted_pins_work(h):
    h.expect('the machine-is-on-led signal is false in ESTOP',
             lambda: not hal.get_value('machine-is-on-led'))

    pulse_signal(h, 'remote-estop-reset', 'estop.reset')
    h.expect('a pulse on the netted estop.reset resets estop',
             lambda: h.stat.task_state == L.STATE_ESTOP_RESET)

    h.machine_on()
    h.expect('the machine-is-on-led signal follows the machine turning on',
             lambda: hal.get_value('machine-is-on-led'))

    h.nml_mode(L.MODE_MANUAL)
    pulse_signal(h, 'remote-mode-mdi', 'mode.mdi')
    h.expect('a pulse on the netted mode.mdi switches task to MDI',
             lambda: h.stat.task_mode == L.MODE_MDI)

    pulse_signal(h, 'remote-estop', 'estop.activate')
    h.expect('a pulse on the netted estop.activate estops the machine',
             lambda: h.stat.task_state == L.STATE_ESTOP)
    h.expect('the machine-is-on-led signal drops with it',
             lambda: not hal.get_value('machine-is-on-led'))

    try:
        h.set('estop.activate', 1)
        h.check('the harness refuses to drive a pin the config wired', False)
    except KeyError:
        h.check('the harness refuses to drive a pin the config wired', True)


run(wiring, netted_pins_work)
