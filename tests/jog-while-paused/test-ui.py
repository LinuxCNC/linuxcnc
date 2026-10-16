#!/usr/bin/env python3
# Jogs while a program is paused: axes move off the paused position, and
# resume brings them back (the other axes first, then Z) with the spindle
# and coolant as they were, before the program goes on.

import math
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
        time.sleep(0.005)
    s.poll()
    return fn()


def errors():
    msgs = []
    while True:
        m = e.poll()
        if not m:
            return msgs
        msgs.append(m[1])


def pos():
    return s.actual_position[:3]


def near(a, b, tol=1e-6):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


def idle():
    return s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0 and hal.get_value('halui.mode.is-manual')


def run_and_pause(prog, at_x):
    c.program_open(os.path.abspath(prog))
    c.wait_complete()
    c.auto(linuxcnc.AUTO_RUN, 0)
    if not wait(lambda: s.actual_position[0] >= at_x, 10):
        check('program reached X%g' % at_x, False, pos())
    c.auto(linuxcnc.AUTO_PAUSE)
    c.wait_complete()
    return wait(lambda: s.pause_jog_ok, 2)


wait(lambda: s.task_state != 0, 10)
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()
wait(lambda: all(s.homed[:3]), 10)
c.teleop_enable(1)
c.wait_complete()
c.mdi('G0 X0 Y0 Z0')
wait(idle)
errors()

# --- pause in the middle of a G1, jog three axes three ways
check('paused and ready to jog', run_and_pause('program.ngc', 5.0))
p = pos()
print('paused at', p, flush=True)
check('spindle and flood on at the pause', s.spindle[0]['direction'] == 1 and s.flood, (s.spindle[0]['direction'], s.flood))

c.jog(linuxcnc.JOG_INCREMENT, False, 2, 20, 5.0)          # Z +5
check('incremental Z jog', wait(lambda: near(pos(), (p[0], p[1], p[2] + 5))), pos())
c.jog(linuxcnc.JOG_CONTINUOUS, False, 0, 10)              # X +, 10 mm/s
time.sleep(0.3)
c.jog(linuxcnc.JOG_STOP, False, 0)
check('continuous X jog stopped', wait(lambda: s.current_vel == 0 and not s.motion.jogging_active
                                       if hasattr(s, 'motion') else not hal.get_value('motion.jog-is-active')), pos())
dx = pos()[0] - p[0]
check('continuous X jog moved', 1.0 < dx < 5.0, dx)
hal.set_p('axis.y.jog-enable', '1')
hal.set_p('axis.y.jog-counts', '200')                      # Y +2
check('wheel Y jog', wait(lambda: near(pos(), (p[0] + dx, p[1] + 2, p[2] + 5))), pos())
s.poll()
check('pause offset reported', near(s.pause_offset[:3], (dx, 2, 5)), s.pause_offset[:3])
check('still paused', s.paused and s.interp_state == linuxcnc.INTERP_PAUSED)
check('no errors from the jogs', not errors())

# --- joint jogs and reverse run are refused while jogged away
c.jog(linuxcnc.JOG_INCREMENT, True, 0, 10, 1)
c.wait_complete()
time.sleep(0.1)
msgs = errors()
check('joint jog refused while paused', any('jog a joint' in m for m in msgs), msgs)
c.auto(linuxcnc.AUTO_REVERSE)
c.wait_complete()
time.sleep(0.1)
msgs = errors()
check('reverse refused while jogged away', any('reverse' in m for m in msgs), msgs)

# --- the operator stops spindle and coolant; resume puts them back and
# waits for the spindle to be at speed before moving
c.spindle(linuxcnc.SPINDLE_OFF)
c.flood(linuxcnc.FLOOD_OFF)
c.wait_complete()
check('spindle and flood off', wait(lambda: s.spindle[0]['direction'] == 0 and not s.flood))
hal.set_p('spindle.0.at-speed', '0')
off = pos()
c.auto(linuxcnc.AUTO_RESUME)
c.wait_complete()
check('resume restores the spindle', wait(lambda: s.spindle[0]['direction'] == 1 and s.spindle[0]['speed'] == 1000, 1), s.spindle[0]['speed'])
check('resume restores the flood', wait(lambda: s.flood, 1))
check('returning', wait(lambda: s.pause_returning, 1))
time.sleep(0.5)
s.poll()
check('no move until the spindle is at speed', near(pos(), off), (pos(), off))

# --- at speed: XY back in a straight line with Z up, then Z down, then on.
# The commanded offsets (pause_offset) show the path; actual_position lags
# them by the joint interpolation.
offsets = []
hal.set_p('spindle.0.at-speed', '1')
end = time.time() + 5
while time.time() < end:
    s.poll()
    offsets.append(s.pause_offset[:3])
    if not s.pause_returning:
        break
    time.sleep(0.002)
s.poll()
check('back at the paused position: offsets exactly zero', offsets and offsets[-1] == (0.0, 0.0, 0.0), offsets[-1:])
z_early = [o for o in offsets if abs(o[2] - 5) > 1e-6 and (abs(o[0]) > 0 or abs(o[1]) > 0)]
check('Z stays up until XY are back', not z_early, z_early[:3])
off_line = [o for o in offsets if abs(o[0] * 2 - o[1] * dx) > 1e-6]
check('XY return is a straight line', not off_line, off_line[:3])
xy_on_way = [o for o in offsets if 0.1 < o[0] < dx - 0.1]
check('XY move back, no jump', len(xy_on_way) > 10, len(xy_on_way))
z_on_way = [o for o in offsets if 0.1 < o[2] < 4.9]
check('Z moves back, no jump', len(z_on_way) > 10, len(z_on_way))
check('program finished at the end point', wait(lambda: idle(), 20), pos())
check('program end position', near(pos()[:2], (0, 0), 1e-6), pos())
check('no errors from the resume', not errors())

# --- abort while jogged away: no jump, stay there
check('paused again', run_and_pause('program.ngc', 5.0))
p = pos()
c.jog(linuxcnc.JOG_INCREMENT, False, 2, 20, 3.0)
check('Z jog before abort', wait(lambda: near(pos(), (p[0], p[1], p[2] + 3))), pos())
c.abort()
c.wait_complete()
check('idle after abort', wait(idle))
time.sleep(0.2)
s.poll()
check('abort keeps the jogged position', near(pos(), (p[0], p[1], p[2] + 3)), pos())
check('pause offset gone after abort', near(s.pause_offset[:3], (0, 0, 0), 0), s.pause_offset[:3])
c.mdi('G0 X1')
check('MDI after the abort', wait(lambda: idle() and near(pos(), (1, p[1], p[2] + 3))), pos())
errors()

# --- soft limits hold during a jog while paused (X max is 30)
check('paused for the limit test', run_and_pause('program.ngc', 5.0))
c.jog(linuxcnc.JOG_CONTINUOUS, False, 0, 40)
time.sleep(1.5)
s.poll()
check('X stops at its soft limit', s.actual_position[0] <= 30 + 1e-9 and s.actual_position[0] > 29, s.actual_position[0])
c.jog(linuxcnc.JOG_STOP, False, 0)
c.abort()
c.wait_complete()
wait(idle)
c.mdi('G0 X0 Y0 Z0')
wait(idle)
errors()

# --- step with an offset returns first, then steps
check('paused for the step test', run_and_pause('program.ngc', 5.0))
p = pos()
c.jog(linuxcnc.JOG_INCREMENT, False, 1, 20, 4.0)
wait(lambda: near(pos(), (p[0], p[1] + 4, p[2])))
c.auto(linuxcnc.AUTO_STEP)
c.wait_complete()
check('step brings the offset back', wait(lambda: near(s.pause_offset[:3], (0, 0, 0), 0), 5), s.pause_offset[:3])
check('step moved on along the program', wait(lambda: s.actual_position[0] > p[0] + 1e-3 or s.actual_position[1] > p[1] + 1e-3, 5), pos())
c.abort()
c.wait_complete()
wait(idle)
c.mdi('G0 X0 Y0 Z0')
wait(idle)
errors()

# --- an M0 stop is a pause too
c.program_open(os.path.abspath('m0.ngc'))
c.wait_complete()
c.auto(linuxcnc.AUTO_RUN, 0)
check('stopped at M0', wait(lambda: s.paused and s.pause_jog_ok, 10), pos())
p = pos()
c.jog(linuxcnc.JOG_INCREMENT, False, 2, 4, 2.0)          # Z +2 at 4 mm/s
wait(lambda: near(pos(), (p[0], p[1], p[2] + 2)))
c.auto(linuxcnc.AUTO_RESUME)
t0 = time.time()
wait(lambda: s.pause_offset[2] == 0, 5)
dt = time.time() - t0
check('the return goes at the speed of the jog', 0.45 < dt < 1.0, round(dt, 2))
check('M0: back and finished', wait(lambda: idle() and near(pos(), (0, p[1], p[2])), 10), pos())

msgs = errors()
check('no unexpected errors', not msgs, msgs)

c.state(linuxcnc.STATE_OFF)
c.wait_complete()
print('%d failures' % fails, flush=True)
os._exit(1 if fails else 0)
