#!/usr/bin/env python3

import linuxcnc
import hal

import time
import sys
import os

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

h = hal.component("test-ui")
h.newpin("done-count", hal.Type.REAL, hal.Dir.IN)
h.ready()
os.system("halcmd source ./postgui.hal")

def wait_for(what, done):
    start = time.time()
    while True:
        s.poll()
        if done():
            return
        if time.time() - start > 10:
            sys.stderr.write("timeout waiting for %s\n" % what)
            sys.exit(1)
        time.sleep(0.05)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.wait_complete()
c.state(linuxcnc.STATE_ON)
c.wait_complete()
wait_for("machine on", lambda: s.task_state == linuxcnc.STATE_ON)

# one joint at a time: a home command while another joint homes is refused
for joint in range(3):
    c.home(joint)
    wait_for("joint %d homed" % joint, lambda: s.homed[joint]
             and not any(j["homing"] for j in s.joint[:3]))
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
