#!/usr/bin/env python3

"""
Regression test: an immediate (GUI-path) home or unhome must not disturb the
trajectory mode, and must not quietly gain permissions it never had.

The G28.2 sequencing dips motion into FREE for the duration of a queued home
(do_homing() only advances there) and restores the previous mode afterwards,
from the EMC_TASK_EXEC::WAITING_FOR_HOMING poll.

That poll is only ever reached through emcTaskCheckPostconditions(), which
task calls only for commands taken off the interp_list. The GUI's Home and
Unhome buttons, halui and linuxcncrsh all send *immediate* commands: those
reach emcTaskIssueCommand() but are never followed by
emcTaskCheckPostconditions(). An earlier revision of this branch dipped for
those too, which had two effects, both regressions against the pre-G28
behaviour:

 1. the mode was dipped to FREE and never restored, silently stranding the
    machine in joint mode; and
 2. because the dip ran *before* the command was issued, an immediate unhome
    started succeeding from teleop, where motion deliberately refuses it
    ("must be in joint mode or disabled to unhome", the EMCMOT_JOINT_UNHOME
    case in command.c).

The sequencing is now scoped to the queued path, and with G28.3 dropped from
this PR an unhome has no queued path at all. Both are checked here, because
neither the scoping nor the drop is visible from the outside: motion does not
change the trajectory mode by itself for a single-joint home or unhome, so
any mode change observed here comes from task.
"""

import linuxcnc
import hal

import sys
import time

h = hal.component("python-ui")
h.ready()

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


def poll():
    s.poll()


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def wait_homed(expected, timeout=10.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        poll()
        if list(s.homed[:3]) == expected:
            return True
        time.sleep(0.01)
    return False


def drain_errors():
    msgs = []
    while True:
        err = e.poll()
        if not err:
            return msgs
        msgs.append(err[1])


c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MANUAL)
c.home(0)
c.home(1)
c.home(2)
if not wait_homed([1, 1, 1]):
    fail("initial home-all did not home all joints: {}".format(list(s.homed[:3])))

# Get into a coordinated (teleop) mode -- what the stray dip destroyed.
c.teleop_enable(1)
time.sleep(0.5)
poll()
mode_before = s.motion_mode
if mode_before != linuxcnc.TRAJ_MODE_TELEOP:
    fail("setup did not reach teleop mode (motion_mode={})".format(mode_before))
drain_errors()

# 1. Immediate unhome from teleop. Motion refuses this by design; task must
#    not dip into FREE first and thereby let it through.
c.unhome(0)
time.sleep(1.0)
poll()

if list(s.homed[:3]) != [1, 1, 1]:
    fail(
        "immediate unhome from teleop went through (homed={}) -- task dipped "
        "into FREE before issuing it, bypassing motion's \"must be in joint "
        "mode or disabled to unhome\" guard".format(list(s.homed[:3]))
    )
if s.motion_mode != mode_before:
    fail(
        "immediate unhome changed the trajectory mode {} -> {} and never "
        "restored it (1=FREE 2=COORD 3=TELEOP)".format(mode_before, s.motion_mode)
    )
if not drain_errors():
    fail("immediate unhome from teleop was silently ignored -- expected motion's refusal")
print("PASS: an immediate unhome from teleop is refused, mode untouched")

# 2. Immediate home from teleop. Motion permits this when idle (see the
#    EMCMOT_JOINT_HOME guard), so it must succeed -- but still without task
#    touching the trajectory mode.
c.home(0)
if not wait_homed([1, 1, 1]):
    fail("immediate home(0) did not complete")
time.sleep(0.8)
poll()
if s.motion_mode != mode_before:
    fail(
        "immediate home changed the trajectory mode {} -> {} and never "
        "restored it".format(mode_before, s.motion_mode)
    )
print("PASS: an immediate home leaves the trajectory mode untouched")

print("done! it all worked")
sys.exit(0)
