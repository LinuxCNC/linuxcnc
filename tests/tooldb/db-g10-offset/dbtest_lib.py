# helpers shared by the ui scripts of this test

import sys
import time

import linuxcnc

savefile = "db_tools.txt"


def wait_for_linuxcnc_startup(status, timeout=10.0):
    """Poll the Status buffer waiting for it to look initialized, rather
    than just allocated (all-zero)."""
    start_time = time.time()
    while time.time() - start_time < timeout:
        status.poll()
        if (status.angular_units == 0.0) \
            or (status.axis_mask == 0) \
            or (status.cycle_time == 0.0) \
            or (status.exec_state != linuxcnc.EXEC_DONE) \
            or (status.interp_state != linuxcnc.INTERP_IDLE) \
            or (status.inpos is False) \
            or (status.linear_units == 0.0) \
            or (status.max_acceleration == 0.0) \
            or (status.max_velocity == 0.0) \
            or (status.program_units == 0.0) \
            or (status.rapidrate == 0.0) \
            or (status.state != linuxcnc.RCS_DONE) \
            or (status.task_state != linuxcnc.STATE_ESTOP):
            time.sleep(0.1)
        else:
            return
    raise RuntimeError("timeout waiting for linuxcnc startup")


def start_machine():
    c = linuxcnc.command()
    s = linuxcnc.stat()
    wait_for_linuxcnc_startup(s)
    c.state(linuxcnc.STATE_ESTOP_RESET)
    c.state(linuxcnc.STATE_ON)
    c.home(-1)
    c.wait_complete()
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete()
    return (c, s)


def fail(msg):
    print("FAIL: %s" % msg)
    sys.exit(1)


def check(what, got, expected):
    if abs(got - expected) > 1e-6:
        fail("%s: expected %.6f, got %.6f" % (what, expected, got))
    print("ok: %s = %.6f" % (what, got))


def tool_table_zoffset(s, idx):
    s.poll()
    return s.tool_table[idx].zoffset


def db_zoffset(toolno, timeout=0):
    """Z offset recorded by the db program for toolno, or None."""
    deadline = time.time() + timeout
    while True:
        try:
            with open(savefile) as f:
                for line in f:
                    items = line.upper().split()
                    if "T%d" % toolno not in items:
                        continue
                    for item in items:
                        if item.startswith("Z"):
                            return float(item[1:])
        except IOError:
            pass
        if time.time() >= deadline:
            return None
        time.sleep(0.1)
