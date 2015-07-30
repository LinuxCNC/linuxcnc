#!/usr/bin/python

import sys
import os
import time

import linuxcnc


# unbuffer stdout
sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)


def wait_for_tool(toolno):
    print "waiting for tool", toolno, "in the spindle"
    timeout = 10
    start = time.time()
    s.poll()
    while s.tool_in_spindle != toolno:
        if (time.time() - start) > timeout:
            print "timeout waiting for tool", toolno
            print "s.tool_in_spindle = ", s.tool_in_spindle
            sys.exit(1)
        time.sleep(0.1)
        s.poll()
    print "got tool %d after %.6f seconds" % (toolno, time.time() - start)


def run_mdi(gcode):
    print "mdi:", gcode
    c.mdi(gcode)
    c.wait_complete()

    error = e.poll()
    if error:
        kind, text = error
        if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            print "MDI error " + text
            sys.exit(1)
        else:
            print "MDI info " + text


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)

c.home(-1)
c.wait_complete()

#print "waiting for homing"
#timeout = 20
#start = time.time()
#s.poll()
#while s.homed != (1, 1, 1, 0, 0, 0, 0, 0, 0):
#    if (time.time() - start) > timeout:
#        print "timeout waiting for homing"
#        print "s.homed = ", s.homed
#        sys.exit(1)
#    time.sleep(0.1)
#    s.poll()

c.mode(linuxcnc.MODE_MDI)

run_mdi('t1 m6')
wait_for_tool(1)

run_mdi('t10 m6')
wait_for_tool(10)

