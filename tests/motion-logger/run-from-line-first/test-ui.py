#!/usr/bin/env python3
# Run-from-line with a remapped code whose subroutine waits (M66): issue #246.
import linuxcnc, sys, time
c = linuxcnc.command(); s = linuxcnc.stat(); e = linuxcnc.error_channel()
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_AUTO)
c.program_open('test.ngc')
c.auto(linuxcnc.AUTO_RUN, 1)
c.wait_complete()
start = time.time()
while time.time() - start < 10:
    s.poll()
    if s.interp_state == linuxcnc.INTERP_IDLE and s.exec_state == linuxcnc.EXEC_DONE:
        break
    time.sleep(0.1)
sys.exit(0)
