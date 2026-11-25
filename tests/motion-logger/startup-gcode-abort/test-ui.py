#!/usr/bin/env python3

import linuxcnc
import sys
import time
import hal

#
# connect to LinuxCNC
#

comp = hal.component('hal-watcher')

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


#
# Immediately abort once linuxcnc is ready!  Github Issue #49
#


# Wait for (any) HAL pin to show up when LinuxCNC is initialized
waitlimit = 300
while 0 < waitlimit:
    try:
        waiting = 'TRUE' != hal.get_value('motion.in-position')
    except RuntimeError:
        break
    time.sleep(0.1)
    waitlimit -= 1

# Abort in the dwell of the startup code, after its first move went out.
waitlimit = 600
while 0 < waitlimit:
    s.poll()
    if s.exec_state == linuxcnc.EXEC_WAITING_FOR_DELAY:
        break
    time.sleep(0.1)
    waitlimit -= 1

print("UI abort")
sys.stdout.flush()

c.abort()
c.wait_complete()

print("UI done with abort")
sys.stdout.flush()

