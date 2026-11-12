#!/usr/bin/env python3

# Check that the status pins Task exports (task.*) match the halui pins
# they replace, in a range of machine states.

import linuxcnc
import hal
import time
import sys
import os

program_start = time.time()

def log(msg):
    delta_t = time.time() - program_start
    print("%.3f: %s" % (delta_t, msg))
    sys.stdout.flush()


# task pin -> halui pin
pairs = {
    'task.machine.units-per-mm': 'halui.machine.units-per-mm',
    'task.estop.is-activated': 'halui.estop.is-activated',
    'task.tool.number': 'halui.tool.number',
    'task.mode.is-manual': 'halui.mode.is-manual',
    'task.mode.is-auto': 'halui.mode.is-auto',
    'task.mode.is-mdi': 'halui.mode.is-mdi',
    'task.mode.is-joint': 'halui.mode.is-joint',
    'task.program.is-idle': 'halui.program.is-idle',
    'task.program.is-running': 'halui.program.is-running',
    'task.program.is-paused': 'halui.program.is-paused',
    'task.program.optional-stop.is-on': 'halui.program.optional-stop.is-on',
    'task.program.block-delete.is-on': 'halui.program.block-delete.is-on',
    'task.max-velocity.value': 'halui.max-velocity.value',
    'task.feed-override.value': 'halui.feed-override.value',
    'task.rapid-override.value': 'halui.rapid-override.value',
    'task.spindle.0.override.value': 'halui.spindle.0.override.value',
    'task.tool.diameter': 'halui.tool.diameter',
}
for j in range(3):
    for p in ('on-soft-min-limit', 'on-soft-max-limit', 'override-limits'):
        pairs['task.joint.%d.%s' % (j, p)] = 'halui.joint.%d.%s' % (j, p)
for a in 'xyz':
    for p in ('pos-feedback', 'pos-relative'):
        pairs['task.axis.%s.%s' % (a, p)] = 'halui.axis.%s.%s' % (a, p)


def same(a, b):
    if isinstance(a, float) or isinstance(b, float):
        return abs(a - b) < 1e-9
    return a == b


def check(what, expect={}, moving=False):
    """Wait for every task pin to match its halui pin (halui polls, so it
    lags), and for the pins in expect to have the given values. While the
    machine moves, halui's positions are always a poll behind, so those
    are not compared."""
    timeout = 5.0
    start = time.time()
    while True:
        bad = []
        for t, h in pairs.items():
            if moving and '.pos-' in t:
                continue
            tv = hal.get_value(t)
            hv = hal.get_value(h)
            if not same(tv, hv):
                bad.append("%s=%r but %s=%r" % (t, tv, h, hv))
        for t, v in expect.items():
            tv = hal.get_value(t)
            if not same(tv, v):
                bad.append("%s=%r, expected %r" % (t, tv, v))
        if not bad:
            log("%s: ok" % what)
            return
        if time.time() - start > timeout:
            log("%s: FAILED" % what)
            for b in bad:
                log("    " + b)
            sys.exit(1)
        time.sleep(0.05)


def wait_for(what, cond):
    start = time.time()
    while time.time() - start < 10.0:
        s.poll()
        if cond():
            return
        time.sleep(0.05)
    log("timeout waiting for %s" % what)
    sys.exit(1)


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete()

c.mode(linuxcnc.MODE_MANUAL)
c.wait_complete()
check("manual", {'task.mode.is-manual': True, 'task.program.is-idle': True,
                 'task.machine.units-per-mm': 1 / 25.4})

for j in range(3):
    c.home(j)
    c.wait_complete()
wait_for("homed", lambda: all(s.homed[:3]))
c.teleop_enable(1)
c.wait_complete()
wait_for("teleop", lambda: s.motion_mode == linuxcnc.TRAJ_MODE_TELEOP)
check("teleop", {'task.mode.is-joint': False})

c.mode(linuxcnc.MODE_MDI)
c.wait_complete()
for cmd in ("G10 L2 P2 X1 Y2 Z3 R30", "G55", "T1 M6", "G43", "G92 X0.5",
            "G0 X1 Y1 Z-1"):
    c.mdi(cmd)
    c.wait_complete()
wait_for("move done", lambda: s.inpos and s.interp_state == linuxcnc.INTERP_IDLE)
check("mdi with offsets and tool 1", {'task.mode.is-mdi': True,
                                      'task.tool.diameter': 0.25,
                                      'task.tool.number': 1})

c.mdi("T2 M6")
c.wait_complete()
check("tool 2", {'task.tool.diameter': 0.375, 'task.tool.number': 2})

c.feedrate(0.7)
c.rapidrate(0.4)
c.spindleoverride(0.8, 0)
c.maxvel(2.0)
c.set_optional_stop(True)
c.set_block_delete(True)
c.wait_complete()
check("overrides", {'task.feed-override.value': 0.7,
                    'task.rapid-override.value': 0.4,
                    'task.spindle.0.override.value': 0.8,
                    'task.max-velocity.value': 2.0,
                    'task.program.optional-stop.is-on': True,
                    'task.program.block-delete.is-on': True})
c.set_optional_stop(False)
c.set_block_delete(False)
c.wait_complete()
check("opt stop / block delete off",
      {'task.program.optional-stop.is-on': False,
       'task.program.block-delete.is-on': False})

c.mode(linuxcnc.MODE_AUTO)
c.wait_complete()
c.program_open(os.path.abspath("slow.ngc"))
c.wait_complete()
c.auto(linuxcnc.AUTO_RUN, 0)
wait_for("program running", lambda: s.interp_state in
         (linuxcnc.INTERP_READING, linuxcnc.INTERP_WAITING) and s.current_vel > 0)
check("running", {'task.mode.is-auto': True, 'task.program.is-running': True},
      moving=True)

c.auto(linuxcnc.AUTO_PAUSE)
wait_for("program paused", lambda: s.interp_state == linuxcnc.INTERP_PAUSED)
check("paused", {'task.program.is-paused': True})

c.abort()
c.wait_complete()
wait_for("program idle", lambda: s.interp_state == linuxcnc.INTERP_IDLE)
check("aborted", {'task.program.is-idle': True})

c.state(linuxcnc.STATE_OFF)
c.state(linuxcnc.STATE_ESTOP)
c.wait_complete()
check("estop", {'task.estop.is-activated': True})

log("all pins match")
sys.exit(0)
