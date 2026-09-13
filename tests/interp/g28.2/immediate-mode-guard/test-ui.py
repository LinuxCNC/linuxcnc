#!/usr/bin/env python3

"""
Regression test for the trajectory-mode guards on *immediate* home / unhome
commands -- the ones the GUI Home/Unhome buttons, halui and linuxcncrsh send.
Immediate commands reach emcTaskIssueCommand() but are never followed by
emcTaskCheckPostconditions(), so the G28.2 queued-home sequencing
(EMC_TASK_EXEC::WAITING_FOR_HOMING, which dips motion into FREE and restores
the prior mode) never runs for them. They must therefore behave exactly as
they did before PR #4172.

Two things are checked, neither visible from outside without provoking it
(motion does not change the trajectory mode by itself for a single-joint
home or unhome, so any mode change seen here came from task):

 1. An immediate *unhome* from teleop is refused by motion ("must be in
    joint mode or disabled to unhome"). Task must not dip into FREE ahead
    of it and let it through, and must not touch the mode.

 2. An immediate *home* from teleop is refused with "must be in joint mode
    to home", and the mode is left alone. do_homing() only advances while
    motion is in FREE (control.c), and manual mode on an all-homed machine
    is TELEOP, so accepting the command here would set the homing state
    machine up and then never run it -- a silently-dropped home (found in
    PR #4172 review of command.c). Before the fix the relaxed
    EMCMOT_JOINT_HOME guard ("... or idle") accepted it.

Config is trivkins so the machine can sit in TELEOP without being fully
homed; the mode guard under test is kinematics-independent.
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

# Get into a coordinated (teleop) mode -- what a stray FREE dip would destroy.
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

# 2. Immediate home from teleop. Must be refused with the joint-mode error,
#    mode untouched. Before the command.c fix this was silently accepted and
#    then never ran (do_homing() only advances in FREE).
drain_errors()
c.home(0)
time.sleep(1.0)
poll()
errs = drain_errors()
if not any("joint mode" in m for m in errs):
    fail(
        "immediate home from teleop was not refused (errors={!r}). do_homing() "
        "only runs in FREE, so an accepted command here is a silently-dropped "
        "home".format(errs)
    )
if s.motion_mode != mode_before:
    fail(
        "immediate home changed the trajectory mode {} -> {}".format(
            mode_before, s.motion_mode
        )
    )
homing_flags = [s.joint[j]["homing"] for j in range(3)]
if any(homing_flags):
    fail(
        "immediate home from teleop left a joint in a homing state "
        "(homing={}) -- it was accepted but cannot run".format(homing_flags)
    )
print("PASS: an immediate home from teleop is refused, mode untouched")

# A legitimate home from joint mode must still work afterwards -- i.e. the
# refused command left nothing stuck (e.g. get_homing_is_active() latched).
c.teleop_enable(0)
time.sleep(0.3)
c.unhome(0)
if not wait_homed([0, 1, 1]):
    fail("could not unhome joint 0 from joint mode after the refused home")
c.home(0)
if not wait_homed([1, 1, 1]):
    fail("a legitimate home from joint mode did not run after the refused home")
print("PASS: homing still works normally from joint mode")

print("done! it all worked")
sys.exit(0)
