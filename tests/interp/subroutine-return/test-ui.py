#!/usr/bin/env python3

import linuxcnc
import linuxcnc_util
import hal

import time
import sys
import os


# this is how long we wait for linuxcnc to do our bidding
timeout = 1.0


#
# set up pins
# shell out to halcmd to net our pins to where they need to go
#

h = hal.component("python-ui")
h.ready()


#
# connect to LinuxCNC
#

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(0)
c.home(1)
c.home(2)
l.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])

c.mode(linuxcnc.MODE_AUTO)

c.program_open("test.ngc")
c.auto(linuxcnc.AUTO_RUN, 1)

# wait for the interpreter to start running the test.ngc program
start_time = time.time()
while (time.time() - start_time) < 2.0:
    s.poll()
    if s.interp_state != linuxcnc.INTERP_IDLE:
        break
    time.sleep(0.001)

if s.interp_state == linuxcnc.INTERP_IDLE:
    print("failed to start interpreter, interp_state is {}".format(e.s.interp_state))
    sys.exit(1)

# tee hee!
os.rename('test.ngc', 'moved-test.ngc')
#os.rename('subs/sub.ngc', 'subs/moved-sub.ngc')

l.wait_for_interp_state(linuxcnc.INTERP_IDLE)

# ok fine, have it back
os.rename('moved-test.ngc', 'test.ngc')
#os.rename('subs/moved-sub.ngc', 'subs/sub.ngc')


print("done! it all worked")

# if we get here it all worked!
sys.exit(0)

