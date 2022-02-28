#!/usr/bin/env python3

import linuxcnc
import linuxcnc_util
import hal

import time
import sys
import os

# Time increment and timeout, seconds
TIME_INCR = 0.1
TIMEOUT = 10.0
MAX_QWAIT_TRIES = 4

#
# connect to LinuxCNC
#

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()
l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)


#
# Create and connect test feedback comp
#
h = hal.component("test-ui")
h.newpin("Xpos", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("Ypos", hal.HAL_FLOAT, hal.HAL_IN)
h.ready()
os.system("halcmd source ./postgui.hal")

#
# Come out of E-stop, turn the machine on, home
#

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(0)
c.home(1)
c.home(2)
l.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])

#
# Print status function
#

def print_stats(x = None, y = None):
    s.poll()
    if x is None:
        sys.stderr.write(
            "queue=%d; active_queue=%d; queue_full=%s;"
            " x=%.3f; y=%.3f\n" %
            (s.queue, s.active_queue, s.queue_full, h['Xpos'], h['Ypos']))
    else:
        sys.stderr.write(
            "queue=%d; active_queue=%d; queue_full=%s;"
            " x=%.3f(%.3f); y=%.3f(%.3f)\n" %
            (s.queue, s.active_queue, s.queue_full,
             h['Xpos'], x-h['Xpos'], h['Ypos'], y-h['Ypos']))

#
# Set up tool and start position
#
c.mode(linuxcnc.MODE_MDI)
c.mdi('T1M6') # Load tool 1
c.mdi('G0 X2 Y2') # Move near to start
c.wait_complete()
start_time = time.time()
while (abs(h['Xpos'] - 2) > .001) or  (abs(h['Ypos'] - 2) > .001):
    if (time.time() - start_time) > TIMEOUT:
        sys.stderr.write("Failed to reach start position in time\n")
        os.system("halcmd show")
        sys.exit(1)
    time.sleep(TIME_INCR)
sys.stderr.write("Starting at X=%.3f Y=%.3f\n" % (h["Xpos"], h["Ypos"]))


#
# Run the generated program, wait for motion to start and the queue to
# fill up
#
c.mode(linuxcnc.MODE_AUTO)
# c.program_open('test.ngc')
c.program_open('3D_Chips.ngc')
c.auto(linuxcnc.AUTO_RUN, 0)
start_time = time.time()
s.poll()
MAX_QWAIT_TRIES=4
ct=0
while not (s.queue > 1000):
    delta_t = time.time() - start_time
    if delta_t > TIMEOUT:
        sys.stderr.write("ct=%d s.queue=%d delta_t=%f TIMEOUT=%f\n"%(
                          ct,s.queue,delta_t,TIMEOUT))
        start_time = time.time()
        ct += 1
    if ct > MAX_QWAIT_TRIES:
        sys.stderr.write("Failed to load segments from program\n")
        sys.exit(1)

    print_stats()
    time.sleep(TIME_INCR)
sys.stderr.write("Program partially loaded\n")

#
# Now stop the program and wait for any additional motion
#
# Program X stepover is 2.5mm, or 0.1in; error out if the X stepover
# exceeds 0.15 (fudge added to eliminate a legitimate stepover during
# lag between cycles)

pre_stop_x, pre_stop_y = h['Xpos'], h['Ypos']
c.abort()

start_time = time.time()
last_x = pre_stop_x
fail = False
while (time.time() - start_time) < TIMEOUT and h['Xpos'] != last_x:
    print_stats(pre_stop_x, pre_stop_y)
    err = abs(pre_stop_x - h['Xpos'])
    if err > 0.15:
        sys.stderr.write(
            'ERROR:  X axis motion stopped %.3f" past stop position\n' % err)
        fail = True
    last_x = h['Xpos']
    time.sleep(TIME_INCR)

if fail:
    sys.exit(1)

sys.stderr.write('Success\n')
sys.exit(0)
