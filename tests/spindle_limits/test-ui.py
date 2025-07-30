#!/usr/bin/env python3

import linuxcnc
import hal
import time
import sys

def wait_for_linuxcnc_startup(status, timeout=10.0):

    """Poll the Status buffer waiting for it to look initialized,
    rather than just allocated (all-zero).  Returns on success, throws
    RuntimeError on failure."""

    start_time = time.time()
    while time.time() - start_time < timeout:
        status.poll()
        if (status.angular_units == 0.0) \
            or (status.axis_mask == 0) \
            or (status.cycle_time == 0.0) \
            or (status.exec_state != linuxcnc.EXEC_DONE) \
            or (status.interp_state != linuxcnc.INTERP_IDLE) \
            or (status.inpos == False) \
            or (status.linear_units == 0.0) \
            or (status.max_acceleration == 0.0) \
            or (status.max_velocity == 0.0) \
            or (status.program_units == 0.0) \
            or (status.rapidrate == 0.0) \
            or (status.state != linuxcnc.RCS_DONE) \
            or (status.task_state != linuxcnc.STATE_ESTOP):
            time.sleep(0.1)
        else:
            # looks good
            return

    # timeout, throw an exception
    raise RuntimeError


def wait_for_mdi_queue(queue_len, timeout=10):
    start_time = time.time()
    while (time.time() - start_time) < timeout:
        s.poll()
        if s.queued_mdi_commands == queue_len:
            return
        time.sleep(0.1)
    print("queued_mdi_commands at %d after %.3f seconds" % (s.queued_mdi_commands, timeout))
    sys.exit(1)


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()
comp = hal.component('hal-watcher')

# Wait for LinuxCNC to initialize itself so the Status buffer stabilizes.
wait_for_linuxcnc_startup(s)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete()


# Check G-code / MDI spindle commands

c.mode(linuxcnc.MODE_MDI)

c.mdi("M3 S10 $0")
c.mdi("M3 S10 $1")
c.mdi("M3 S10 $2")
c.wait_complete()

s.poll()

assert hal.get_value('spindle.0.speed-out') == 100
assert s.spindle[0]['speed'] == 100
assert hal.get_value('spindle.1.speed-out') == 200
assert s.spindle[1]['speed'] == 200
assert hal.get_value('spindle.2.speed-out') == 300
assert s.spindle[2]['speed'] == 300

c.mdi("M3 S10000 $0")
c.mdi("M3 S10000 $1")
c.mdi("M3 S10000 $2")
c.wait_complete()

s.poll()

assert hal.get_value('spindle.0.speed-out') == 1000
assert s.spindle[0]['speed'] == 1000
assert hal.get_value('spindle.1.speed-out') == 2000
assert s.spindle[1]['speed'] == 2000
assert hal.get_value('spindle.2.speed-out') == 3000
assert s.spindle[2]['speed'] == 3000

c.mdi("M4 S10 $0")
c.mdi("M4 S10 $1")
c.mdi("M4 S10 $2")
c.wait_complete()

s.poll()

assert hal.get_value('spindle.0.speed-out') == -500
assert s.spindle[0]['speed'] == -500
assert hal.get_value('spindle.1.speed-out') == -200
assert s.spindle[1]['speed'] == -200
assert hal.get_value('spindle.2.speed-out') == -300
assert s.spindle[2]['speed'] == -300

c.mdi("M4 S10000 $0")
c.mdi("M4 S10000 $1")
c.mdi("M4 S10000 $2")
c.wait_complete()

s.poll()

assert hal.get_value('spindle.0.speed-out') == -1500
assert s.spindle[0]['speed'] == -1500
assert hal.get_value('spindle.1.speed-out') == -2500
assert s.spindle[1]['speed'] == -2500
assert hal.get_value('spindle.2.speed-out') == -3000
assert s.spindle[2]['speed'] == -3000

# Check Python interface commands

c.spindle(linuxcnc.SPINDLE_FORWARD, 10, 0, 0)
c.spindle(linuxcnc.SPINDLE_FORWARD, 10, 1, 0)
c.spindle(linuxcnc.SPINDLE_FORWARD, 10, 2, 0)
c.wait_complete()

s.poll()

assert hal.get_value('spindle.0.speed-out') == 100
assert s.spindle[0]['speed'] == 100
assert hal.get_value('spindle.1.speed-out') == 200
assert s.spindle[1]['speed'] == 200
assert hal.get_value('spindle.2.speed-out') == 300
assert s.spindle[2]['speed'] == 300

c.spindle(linuxcnc.SPINDLE_FORWARD, 10000, 0, 0)
c.spindle(linuxcnc.SPINDLE_FORWARD, 10000, 1, 0)
c.spindle(linuxcnc.SPINDLE_FORWARD, 10000, 2, 0)
c.wait_complete()

s.poll()

assert hal.get_value('spindle.0.speed-out') == 1000
assert s.spindle[0]['speed'] == 1000
assert hal.get_value('spindle.1.speed-out') == 2000
assert s.spindle[1]['speed'] == 2000
assert hal.get_value('spindle.2.speed-out') == 3000
assert s.spindle[2]['speed'] == 3000

c.spindle(linuxcnc.SPINDLE_REVERSE, 10, 0, 0)
c.spindle(linuxcnc.SPINDLE_REVERSE, 10, 1, 0)
c.spindle(linuxcnc.SPINDLE_REVERSE, 10, 2, 0)
c.wait_complete()

s.poll()

assert hal.get_value('spindle.0.speed-out') == -500
assert s.spindle[0]['speed'] == -500
assert hal.get_value('spindle.1.speed-out') == -200
assert s.spindle[1]['speed'] == -200
assert hal.get_value('spindle.2.speed-out') == -300
assert s.spindle[2]['speed'] == -300

c.spindle(linuxcnc.SPINDLE_REVERSE, 10000, 0, 0)
c.spindle(linuxcnc.SPINDLE_REVERSE, 10000, 1, 0)
c.spindle(linuxcnc.SPINDLE_REVERSE, 10000, 2, 0)
c.wait_complete()

s.poll()

assert hal.get_value('spindle.0.speed-out') == -1500
assert s.spindle[0]['speed'] == -1500
assert hal.get_value('spindle.1.speed-out') == -2500
assert s.spindle[1]['speed'] == -2500
assert hal.get_value('spindle.2.speed-out') == -3000
assert s.spindle[2]['speed'] == -3000

sys.exit(0)

