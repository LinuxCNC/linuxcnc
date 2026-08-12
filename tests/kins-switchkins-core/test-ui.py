#!/usr/bin/env python3
# A kinematics module on switchkins_core: switchkinscomp registers identity
# as type 0 and an X offset of 5 as type 1, and switchkins_core does the rest.
import hal
import linuxcnc
import sys
import time

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

errors = 0

def error(what):
    global errors
    errors += 1
    print("*** ERROR %s" % what)

def kins_type():
    return int(hal.get_value("motion.kins-type"))

def position():
    s.poll()
    return tuple(round(v, 6) for v in s.position[:3])

def joints():
    s.poll()
    return tuple(round(v, 6) for v in s.joint_position[:3])

def drain():
    said = []
    while True:
        m = e.poll()
        if not m:
            return said
        said.append(m[1])

def wait_idle():
    deadline = time.time() + 30
    while time.time() < deadline:
        s.poll()
        if s.interp_state == linuxcnc.INTERP_IDLE and not s.queue:
            return
        time.sleep(0.05)
    error("timed out waiting for the interpreter")

def mdi(cmd):
    c.mdi(cmd)
    c.wait_complete(30)
    wait_idle()
    time.sleep(0.2)   # let the error channel and the status catch up

def check(what, got, want):
    if got != want:
        error("%s is %s, expected %s" % (what, got, want))

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete(30)
c.home(-1)
c.wait_complete(60)
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
drain()

# the pins switchkins_core creates on the module's own component
check("kinematics type at start", kins_type(), 0)
check("kinstype.is-0", hal.get_value("kinstype.is-0"), True)
check("kinstype.is-1", hal.get_value("kinstype.is-1"), False)

# identity: joints and coordinates are the same thing
mdi("G0 X10 Y2 Z3")
check("joints in identity", joints(), (10, 2, 3))
check("position in identity", position(), (10, 2, 3))

# type 1 puts X at joint 0 plus the offset, the joints stay where they are
mdi("G12.1 P1")
check("kinematics type after G12.1 P1", kins_type(), 1)
check("kinstype.is-1", hal.get_value("kinstype.is-1"), True)
check("joints after the switch", joints(), (10, 2, 3))
check("position after the switch", position(), (15, 2, 3))

# and moves through the module's inverse
mdi("G0 X20")
check("joints after G0 X20 in type 1", joints(), (15, 2, 3))
check("position after G0 X20 in type 1", position(), (20, 2, 3))

# back to identity
mdi("G12.1 P0")
check("kinematics type after G12.1 P0", kins_type(), 0)
check("position back in identity", position(), (15, 2, 3))
said = drain()
if said:
    error("unexpected messages: %s" % said)

# a type the module does not have is refused and changes nothing
mdi("G12.1 P2")
said = drain()
if not said:
    error("no report of the refused switch to type 2")
check("kinematics type after G12.1 P2", kins_type(), 0)
check("position after the refused switch", position(), (15, 2, 3))

print("Exiting with %d errors" % errors)
c.state(linuxcnc.STATE_ESTOP)
sys.exit(1 if errors else 0)
