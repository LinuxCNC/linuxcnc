"""Shared harness for the halui pin tests.

Each test directory runs linuxcnc with its test-ui.py as the [DISPLAY]
program, so the harness starts once halui and task are both up. It:

  - Finds every pin halui exported and creates a mirror pin with the opposite
    direction on its own HAL component, wired straight to it. Tests drive
    halui's inputs and read its outputs by suffix:
        h.set('machine.on', 1)
        h.get('machine.is-on')

  - Talks to task through the stock linuxcnc python module. That is used only
    for test setup (opening a program, homing before a jog test, ...) and as
    ground truth for what task actually did -- never as the thing under test.

  - Records every check. A failed check is reported and the run carries on,
    so one run shows everything that is broken. The process exits non-zero at
    the end if anything failed.

The tests assert what halui does today. Where today's behaviour is a bug, the
check goes through known_bug() with an explanation: the suite is a baseline
that flags any change in behaviour, and fixing one of those bugs has to
update its test deliberately.

The tests only look at pins and task state, so the same suite applies
whether halui runs as its own process or inside task.
"""

import math
import os
import sys
import time
import traceback

import hal
import linuxcnc

# halui polls its pins and task's status every 20 ms. Holding an input for a
# few of those guarantees halui samples both edges.
HALUI_PERIOD = 0.02
HOLD = 0.15
TIMEOUT = 5.0

COMP = 'halui-test'


class Halui:

    def __init__(self):
        self.t0 = time.time()
        self.checks = 0
        self.failures = []
        self.known_bugs = []
        self.touched = set()

        # Every pin halui exported, by suffix: {'machine.on': (type, dir)},
        # and the value each input had before we wired it (halui's default).
        self.pins = {}
        self.defaults = {}
        for p in hal.get_info_pins():
            if p['NAME'].startswith('halui.'):
                sfx = p['NAME'][len('halui.'):]
                self.pins[sfx] = (p['TYPE'], p['DIRECTION'])
                if p['DIRECTION'] == hal.HAL_IN:
                    self.defaults[sfx] = p['VALUE']
        if not self.pins:
            raise RuntimeError('no halui pins found -- is halui running?')

        self.comp = hal.component(COMP)
        for sfx, (typ, d) in sorted(self.pins.items()):
            mirror = hal.HAL_OUT if d == hal.HAL_IN else hal.HAL_IN
            self.comp.newpin(sfx, typ, mirror)
            # Wiring our output to halui's input makes the signal carry our
            # value, so start from halui's own default (scales, deadbands,
            # count-enable...) rather than silently replacing it with 0.
            if d == hal.HAL_IN:
                self.comp[sfx] = self.defaults[sfx]
        self.comp.ready()

        # Pins the config already wired elsewhere cannot be driven from here.
        self.external = set()
        for sfx, (typ, d) in sorted(self.pins.items()):
            # hal.connect() does not raise when the link fails: it returns
            # True (hal_link() != 0). A failed link here means the config
            # already wired this halui pin.
            sig = 'ht.' + sfx
            hal.new_sig(sig, typ)
            if hal.connect('halui.' + sfx, sig):
                self.external.add(sfx)
                continue
            if hal.connect(COMP + '.' + sfx, sig):
                raise RuntimeError('could not wire %s.%s' % (COMP, sfx))

        self.stat = linuxcnc.stat()
        self.cmd = linuxcnc.command()
        self.errors = linuxcnc.error_channel()
        self.poll()
        self.log('harness up: %d halui pins, %d wired externally'
                 % (len(self.pins), len(self.external)))

    # -- reporting ---------------------------------------------------------

    def log(self, msg):
        print('%7.3f: %s' % (time.time() - self.t0, msg), flush=True)

    def section(self, name):
        self.log('')
        self.log('==== %s ====' % name)

    def check(self, name, ok, detail=''):
        self.checks += 1
        self.log('%s %s%s' % ('PASS' if ok else 'FAIL', name,
                              (' -- ' + detail) if detail else ''))
        if not ok:
            self.failures.append(name)
        return ok

    def known_bug(self, name, ok, explanation):
        """Assert today's buggy behaviour, and say why it is a bug."""
        self.known_bugs.append(name)
        return self.check('KNOWN-BUG ' + name, ok, explanation)

    def finish(self):
        here = os.path.dirname(os.path.abspath(sys.argv[0]))
        with open(os.path.join(here, 'touched-pins.txt'), 'w') as f:
            for p in sorted(self.touched):
                f.write(p + '\n')
        self.log('')
        self.log('%d checks, %d failed, %d known bugs, %d/%d halui pins exercised'
                 % (self.checks, len(self.failures), len(self.known_bugs),
                    len(self.touched & set(self.pins)), len(self.pins)))
        for f in self.failures:
            self.log('  FAILED: ' + f)
        sys.exit(1 if self.failures else 0)

    # -- halui pins --------------------------------------------------------

    def has(self, sfx):
        return sfx in self.pins

    def _pin(self, sfx, want_input):
        if sfx not in self.pins:
            raise KeyError('halui has no pin %r' % sfx)
        is_input = self.pins[sfx][1] == hal.HAL_IN
        if is_input != want_input:
            raise KeyError('halui.%s is an %s pin' % (sfx, 'input' if is_input else 'output'))
        self.touched.add(sfx)

    def set(self, sfx, value):
        self._pin(sfx, want_input=True)
        if sfx in self.external:
            raise KeyError('halui.%s is wired by the config; cannot drive it' % sfx)
        self.comp[sfx] = value

    def get(self, sfx):
        self._pin(sfx, want_input=False)
        return self.comp[sfx]

    def default(self, sfx):
        """halui's value for an input pin before the harness wired it."""
        self._pin(sfx, want_input=True)
        return self.defaults[sfx]

    def pulse(self, sfx, hold=HOLD):
        """Rising edge, held long enough for halui to see it, then released."""
        self.set(sfx, 1)
        time.sleep(hold)
        self.set(sfx, 0)
        time.sleep(hold)

    def settle(self, t=HOLD):
        time.sleep(t)

    # -- waiting -----------------------------------------------------------

    def poll(self):
        self.stat.poll()
        return self.stat

    def wait_for(self, fn, timeout=TIMEOUT):
        end = time.time() + timeout
        while True:
            self.poll()
            try:
                if fn():
                    return True
            except Exception:
                pass
            if time.time() > end:
                return False
            time.sleep(HALUI_PERIOD / 2)

    def expect(self, name, fn, timeout=TIMEOUT, detail=None):
        ok = self.wait_for(fn, timeout)
        return self.check(name, ok, detail() if callable(detail) else (detail or ''))

    def stays(self, fn, duration=0.5):
        """True if fn holds for the whole duration (records nothing)."""
        end = time.time() + duration
        while time.time() < end:
            self.poll()
            if not fn():
                return False
            time.sleep(HALUI_PERIOD / 2)
        return True

    def expect_steady(self, name, fn, duration=0.5, detail=None):
        """fn must stay true for the whole duration."""
        ok = self.stays(fn, duration)
        return self.check(name, ok, detail() if callable(detail) else (detail or ''))

    def expect_pin(self, name, sfx, want, timeout=TIMEOUT, tol=1e-6):
        is_float = self.pins[sfx][0] == hal.HAL_FLOAT

        def match():
            v = self.get(sfx)
            if is_float:
                return abs(v - want) <= tol
            return v == want
        return self.expect(name, match, timeout,
                           detail=lambda: 'halui.%s = %r, want %r' % (sfx, self.get(sfx), want))

    def drain_errors(self):
        out = []
        while True:
            e = self.errors.poll()
            if not e:
                return out
            out.append(e[1])

    def reset_between_sections(self):
        for sfx in sorted(self.touched):
            if self.pins[sfx][1] == hal.HAL_IN and sfx not in self.external:
                self.comp[sfx] = self.defaults[sfx]
        time.sleep(0.3)
        self.nml_abort()

    # -- task setup, via NML (not under test) ------------------------------

    def nml_abort(self):
        self.cmd.abort()
        self.cmd.wait_complete()
        self.wait_for(lambda: self.stat.interp_state == linuxcnc.INTERP_IDLE)

    def nml_state(self, state):
        # task has no distinct OFF state: a request for OFF reads back as
        # ESTOP_RESET
        want = linuxcnc.STATE_ESTOP_RESET if state == linuxcnc.STATE_OFF else state
        self.cmd.state(state)
        self.cmd.wait_complete()
        return self.wait_for(lambda: self.stat.task_state == want)

    def machine_on(self):
        if self.poll().task_state == linuxcnc.STATE_ON:
            return
        if self.stat.task_state == linuxcnc.STATE_ESTOP:
            self.nml_state(linuxcnc.STATE_ESTOP_RESET)
        if not self.nml_state(linuxcnc.STATE_ON):
            raise RuntimeError('setup: could not turn the machine on')

    def machine_estop(self):
        self.nml_state(linuxcnc.STATE_ESTOP)

    def nml_mode(self, mode):
        self.cmd.mode(mode)
        self.cmd.wait_complete()
        if not self.wait_for(lambda: self.stat.task_mode == mode):
            raise RuntimeError('setup: could not switch to mode %d' % mode)

    def nml_teleop(self, enable):
        self.cmd.teleop_enable(enable)
        self.cmd.wait_complete()
        want = linuxcnc.TRAJ_MODE_TELEOP if enable else linuxcnc.TRAJ_MODE_FREE
        if not self.wait_for(lambda: self.stat.motion_mode == want):
            raise RuntimeError('setup: could not set teleop=%d' % enable)

    def nml_home_all(self):
        # Note: once every joint is homed, task switches an identity-kins
        # machine to teleop by itself.
        self.nml_mode(linuxcnc.MODE_MANUAL)
        self.cmd.home(-1)
        self.cmd.wait_complete()
        if not self.wait_for(lambda: all(self.stat.homed[:self.stat.joints]), 20):
            raise RuntimeError('setup: homing failed')

    def nml_unhome_all(self):
        # motion refuses to unhome in teleop ("must be in joint mode or
        # disabled to unhome"), which is where homing leaves the machine
        self.nml_mode(linuxcnc.MODE_MANUAL)
        if self.poll().motion_mode == linuxcnc.TRAJ_MODE_TELEOP:
            self.nml_teleop(0)
        self.cmd.unhome(-1)
        self.cmd.wait_complete()
        if not self.wait_for(lambda: not any(self.stat.homed[:self.stat.joints])):
            raise RuntimeError('setup: unhoming failed')

    def wait_idle(self, timeout=20):
        return self.wait_for(lambda: self.stat.interp_state == linuxcnc.INTERP_IDLE
                             and self.stat.exec_state == linuxcnc.EXEC_DONE, timeout)

    def nml_mdi(self, code, timeout=20):
        self.nml_mode(linuxcnc.MODE_MDI)
        self.cmd.mdi(code)
        self.cmd.wait_complete()
        if not self.wait_idle(timeout):
            raise RuntimeError('setup: MDI %r did not finish' % code)

    def joint_pos(self, j):
        return self.stat.joint_actual_position[j]

    def axis_pos(self, a):
        return self.stat.actual_position['xyzabcuvw'.index(a)]

    def wait_joint_still(self, j, timeout=10):
        last = [None]
        def still():
            p = self.joint_pos(j)
            done = last[0] is not None and abs(p - last[0]) < 1e-9 and self.stat.inpos
            last[0] = p
            return done
        return self.wait_for(still, timeout)


def run(*sections):
    """Run each section function against one harness; report at the end.

    Between sections every input a test drove goes back to halui's default and
    task is aborted, so a section cannot leave a held jog, a paused program or
    a half-finished MDI behind for the next one.
    """
    h = Halui()
    for fn in sections:
        h.section(fn.__name__)
        try:
            fn(h)
        except Exception:
            h.log(traceback.format_exc())
            h.check('%s ran to completion' % fn.__name__, False, 'exception, see above')
        h.reset_between_sections()
    h.finish()


def close(a, b, tol=1e-6):
    return abs(a - b) <= tol


INCH_PER_MM = 1 / 25.4
