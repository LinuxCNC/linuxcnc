#!/usr/bin/env python3
# halui inside task and an NML client side by side:
# - halui.max-velocity is limited by [TRAJ]MAX_LINEAR_VELOCITY, not 1;
# - a halui command that task refuses does not turn the status of the
#   client's own command into ERROR, and a halui command does not clear
#   the ERROR of a client command that failed.

import os
import sys
import time

import hal
import linuxcnc

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()
h = hal.component("test-ui")
h.newpin("home", hal.Type.BOOL, hal.Dir.OUT)
h.newpin("mv-counts", hal.Type.SINT, hal.Dir.OUT)
h.ready()
os.system("halcmd net home test-ui.home halui.joint.0.home")
os.system("halcmd net mv-counts test-ui.mv-counts halui.max-velocity.counts")
os.system("halcmd setp halui.max-velocity.count-enable 1")
os.system("halcmd setp halui.max-velocity.scale 0.01")
os.system("halcmd setp halui.max-velocity.direct-value 1")

fails = 0


def check(name, ok, detail=''):
    global fails
    print('%s: %s %s' % ('ok' if ok else 'FAIL', name, detail), flush=True)
    if not ok:
        fails += 1


def wait(fn, timeout=5.0):
    end = time.time() + timeout
    while time.time() < end:
        s.poll()
        if fn():
            return True
        time.sleep(0.01)
    s.poll()
    return fn()


def errors():
    msgs = []
    while True:
        m = e.poll()
        if not m:
            return msgs
        msgs.append(m[1])


def settle():
    # give task a few cycles to dispatch a halui command
    time.sleep(0.2)


wait(lambda: s.task_state != 0, 10)
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
check('machine on', c.wait_complete() == linuxcnc.RCS_DONE)

# --- max velocity above 1 unit/s through the counts pins
h['mv-counts'] = 250
check('max-velocity counts 250 gives 2.5', wait(lambda: abs(s.max_velocity - 2.5) < 1e-9), s.max_velocity)
h['mv-counts'] = 1000
check('max-velocity capped at MAX_LINEAR_VELOCITY', wait(lambda: abs(s.max_velocity - 4) < 1e-9), s.max_velocity)
errors()

# --- a refused halui command leaves the client's DONE alone
c.state(linuxcnc.STATE_OFF)
check('machine off', c.wait_complete() == linuxcnc.RCS_DONE)
h['home'] = 1
settle()
h['home'] = 0
msgs = errors()
check('halui home refused while off', msgs != [], msgs)
check('client command still DONE after the refused halui command',
      c.wait_complete() == linuxcnc.RCS_DONE)
s.poll()
check('status not ERROR', s.state != linuxcnc.RCS_ERROR, s.state)

# --- a halui command leaves the client's ERROR alone
c.home(0)
check('client home refused while off', c.wait_complete() == linuxcnc.RCS_ERROR)
errors()
h['mv-counts'] = 150
check('halui max-velocity applied', wait(lambda: abs(s.max_velocity - 1.5) < 1e-9), s.max_velocity)
check('client command still ERROR after a halui command',
      c.wait_complete() == linuxcnc.RCS_ERROR)

errors()
print('%d failures' % fails, flush=True)
os._exit(1 if fails else 0)
