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
h.newpin("finished", hal.Type.REAL, hal.Dir.IN)
h.ready()
os.system("halcmd source ./postgui.hal")

failures = []

def settle(what, done):
    """wait_complete() must not return before the command took effect;
    note it if it did, then wait for it so the rest still runs"""
    s.poll()
    if done():
        return
    failures.append("wait_complete() returned before " + what)
    start = time.time()
    while not done():
        if time.time() - start > 60:
            sys.stderr.write("timeout waiting for %s\n" % what)
            sys.exit(1)
        time.sleep(0.05)
        s.poll()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.wait_complete(60)
# slow-controller: task is up, from now on the controller takes most of
# the servo period
if hal.component_exists("slowkins"):
    hal.set_p("slowkins.wait-ns", "700000")
c.state(linuxcnc.STATE_ON)
c.wait_complete(60)
settle("the machine was on", lambda: s.task_state == linuxcnc.STATE_ON)

# one joint at a time: a home command while another joint homes is refused
for joint in range(3):
    c.home(joint)
    start = time.time()
    while True:
        s.poll()
        if s.homed[joint] and not any(j["homing"] for j in s.joint[:3]):
            break
        if time.time() - start > 60:
            sys.stderr.write("timeout homing joint %d\n" % joint)
            sys.exit(1)
        time.sleep(0.05)

c.mode(linuxcnc.MODE_AUTO)
c.wait_complete(60)
settle("auto mode was on", lambda: s.task_mode == linuxcnc.MODE_AUTO
       and s.motion_mode == linuxcnc.TRAJ_MODE_COORD)

c.program_open('test.ngc')
c.auto(linuxcnc.AUTO_RUN, 0)

start = time.time()
while True:
    s.poll()
    if s.interp_state != linuxcnc.INTERP_IDLE:
        break
    if time.time() - start > 60:
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
    if time.time() - start > 600:
        sys.stderr.write("timeout waiting for the program to finish\n")
        sys.exit(1)
    time.sleep(0.05)

err = e.poll()
while err:
    errors.append(err[1])
    err = e.poll()

for msg in failures + errors:
    sys.stderr.write("error: %s\n" % msg)
if h["finished"] != 1:
    sys.stderr.write("the program did not finish\n")
sys.exit(1 if failures or errors or h["finished"] != 1 else 0)
