# The machine, the helpers and the finish that twp-native and
# twp-native-ptp share: see their READMEs.


import linuxcnc
import hal
import sys
import os
import time
import math
import numpy as np

TOPDIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(TOPDIR, "tests", "kins-twp", "xyzacb"))
import remap_funcs_twp as twp

JOINTS = 6
TABLE, SECONDARY, PRIMARY = 3, 4, 5      # A, B, C
I4 = np.asmatrix(np.identity(4))

class Log:
    def debug(self, *a): pass
    def error(self, *a): print("oracle:", a)
log = Log()

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

def drain():
    while True:
        m = e.poll()
        if not m:
            return
        print("channel:", m)
        if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            error("reported: %s" % m[1])

# what c.wait_complete() waits for, task done with the last command sent,
# looked at every 2 ms rather than every 10: a step of several commands
# pays that wait on each
def done(timeout=60):
    deadline = time.time() + timeout
    while time.time() < deadline:
        s.poll()
        if s.echo_serial_number >= c.serial and s.state != linuxcnc.RCS_EXEC:
            return
        time.sleep(0.002)
    error("timed out waiting for task")

# the move has ended when task has nothing left to run and motion is at
# rest; in position with an empty queue alone is also true before task has
# sent the move on.  Status is a copy task writes once a cycle, so it only
# counts from a copy written two task cycles after the command, and the
# joints only count as still over two copies a servo cycle or more apart
def settled():
    deadline = time.time() + 60
    s.poll()
    beat = s.taskbeat + 2
    last = None
    last_hb = None
    while time.time() < deadline:
        s.poll()
        if s.taskbeat >= beat:
            now = [s.joint_position[i] for i in range(JOINTS)]
            if (s.interp_state == linuxcnc.INTERP_IDLE and s.inpos and not s.queue
                    and now == last and s.heartbeat != last_hb):
                return now
            last = now
            last_hb = s.heartbeat
        time.sleep(0.002)
    error("timed out waiting for the move")
    return last

def mdi(*cmds):
    for cmd in cmds:
        c.mdi(cmd)
        done()
    return settled()

# the path of a move is read from the sampler log, every servo cycle of the
# joint commands and the world position, which halsampler writes to
# samples.log as it runs; status would give a snapshot per task cycle, which
# misses the turn of a move that stops and comes back
LOG = "samples.log"

# the samples read so far, and how far into the file they reach: each read
# takes only the lines written since
sampler = {"samples": [], "at": 0}

def log_samples():
    with open(LOG) as f:
        f.seek(sampler["at"])
        text = f.read()
    end = text.rfind("\n") + 1     # the rest is a line still being written
    sampler["at"] += len(text[:end].encode())
    for line in text[:end].split("\n")[:-1]:
        v = line.split()
        if len(v) != 1 + JOINTS + 3:
            continue               # an "overrun" line, counted from the pin at the end
        v = [float(x) for x in v[1:]]
        sampler["samples"].append((v[:JOINTS], v[JOINTS:]))
    return sampler["samples"]

# the log is block buffered: read it until its last line has the machine
# at rest where it is, after n0 lines
def caught_up(what, at, n0=0):
    deadline = time.time() + 10
    while True:
        samples = log_samples()
        if len(samples) > n0 and close(samples[-1][0], at, 2e-6):
            return samples
        if time.time() > deadline:
            error("%s: the sampler log did not catch up with the machine" % what)
            return samples
        time.sleep(0.01)

# run one command and return the settled joints and the log of the way there,
# from a log that has caught up with the machine before the move as well
def sampled(cmd):
    s.poll()
    n0 = len(caught_up(cmd, [s.joint_position[i] for i in range(JOINTS)]))
    c.mdi(cmd)
    done()
    end = settled()
    samples = caught_up(cmd, end, n0)
    return end, samples[n0:]

# a point-to-point move runs every joint on a straight line in joint space,
# all together: the fraction of the way each moving joint has gone is the
# same for all of them at every sample, never goes back, and reaches one;
# the log prints six decimals, which on a short move is 1e-5 of the way
def joint_line(what, samples, start, end):
    moving = [i for i in range(JOINTS) if abs(end[i] - start[i]) > 1e-6]
    if not moving:
        error("%s: no joint moved" % what)
        return
    last = 0.0
    for n, (j, p) in enumerate(samples):
        fs = [(j[i] - start[i]) / (end[i] - start[i]) for i in moving]
        f = sum(fs) / len(fs)
        if max(abs(x - f) for x in fs) > 1e-5:
            error("%s: joints out of step at sample %d of %d: %s" % (what, n, len(samples), fs))
            return
        if f < last - 1e-6:
            error("%s: the joint path ran backwards at sample %d of %d (%.4f after %.4f)" % (what, n, len(samples), f, last))
            return
        if f < -1e-6 or f > 1 + 1e-6:
            error("%s: a joint left its segment at sample %d of %d (fraction %.4f)" % (what, n, len(samples), f))
            return
        last = f
    if last < 1 - 1e-6:
        error("%s: the last sample is short of the end (fraction %.4f)" % (what, last))
    print("%s: %d joints on one line in joint space through %d samples" % (what, len(moving), len(samples)))

def show(what, j):
    print("%-26s %s" % (what, " ".join("%.4f" % v for v in j)))

def wrap(d):
    return (d + 180.0) % 360.0 - 180.0

def close(a, b, tol=1e-6):
    return all(abs(x - y) <= tol for x, y in zip(a, b))

# the pairs the oracle would keep for a tool axis, in machine coordinates,
# as (b, c) in degrees, and which is nearest to the head's present angles
def oracle_pairs(z):
    t1s, t2s = twp.kins_calc_possible_joint_angles(log, list(z), None)
    pairs = []
    for t1 in t1s or []:
        for t2 in t2s or []:
            m = twp.kins_calc_transformation_matrix(t1, t2, 0, I4, 'inv')
            got = [m[0, 2], m[1, 2], m[2, 2]]
            if close(got, z, 1e-6):
                pairs.append((math.degrees(t2), math.degrees(t1)))
    return pairs

def nearest_pair(pairs, b_now, c_now):
    return min(pairs, key=lambda p: abs(wrap(p[0] - b_now)) + abs(wrap(p[1] - c_now)))

def tool_axis(joints):
    # the tool axis in work coordinates: the head as the oracle models it,
    # brought into the table's frame the way the module reports the work
    m = twp.kins_calc_transformation_matrix(math.radians(joints[PRIMARY]),
                                            math.radians(joints[SECONDARY]), 0, I4, 'inv')
    zm = np.array([m[0, 2], m[1, 2], m[2, 2]])
    a = math.radians(joints[TABLE])
    W = np.array([[1, 0, 0], [0, math.cos(a), math.sin(a)], [0, -math.sin(a), math.cos(a)]])
    return W.T.dot(zm)

def plane_axes():
    s.poll()
    r = s.g68_rotation
    return ([r[0], r[3], r[6]], [r[1], r[4], r[7]], [r[2], r[5], r[8]])

def rot_x(d):
    r = math.radians(d)
    return np.array([[1, 0, 0], [0, math.cos(r), -math.sin(r)], [0, math.sin(r), math.cos(r)]])

def rot_y(d):
    r = math.radians(d)
    return np.array([[math.cos(r), 0, math.sin(r)], [0, 1, 0], [-math.sin(r), 0, math.cos(r)]])

# the sampler kept up, and the count of failures, for checkresult
def finish():
    for f in ("sim.var", "sim.var.bak"):
        try:
            os.unlink(f)
        except OSError:
            pass

    overruns = int(hal.get_value("sampler.0.overruns"))
    if overruns:
        error("the sampler lost %d samples" % overruns)
    if not errors:
        os.unlink(LOG)
    print("Exiting with %d errors" % errors)
    sys.exit(1 if errors else 0)
