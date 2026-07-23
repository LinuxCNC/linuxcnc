#!/usr/bin/env python3

"""
Regression test for the flush_segments() ordering fix in HOME_CYCLE()/
HOME_CYCLE_JOINT()/UNHOME_AXES()/UNHOME_JOINT()/HOME_CYCLE_IF_UNHOMED()
(emccanon.cc).

STRAIGHT_FEED/STRAIGHT_TRAVERSE buffer points into chained_points for
arc-blend lookahead and only reach interp_list on a flush (see
see_segment()/flush_segments()). Without an explicit flush_segments() call
at the start of the home/unhome canon functions, a queued move immediately
before a G28.2/G28.3 could silently get reordered to run *after* the home
instead of before it.

test.ngc queues "G1 X2" (a multi-cycle move, slow enough to poll mid-flight)
immediately followed by "G28.2 P0". This script polls position and homed
state throughout the run and asserts joint 0 never reports homed=1 before
its position has actually reached the X2 target -- if the move were
silently deferred to after the home (the bug), homed would flip true while
position was still near its starting point.
"""

import linuxcnc
import hal

import sys
import time

h = hal.component("python-ui")
h.ready()

c = linuxcnc.command()
s = linuxcnc.stat()


def poll():
    s.poll()


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_AUTO)
time.sleep(0.2)

c.program_open("test.ngc")
time.sleep(0.2)
c.auto(linuxcnc.AUTO_RUN, 0)

saw_position_near_target = False
homed_while_short_of_target = None
t0 = time.time()
while time.time() - t0 < 10.0:
    poll()
    if s.position[0] > 1.9:
        saw_position_near_target = True
    if s.homed[0] and not saw_position_near_target:
        homed_while_short_of_target = s.position[0]
        break
    if s.exec_state == linuxcnc.EXEC_DONE and s.interp_state == linuxcnc.INTERP_IDLE:
        break
    time.sleep(0.001)

if homed_while_short_of_target is not None:
    fail("joint 0 reported homed while X was still at {} (target 2.0) -- "
         "the queued move was reordered to run after G28.2 P0".format(homed_while_short_of_target))

if not saw_position_near_target:
    fail("X never reached its target -- move did not run at all")

t1 = time.time()
while time.time() - t1 < 5.0:
    poll()
    if s.exec_state == linuxcnc.EXEC_DONE and s.interp_state == linuxcnc.INTERP_IDLE:
        break
    time.sleep(0.01)

if not s.homed[0]:
    fail("joint 0 never ended up homed")

print("PASS: the queued move completed before G28.2 P0 homed the joint")
print("done! it all worked")
sys.exit(0)
