#!/usr/bin/env python3
# A world jog under the tool centre point slows to a stop where a joint
# reaches its soft limit, and a jog away from it is free: see README.
import hal
import linuxcnc
import os
import sys
import time

JOINTS = 6
LOG = "samples.log"
SERVO = 0.001
C = 5

ini = linuxcnc.ini("test.ini")
VEL = [float(ini.find("JOINT_%d" % j, "MAX_VELOCITY")) for j in range(JOINTS)]
ACC = [float(ini.find("JOINT_%d" % j, "MAX_ACCELERATION")) for j in range(JOINTS)]

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

errors = 0

def error(what):
    global errors
    errors += 1
    print("*** ERROR %s" % what)

def joints():
    s.poll()
    return [s.joint_position[i] for i in range(JOINTS)]

def drain():
    said = []
    while True:
        m = e.poll()
        if not m:
            return said
        said.append(m)

def settled():
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        now = joints()
        if s.inpos and not s.queue and now == last:
            return now
        last = now
        time.sleep(0.01)
    error("timed out waiting for the move")
    return last

def mdi(cmd):
    c.mdi(cmd)
    c.wait_complete(60)
    return settled()

def log_samples():
    with open(LOG) as f:
        lines = f.read().split("\n")
    out = []
    for line in lines[:-1]:
        v = line.split()
        if len(v) != 1 + 2 * JOINTS:
            continue
        v = [float(x) for x in v[1:]]
        out.append((v[:JOINTS], v[JOINTS:]))
    return out

def catch_up(where):
    # the log is block buffered: wait until it has caught up with the
    # machine at rest before marking where the next move starts in it
    deadline = time.time() + 10
    while True:
        samples = log_samples()
        if samples and max(abs(a - b) for a, b in zip(samples[-1][0], where)) < 2e-6:
            return len(samples)
        if time.time() > deadline:
            error("the sampler log did not catch up with the machine")
            return len(samples)
        time.sleep(0.02)

def checked(what, n0):
    end = settled()
    catch_up(end)
    said = drain()
    samples = log_samples()[n0:]
    for m in said:
        print("channel:", m)
    # a jog that stops at a joint's limit says nothing: no soft limit
    # error, no following error
    if said:
        error("%s said: %s" % (what, said[0][1].strip()))
    s.poll()
    if s.task_state != linuxcnc.STATE_ON:
        error("the machine is not on after %s (task state %d)" % (what, s.task_state))
    # the commanded velocity and acceleration of every joint against its
    # own INI limit, every servo cycle; a joint over its limit is what the
    # drive could not follow.  A fault freezes the command in one cycle,
    # which is not an acceleration the planner asked for: the samples stop
    # at the last moving one
    last = max((k for k in range(len(samples)) if any(abs(v) > 0 for v in samples[k][1])), default=-1)
    samples = samples[:last + 1]
    print("%d samples through %s" % (len(samples), what))
    for j in range(JOINTS):
        vpeak = max(abs(v[j]) for p, v in samples) if samples else 0.0
        apeak = 0.0
        for k in range(1, len(samples)):
            apeak = max(apeak, abs(samples[k][1][j] - samples[k - 1][1][j]) / SERVO)
        print("joint %d: velocity peak %8.3f of %8.3f (%.2fx), acceleration peak %9.2f of %9.2f (%.2fx)"
              % (j, vpeak, VEL[j], vpeak / VEL[j], apeak, ACC[j], apeak / ACC[j]))
        if vpeak > VEL[j] * 1.001:
            error("joint %d was commanded at %.3f, over its limit of %.3f" % (j, vpeak, VEL[j]))
        if apeak > ACC[j] * 1.01:
            error("joint %d was commanded at %.2f, over its acceleration limit of %.2f" % (j, apeak, ACC[j]))
    return end

def tip_held(what):
    s.poll()
    after = list(s.position[:3])
    if max(abs(a - b) for a, b in zip(after, tip)) > 1e-3:
        error("the tip moved from %s to %s through %s" % (tip, after, what))


Y = 1
YMAX = float(ini.find("JOINT_1", "MAX_LIMIT"))
YMIN = float(ini.find("JOINT_1", "MIN_LIMIT"))

def at_limit(what, end):
    # the carriage came to rest against its travel, not short of it and
    # not past it
    room = min(YMAX - end[Y], end[Y] - YMIN)
    print("%s: joint 1 at %.6f, %.6f from its limit, C %.3f" % (what, end[Y], room, end[4]))
    if room < 0:
        error("%s: joint 1 at %.6f is past its limit" % (what, end[Y]))
    elif room > 1e-3:
        error("%s: joint 1 stopped %.6f short of its limit" % (what, room))

def jog_ended(what):
    s.poll()
    if any(s.axis[a]["velocity"] != 0 for a in range(9)):
        error("%s: an axis is still moving" % what)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete(30)
c.home(-1)
c.wait_complete(60)
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
drain()

# the tool centre point kinematics, the head laid over by a joint move;
# the carriage at X-400 puts the tip at X0, the centre of the circle a jog
# of C swings the carriage on.  Joint 1 has 300 mm of travel each way, so
# the circle leaves it about 48.6 degrees round
mdi("G12.1 P0")
mdi("G53.7 G0 J0=-400 J1=0 J2=0 J3=90 J4=0 J5=0")
start = joints()
s.poll()
tip = list(s.position[:3])
drain()
c.mode(linuxcnc.MODE_MANUAL)
c.wait_complete(30)
c.teleop_enable(1)
c.wait_complete(30)

# a continuous jog of C held well past where joint 1 runs out: the jog
# slows and comes to rest with the carriage at its limit, and ends
n0 = catch_up(start)
c.jog(linuxcnc.JOG_CONTINUOUS, 0, C, 480)
deadline = time.time() + 60
moved = False
while time.time() < deadline:
    s.poll()
    if abs(s.position[C]) > 10:
        moved = True
    # held until the jog has come to rest with C well round, or a limit
    # error has stopped it
    if moved and s.axis[C]["velocity"] == 0:
        break
    if s.task_state != linuxcnc.STATE_ON:
        break
    time.sleep(0.01)
else:
    error("the continuous jog of C did not come to rest")
c.jog(linuxcnc.JOG_STOP, 0, C)
end = checked("the continuous jog of C into the limit", n0)
at_limit("the continuous jog of C", end)
tip_held("the continuous jog of C")
jog_ended("the continuous jog of C")
stop_c = end[4]

# away from the limit by 20 degrees: free, and it lands where it was sent
n0 = catch_up(end)
c.jog(linuxcnc.JOG_INCREMENT, 0, C, 480, -20 if stop_c > 0 else 20)
end = checked("the incremental jog of C away", n0)
want = stop_c - 20 if stop_c > 0 else stop_c + 20
if abs(end[4] - want) > 1e-3:
    error("C ended at %.3f, not %.3f, jogging away from the limit" % (end[4], want))

# an incremental jog of 60 degrees back towards it: it stops at the
# limit, short of where it was sent, and ends there
n0 = catch_up(end)
c.jog(linuxcnc.JOG_INCREMENT, 0, C, 480, 60 if stop_c > 0 else -60)
end = checked("the incremental jog of C into the limit", n0)
at_limit("the incremental jog of C", end)
tip_held("the incremental jog of C")
jog_ended("the incremental jog of C")

# and a jog away works straight after it
n0 = catch_up(end)
c.jog(linuxcnc.JOG_INCREMENT, 0, C, 480, -10 if stop_c > 0 else 10)
again = checked("the jog away after the incremental one", n0)
want = end[4] - 10 if stop_c > 0 else end[4] + 10
if abs(again[4] - want) > 1e-3:
    error("C ended at %.3f, not %.3f, jogging away again" % (again[4], want))

overruns = int(hal.get_value("sampler.0.overruns"))
if overruns:
    error("the sampler lost %d samples" % overruns)
if not errors:
    os.unlink(LOG)
print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
