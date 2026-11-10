#!/usr/bin/env python3

# Drive Task through its command input pins (task.*), without halui, and
# check that each one has the same effect halui's pin has.

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


inputs = [
    'machine.on', 'machine.off', 'estop.reset',
    'mode.teleop', 'mode.joint',
    'mist.on', 'mist.off', 'flood.on', 'flood.off',
    'program.run', 'program.pause', 'program.resume', 'program.step',
    'program.stop', 'abort',
    'program.optional-stop.on', 'program.optional-stop.off',
    'program.block-delete.on', 'program.block-delete.off',
    'home-all', 'joint.0.home', 'joint.0.unhome',
    'spindle.0.start', 'spindle.0.stop', 'spindle.0.forward',
    'spindle.0.reverse', 'spindle.0.increase', 'spindle.0.decrease',
    'spindle.0.brake-on', 'spindle.0.brake-off',
]

h = hal.component("python-ui")
for name in inputs:
    h.newpin(name, hal.Type.BOOL, hal.Dir.OUT)
h.ready()
for name in inputs:
    sig = 'sig-' + name.replace('.', '-')
    if os.system("halcmd net %s python-ui.%s task.%s" % (sig, name, name)):
        log("cannot connect task.%s" % name)
        sys.exit(1)

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


def pulse(name):
    h[name] = True
    time.sleep(0.05)
    h[name] = False
    time.sleep(0.05)


def wait_for(what, cond):
    start = time.time()
    while time.time() - start < 10.0:
        s.poll()
        if cond():
            log("%s: ok" % what)
            return
        time.sleep(0.05)
    log("timeout waiting for %s" % what)
    log("    task_state=%d task_mode=%d interp=%d state=%d" % (s.task_state, s.task_mode, s.interp_state, s.state))
    while True:
        err = e.poll()
        if not err:
            break
        log("    error channel: %s" % err[1])
    sys.exit(1)


def press(name, what, cond):
    pulse(name)
    wait_for("%s -> %s" % (name, what), cond)


press('estop.reset', 'estop reset', lambda: s.task_state == linuxcnc.STATE_ESTOP_RESET)
press('machine.on', 'machine on', lambda: s.task_state == linuxcnc.STATE_ON)

c.mode(linuxcnc.MODE_MANUAL)
c.wait_complete()

press('mist.on', 'mist on', lambda: s.mist)
press('mist.off', 'mist off', lambda: not s.mist)
press('flood.on', 'flood on', lambda: s.flood)
press('flood.off', 'flood off', lambda: not s.flood)

press('home-all', 'all homed', lambda: all(s.homed[:3]))
press('mode.joint', 'joint mode', lambda: s.motion_mode == linuxcnc.TRAJ_MODE_FREE)
# motion only unhomes in joint mode
press('joint.0.unhome', 'joint 0 unhomed', lambda: not s.homed[0])
press('joint.0.home', 'joint 0 homed', lambda: s.homed[0])
press('mode.teleop', 'teleop mode', lambda: s.motion_mode == linuxcnc.TRAJ_MODE_TELEOP)

press('program.optional-stop.on', 'optional stop on', lambda: s.optional_stop)
press('program.optional-stop.off', 'optional stop off', lambda: not s.optional_stop)
press('program.block-delete.on', 'block delete on', lambda: s.block_delete)
press('program.block-delete.off', 'block delete off', lambda: not s.block_delete)

press('spindle.0.forward', 'spindle forward', lambda: s.spindle[0]['direction'] == 1)
press('spindle.0.reverse', 'spindle reverse', lambda: s.spindle[0]['direction'] == -1)
press('spindle.0.stop', 'spindle stopped', lambda: s.spindle[0]['direction'] == 0)
press('spindle.0.start', 'spindle started', lambda: s.spindle[0]['direction'] == 1)
s.poll()
speed = abs(s.spindle[0]['speed'])
press('spindle.0.increase', 'spindle faster', lambda: abs(s.spindle[0]['speed']) > speed)
s.poll()
speed = abs(s.spindle[0]['speed'])
press('spindle.0.decrease', 'spindle slower', lambda: abs(s.spindle[0]['speed']) < speed)
press('spindle.0.stop', 'spindle stopped', lambda: s.spindle[0]['direction'] == 0)
press('spindle.0.brake-on', 'brake on', lambda: s.spindle[0]['brake'])
press('spindle.0.brake-off', 'brake off', lambda: not s.spindle[0]['brake'])

# a rejected internal command must not mark the UI's last command failed
c.mode(linuxcnc.MODE_MANUAL)
c.wait_complete()
pulse('program.step')       # can't step in manual mode
time.sleep(0.2)
s.poll()
if s.state != linuxcnc.RCS_DONE:
    log("rejected internal command changed the UI command status to %d" % s.state)
    sys.exit(1)
log("rejected internal command: UI status still done")

# nor clear the error of the UI's last command, when one of them succeeds
c.auto(linuxcnc.AUTO_STEP)  # can't step in manual mode either
c.wait_complete()
s.poll()
if s.state != linuxcnc.RCS_ERROR:
    log("refused UI command: status %d, expected error" % s.state)
    sys.exit(1)
pulse('mist.on')
wait_for('mist.on -> mist on', lambda: s.mist)
if s.state != linuxcnc.RCS_ERROR:
    log("internal command cleared the UI command's error: status %d" % s.state)
    sys.exit(1)
log("internal command: UI command's error kept")
pulse('mist.off')
wait_for('mist.off -> mist off', lambda: not s.mist)
while e.poll():
    pass

c.program_open(os.path.abspath("slow.ngc"))
c.wait_complete()
press('program.run', 'auto mode, program running',
      lambda: s.task_mode == linuxcnc.MODE_AUTO and
              s.interp_state in (linuxcnc.INTERP_READING, linuxcnc.INTERP_WAITING))
press('program.pause', 'paused', lambda: s.interp_state == linuxcnc.INTERP_PAUSED)
press('program.resume', 'running again',
      lambda: s.interp_state in (linuxcnc.INTERP_READING, linuxcnc.INTERP_WAITING) and s.current_vel > 0)
press('program.stop', 'stopped', lambda: s.interp_state == linuxcnc.INTERP_IDLE)
press('program.step', 'stepped (paused)', lambda: s.interp_state == linuxcnc.INTERP_PAUSED)
press('abort', 'aborted', lambda: s.interp_state == linuxcnc.INTERP_IDLE)

c.mode(linuxcnc.MODE_MANUAL)
c.wait_complete()

# Task reports a machine that is off but out of estop as ESTOP_RESET
press('machine.off', 'machine off', lambda: s.task_state == linuxcnc.STATE_ESTOP_RESET)

c.state(linuxcnc.STATE_ESTOP)
c.wait_complete()

log("all input pins work")
sys.exit(0)
