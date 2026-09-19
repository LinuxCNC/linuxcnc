#!/usr/bin/env python3
# A turn of the head under the tool centre point must not run a slide past
# its own limits: see README.
import hal
import linuxcnc
import os
import sys
import time

JOINTS = 6
LOG = "samples.log"
SERVO = 0.001

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
        time.sleep(0.05)
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

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete(30)
c.home(-1)
c.wait_complete(60)
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
drain()

# the tool centre point kinematics, the head laid over by a joint move so
# that getting there cannot swing anything; the carriage at X-400 puts the
# tip at X0, the centre of the half circle it is about to draw
mdi("G12.1 P0")
mdi("G53.7 G0 J0=-400 J1=0 J2=0 J3=90 J4=0 J5=0")
start = joints()
s.poll()
tip = list(s.position[:3])
print("start joints %s, tip %s" % (" ".join("%.3f" % v for v in start),
                                  " ".join("%.3f" % v for v in tip)))
drain()
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

def swing(cmd, n0):
    c.mdi(cmd)
    c.wait_complete(60)
    end = settled()
    said = drain()
    time.sleep(0.5)
    samples = log_samples()[n0:]
    for m in said:
        print("channel:", m)
    faults = [m for m in said if "following error" in m[1]]
    if faults:
        error("%s tripped a following error: %s" % (cmd, faults[0][1].strip()))
    s.poll()
    if s.task_state != linuxcnc.STATE_ON:
        error("the machine is not on after %s (task state %d)" % (cmd, s.task_state))
    # the commanded velocity and acceleration of every joint against its
    # own INI limit, every servo cycle; a joint over its limit is what the
    # drive could not follow.  A fault freezes the command in one cycle,
    # which is not an acceleration the planner asked for: the samples stop
    # at the last moving one
    last = max((k for k in range(len(samples)) if any(abs(v) > 0 for v in samples[k][1])), default=-1)
    samples = samples[:last + 1]
    print("%d samples through %s" % (len(samples), cmd))
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

# the swing: the tip holds, C turns half a revolution, the carriage follows
# a half circle of radius 400 to keep the tip where it is
end = swing("G0 C180", catch_up(start))
if abs(end[4] - 180) > 1e-3:
    error("C ended at %.4f, not 180" % end[4])
tip_held("G0 C180")

# the same on an arc: the tip draws a full circle of radius 100 at a feed
# it could keep on its own while C turns another revolution, so the
# carriage rides the small circle and the big swing together
end = swing("G17 G2 X0 Y0 I100 J0 C540 F5000", catch_up(end))
if abs(end[4] - 540) > 1e-3:
    error("C ended at %.4f, not 540" % end[4])
tip_held("the arc")

overruns = int(hal.get_value("sampler.0.overruns"))
if overruns:
    error("the sampler lost %d samples" % overruns)
if not errors:
    os.unlink(LOG)
print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
