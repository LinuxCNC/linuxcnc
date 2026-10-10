#!/usr/bin/env python3

import linuxcnc
import linuxcnc_util
import hal

import time
import sys
import os

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()
l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)

h = hal.component("test-ui")
h.newpin("done-count", hal.Type.REAL, hal.Dir.IN)
h.ready()
os.system("halcmd source ./postgui.hal")

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(0)
c.home(1)
c.home(2)
l.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])
c.mode(linuxcnc.MODE_AUTO)
c.wait_complete()

c.program_open('test.ngc')
c.auto(linuxcnc.AUTO_RUN, 0)

start = time.time()
while True:
    s.poll()
    if s.interp_state != linuxcnc.INTERP_IDLE:
        break
    if time.time() - start > 10:
        sys.stderr.write("timeout waiting for the program to start\n")
        sys.exit(1)
    time.sleep(0.001)

errors = []
while True:
    err = e.poll()
    if err:
        errors.append(err[1])
    s.poll()
    if s.interp_state == linuxcnc.INTERP_IDLE:
        break
    if time.time() - start > 60:
        sys.stderr.write("timeout waiting for the program to finish\n")
        sys.exit(1)
    time.sleep(0.05)

err = e.poll()
while err:
    errors.append(err[1])
    err = e.poll()

for msg in errors:
    sys.stderr.write("error: %s\n" % msg)
sys.stderr.write("probes done: %d\n" % h["done-count"])
if errors or h["done-count"] != 100:
    sys.exit(1)
sys.exit(0)
