#!/usr/bin/env python3
# RETAIN_WORK_PLANE keeps the tilted work plane through M2 and an abort.
# After an abort the plane is the one the machine was in, not the one the
# read ahead had reached; each case checks it by moving to the plane's
# origin, which the interpreter only finds if it agrees with the machine.
# Run once with the setting and once without.
import os, sys, time
import linuxcnc, linuxcnc_util

result = open("result", "a")
def say(*a):
    print(*a, file=result, flush=True)

inifile = linuxcnc.ini(os.environ["INI_FILE_NAME"])
say("RETAIN_WORK_PLANE = %s" % (inifile.find("RS274NGC", "RETAIN_WORK_PLANE") or "0"))
c = linuxcnc.command()
e = linuxcnc.error_channel()
s = linuxcnc.stat()
l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MANUAL)
c.home(-1)
l.wait_for_home(joints=[1, 1, 1, 1, 0, 0, 0, 0, 0])

def errors():
    msgs = []
    while True:
        m = e.poll()
        if not m:
            return msgs
        msgs.append(m[1])

def mdi(*cmds):
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete()
    for cmd in cmds:
        c.mdi(cmd)
        c.wait_complete()
        l.wait_for_interp_state(linuxcnc.INTERP_IDLE, timeout=30)

def run(prog, abort_after=None):
    c.mode(linuxcnc.MODE_AUTO)
    c.wait_complete()
    c.program_open(prog)
    c.wait_complete()
    c.auto(linuxcnc.AUTO_RUN, 0)
    c.wait_complete()
    if abort_after is not None:
        time.sleep(abort_after)
        c.abort()
        c.wait_complete()
    time.sleep(0.3)
    l.wait_for_interp_state(linuxcnc.INTERP_IDLE, timeout=30)

def at(x, y, z):
    # the machine at a point in G54 millimetres; status is in inches
    s.poll()
    d = max(abs(s.position[i] * 25.4 - v) for i, v in enumerate((x, y, z)))
    return "at %g %g %g" % (x, y, z) if d < 1e-6 else \
        "at %.4f %.4f %.4f" % tuple(s.position[i] * 25.4 for i in range(3))

def case(name, prog, setup, abort_after, origin):
    mdi("G69", "G21", "G54", "G52 X0 Y0 Z0", "G10 L2 P2 X5 Y0 Z0", "G0 X0 Y0 Z0", *setup)
    errors()
    if prog:
        run(prog, abort_after)
    s.poll()
    active = s.g68_active
    s.poll()
    g5x = s.g5x_index
    mdi("G52 X0 Y0 Z0", "G1 X0 Y0 Z0 F2000")
    msgs = errors()
    say("%s: plane %s, G5%d, %s%s" % (name, "kept" if active else "cancelled", 3 + g5x, at(*origin),
                                "; " + "; ".join(msgs) if msgs else ""))

# where G1 X0 Y0 Z0 lands with the plane kept and with it cancelled
o, z = (10, 20, 5), (0, 0, 0)
retain = inifile.find("RS274NGC", "RETAIN_WORK_PLANE") == "1"
# no program: a change of coordinate system in the plane takes it along
case("G55 in the plane", None, ["G68.2 X10 Y20 Z5 I10 J20 K30", "G55"], None, (15, 20, 5))
case("M2", "end.ngc", ["G68.2 X10 Y20 Z5 I10 J20 K30"], None, o if retain else z)
case("abort, read ahead in the next plane", "readahead.ngc", [], 1.0, o if retain else z)
case("abort, read ahead past G55", "g55.ngc", [], 1.0, o if retain else z)
case("abort, read ahead in a plane on G55", "g55plane.ngc", [], 1.0, z)
case("abort, G52 in the plane", "slow.ngc", ["G68.2 X10 Y20 Z5 I10 J20 K30", "G52 X1"], 1.0, o if retain else z)
mdi("G69")
c.state(linuxcnc.STATE_OFF)
