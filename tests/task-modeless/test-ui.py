#!/usr/bin/env python3
# The task mode cannot be set: it reports what task runs, and task accepts
# jogs, MDI and program runs by what it is doing. Nothing here calls
# c.mode() except to check what it does: it sets this process's view, which
# stat.task_mode reports while task is idle. halui's mode pins show task's
# own mode.

import os
import sys
import time

import hal
import linuxcnc

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()
h = hal.component("test-ui")
h.ready()

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


def idle():
    return (s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0
            and hal.get_value('halui.mode.is-manual'))


def x():
    return s.actual_position[0]


MODE = {linuxcnc.MODE_MANUAL: 'MANUAL', linuxcnc.MODE_MDI: 'MDI', linuxcnc.MODE_AUTO: 'AUTO'}
TRAJ = {linuxcnc.TRAJ_MODE_FREE: 'FREE', linuxcnc.TRAJ_MODE_TELEOP: 'TELEOP',
        linuxcnc.TRAJ_MODE_COORD: 'COORD'}


def modes():
    return '%s/%s' % (MODE.get(s.task_mode), TRAJ.get(s.motion_mode))


wait(lambda: s.task_mode in MODE and s.motion_mode in TRAJ, 10.0)
check('starts idle', s.task_mode == linuxcnc.MODE_MANUAL and s.motion_mode == linuxcnc.TRAJ_MODE_FREE, modes())

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete()

# --- before homing: MDI is refused, joint jogs work
errors()
c.mdi('G0 X1')
c.wait_complete()
time.sleep(0.1)
msgs = errors()
check('MDI refused before homing', any('not homed' in m for m in msgs), msgs)
check('still idle in joint mode', wait(lambda: idle() and s.motion_mode == linuxcnc.TRAJ_MODE_FREE), modes())

c.jog(linuxcnc.JOG_INCREMENT, True, 1, 10, 0.5)
check('joint jog without a mode change', wait(lambda: abs(s.joint_actual_position[1] - 0.5) < 1e-6), s.joint_actual_position[1])

c.home(-1)
c.wait_complete()
check('homed', wait(lambda: all(s.homed[:3])), s.homed[:3])
c.teleop_enable(1)
c.wait_complete()
check('world mode', wait(lambda: s.motion_mode == linuxcnc.TRAJ_MODE_TELEOP), modes())

# --- no program open: run and step are refused
errors()
c.auto(linuxcnc.AUTO_STEP)
c.wait_complete()
time.sleep(0.1)
msgs = errors()
check('step refused with no program', any('no program is open' in m for m in msgs), msgs)
check('still idle after the refused step', wait(idle), modes())

# --- MDI from idle, no mode change
c.mdi('G0 X1')
check('MDI shows as MDI mode', wait(lambda: s.task_mode == linuxcnc.MODE_MDI, 1.0), modes())
check('MDI runs in coordinated mode', wait(lambda: s.motion_mode == linuxcnc.TRAJ_MODE_COORD, 1.0), modes())
check('MDI moved X', wait(lambda: abs(x() - 1) < 1e-6), x())
check('idle again after MDI', wait(idle), modes())
check('back in world mode', wait(lambda: s.motion_mode == linuxcnc.TRAJ_MODE_TELEOP), modes())
check('halui sees manual', wait(lambda: hal.get_value('halui.mode.is-manual')))
check('no errors from MDI', not errors())

# --- DONE means ready: right after wait_complete() motion is back in world
# mode and a world jog is accepted
for i in range(5):
    c.mdi('G0 X%d' % (i % 2))
    r = c.wait_complete()
    s.poll()
    if r != linuxcnc.RCS_DONE or s.motion_mode != linuxcnc.TRAJ_MODE_TELEOP:
        break
check('world mode as soon as MDI is DONE', r == linuxcnc.RCS_DONE and s.motion_mode == linuxcnc.TRAJ_MODE_TELEOP, (r, modes()))
c.jog(linuxcnc.JOG_INCREMENT, False, 1, 10, 0.1)
c.wait_complete()
check('a jog right after MDI is accepted', wait(lambda: abs(s.actual_position[1] - 0.1) < 1e-6, 2) and not errors(), s.actual_position[1])
c.jog(linuxcnc.JOG_INCREMENT, False, 1, 10, -0.1)
wait(lambda: abs(s.actual_position[1]) < 1e-6)
c.mdi('G0 X1')
wait(idle)

# --- wheel jog while idle after MDI
hal.set_p('axis.x.jog-enable', '1')
hal.set_p('axis.x.jog-counts', '100')
check('wheel jog after MDI', wait(lambda: abs(x() - 2) < 1e-6), x())

# --- a world jog right after MDI, with no mode change
c.jog(linuxcnc.JOG_INCREMENT, False, 0, 10, 0.5)
check('world jog after MDI', wait(lambda: abs(x() - 2.5) < 1e-6), x())

# --- mode() sets this process's view only
c.mode(linuxcnc.MODE_AUTO)
c.wait_complete()
time.sleep(0.1)
s.poll()
check('mode(AUTO) shows as the view', s.task_mode == linuxcnc.MODE_AUTO, modes())
check('mode(AUTO) leaves task idle', hal.get_value('halui.mode.is-manual') and s.motion_mode == linuxcnc.TRAJ_MODE_TELEOP, modes())
c.mode(linuxcnc.MODE_MDI)
c.wait_complete()
time.sleep(0.1)
s.poll()
check('mode(MDI) shows as the view', s.task_mode == linuxcnc.MODE_MDI, modes())
check('mode(MDI) leaves task idle', hal.get_value('halui.mode.is-manual') and s.motion_mode == linuxcnc.TRAJ_MODE_TELEOP, modes())
c.jog(linuxcnc.JOG_INCREMENT, False, 0, 10, 0.5)
check('world jog with the MDI view', wait(lambda: abs(x() - 3) < 1e-6), x())
c.mode(linuxcnc.MODE_MANUAL)
s.poll()
check('mode(MANUAL) view', s.task_mode == linuxcnc.MODE_MANUAL, modes())
check('no errors from mode()', not errors())

# --- queued MDI; jogs are refused while it runs
c.mdi('G4 P1')
c.mdi('G0 X3.5')
check('MDI queue runs', wait(lambda: s.task_mode == linuxcnc.MODE_MDI, 1.0), modes())
check('halui sees MDI', wait(lambda: hal.get_value('halui.mode.is-mdi'), 1.0))
y = s.actual_position[1]
c.jog(linuxcnc.JOG_INCREMENT, False, 1, 10, 0.5)
c.wait_complete()
time.sleep(0.1)
msgs = errors()
check('jog refused while MDI runs', any('while MDI is running' in m for m in msgs), msgs)
c.mode(linuxcnc.MODE_MANUAL)
c.wait_complete()
check('mode(MANUAL) does not stop MDI: queued MDI finished', wait(lambda: abs(x() - 3.5) < 1e-6, 4.0), x())
check('idle after the MDI queue', wait(idle), modes())
check('Y did not move', abs(s.actual_position[1] - y) < 1e-6, (s.actual_position[1], y))

# --- a jog in progress holds off MDI
c.jog(linuxcnc.JOG_CONTINUOUS, False, 1, 1)
wait(lambda: s.current_vel > 0.5, 1.0)
c.mdi('G0 X0')
c.wait_complete()
time.sleep(0.1)
msgs = errors()
check('MDI refused while jogging', any('while jogging' in m for m in msgs), msgs)
c.jog(linuxcnc.JOG_STOP, False, 1)
check('jog stopped, idle', wait(lambda: idle() and s.current_vel == 0), modes())

# --- a program run from idle, no mode change
c.program_open(os.path.abspath('program.ngc'))
c.wait_complete()
c.auto(linuxcnc.AUTO_RUN, 0)
check('program shows as AUTO mode', wait(lambda: s.task_mode == linuxcnc.MODE_AUTO, 1.0), modes())
check('halui sees auto', wait(lambda: hal.get_value('halui.mode.is-auto'), 1.0))
check('program reached the dwell', wait(lambda: abs(x() - 2) < 1e-6), x())
c.mode(linuxcnc.MODE_MANUAL)
c.mdi('G0 X0')
c.jog(linuxcnc.JOG_INCREMENT, False, 1, 10, 0.5)
c.wait_complete()
time.sleep(0.1)
msgs = errors()
s.poll()
check('mode(MANUAL) leaves the program running', s.task_mode == linuxcnc.MODE_AUTO and hal.get_value('halui.mode.is-auto'), modes())
check('MDI refused while the program runs', any('EMC_TASK_PLAN_EXECUTE' in m for m in msgs), msgs)
check('jog refused while the program runs', any('EMC_JOG_INCR' in m for m in msgs), msgs)
check('program finished', wait(lambda: abs(x() - 4) < 1e-6, 5.0), x())
check('idle after the program', wait(idle), modes())
check('world mode after the program', wait(lambda: s.motion_mode == linuxcnc.TRAJ_MODE_TELEOP), modes())

# --- abort a paused program
c.auto(linuxcnc.AUTO_RUN, 0)
wait(lambda: abs(x() - 2) < 1e-6)
c.auto(linuxcnc.AUTO_PAUSE)
check('paused program is AUTO', wait(lambda: s.paused and s.task_mode == linuxcnc.MODE_AUTO), modes())
c.abort()
c.wait_complete()
check('abort ends the run', wait(idle), modes())
errors()

# --- joint mode is kept across a run
c.teleop_enable(0)
c.wait_complete()
wait(lambda: s.motion_mode == linuxcnc.TRAJ_MODE_FREE)
c.mdi('G0 X1')
check('MDI from joint mode', wait(lambda: abs(x() - 1) < 1e-6), x())
check('back in joint mode', wait(lambda: idle() and s.motion_mode == linuxcnc.TRAJ_MODE_FREE), modes())
c.teleop_enable(1)
c.wait_complete()

# --- halui MDI command
hal.set_p('halui.mdi-command-00', '1')
check('halui MDI runs', wait(lambda: abs(x()) < 1e-6), x())
check('idle after halui MDI', wait(idle), modes())
check('halui-mdi-is-running cleared', wait(lambda: not hal.get_value('halui.halui-mdi-is-running')))
hal.set_p('halui.mdi-command-00', '0')

# --- the view comes back once a run ends
c.mode(linuxcnc.MODE_MDI)
c.mdi('G0 X0.5')
check('MDI from the MDI view', wait(lambda: abs(x() - 0.5) < 1e-6), x())
check('idle again, MDI view kept', wait(lambda: hal.get_value('halui.mode.is-manual') and s.interp_state == linuxcnc.INTERP_IDLE)
      and s.task_mode == linuxcnc.MODE_MDI, modes())
c.mode(linuxcnc.MODE_MANUAL)

# --- step from idle starts the program paused
c.auto(linuxcnc.AUTO_STEP)
check('step starts the program', wait(lambda: s.task_mode == linuxcnc.MODE_AUTO and s.paused), modes())
c.abort()
c.wait_complete()
check('abort after step', wait(idle), modes())

msgs = errors()
check('no unexpected errors', not msgs, msgs)

c.state(linuxcnc.STATE_OFF)
c.wait_complete()
print('%d failures' % fails, flush=True)
sys.exit(1 if fails else 0)
