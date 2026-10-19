# The machine, the helpers and the finish that the twp-static tests share:
# see twp-static/README.
#
# The sims move fast, and task runs a 1 ms cycle: most checks look only at
# where a move ends.  The tests wait on what c.wait_complete() waits for,
# polled every 2 ms, and on the joints at rest over two status copies.

import linuxcnc
import sys
import os
import time

ini = linuxcnc.ini(os.environ["INI_FILE_NAME"])
HEAD = ini.find("KINS", "KINEMATICS").startswith("5axiskins")
JOINTS = int(ini.find("KINS", "JOINTS"))
# the type that follows the part, and the rotaries' letters
TCP = "G12.1 P0" if HEAD else "G12.1 P1"
FIRST, SECOND = ("B", "C") if HEAD else ("A", "C")
START = "G53 G0 X20 Y-10 Z120 B0 C0" if HEAD else "G53 G0 X20 Y-10 Z120 A0 C0"

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()
c.mode(linuxcnc.MODE_MDI)

errors = 0

def error(msg):
    global errors
    errors += 1
    print("*** ERROR " + msg)

def drain(expect_error=False):
    got = False
    while True:
        m = e.poll()
        if not m:
            return got
        print("channel:", m)
        if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            got = True
            if not expect_error:
                error("reported: %s" % m[1])

# what c.wait_complete() waits for, task done with the last command sent,
# looked at every 2 ms rather than every 10
def done(timeout=60):
    deadline = time.time() + timeout
    while time.time() < deadline:
        s.poll()
        if s.echo_serial_number >= c.serial and s.state != linuxcnc.RCS_EXEC:
            return s.state
        time.sleep(0.002)
    error("timed out waiting for task")
    return s.state

# the joints still over two status copies a servo cycle or more apart, on a
# copy written two task cycles after the command: two polls can read the
# same copy
def still(ready=lambda: True):
    deadline = time.time() + 60
    s.poll()
    beat = s.taskbeat + 2
    last = None
    last_hb = None
    while time.time() < deadline:
        s.poll()
        if s.taskbeat >= beat:
            now = [s.joint_position[i] for i in range(JOINTS)]
            if ready() and now == last and s.heartbeat != last_hb:
                return now
            last = now
            last_hb = s.heartbeat
        time.sleep(0.002)
    error("timed out waiting for the move")
    return last

# the move has ended when task has nothing left to run and motion is at
# rest; in position with an empty queue alone is also true before task has
# sent the move on
def settled():
    return still(lambda: s.interp_state == linuxcnc.INTERP_IDLE and s.inpos and not s.queue)

def mdi(cmd):
    c.mdi(cmd)
    done()
    j = settled()
    drain()
    return j

# no tool, no plane, the machine frame type, in one block
RESET = "G49 G69 G13.1"

def run(select, steps):
    # from one machine point, on the type asked for, the joints after each step
    for cmd in (RESET, START, select):
        mdi(cmd)
    return [mdi(cmd) for cmd in steps]

def same(a, b, n=None):
    n = JOINTS if n is None else n
    return max(abs(p - q) for p, q in zip(a[:n], b[:n])) < 1e-6

def show(j):
    return " ".join("%.6f" % v for v in j)

plane = "G68.2 X10 Y-5 Z0 I60 J30 K0"

def finish():
    mdi("G69")
    mdi("G49")
    for f in ("sim.var", "sim.var.bak", "preview.var"):
        try:
            os.unlink(f)
        except OSError:
            pass
    print("Exiting with %d errors" % errors)
    sys.exit(1 if errors else 0)
