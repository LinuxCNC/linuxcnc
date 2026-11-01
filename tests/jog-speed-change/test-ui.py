#!/usr/bin/env python3
# A continuous jog asked again at a lower speed while it runs: see README.
import hal
import linuxcnc
import os
import sys
import time

FAST = 80.0
SLOW = 10.0
LOG = "samples.log"
SERVO = 0.001

ini = linuxcnc.ini(sys.argv[2] if len(sys.argv) > 2 else "test.ini")
ACC = float(ini.find("JOINT_0", "MAX_ACCELERATION"))

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

errors = 0

def error(what):
    global errors
    errors += 1
    print("*** ERROR %s" % what)

def drain():
    while True:
        m = e.poll()
        if not m:
            break
        print("channel:", m)
        if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            error("reported: %s" % m[1].strip())

def wait_for(what, test, timeout=30):
    deadline = time.time() + timeout
    while time.time() < deadline:
        s.poll()
        if test():
            return True
        time.sleep(0.05)
    error("timed out waiting for %s" % what)
    return False

def settled():
    wait_for("the machine to stop",
             lambda: s.inpos and not hal.get_value("motion.jog-is-active"))

def slow_down(jjog, what):
    # FAST, then the same jog again at SLOW while it still runs
    c.jog(linuxcnc.JOG_CONTINUOUS, jjog, 0, FAST)
    time.sleep(0.5)
    c.jog(linuxcnc.JOG_CONTINUOUS, jjog, 0, SLOW)
    time.sleep(0.5)
    s.poll()
    v = abs(s.joint[0]["velocity"])
    print("%s: %.3f after the slow down" % (what, v))
    if abs(v - SLOW) > 0.01:
        error("%s runs at %.3f, not %.3f" % (what, v, SLOW))
    c.jog(linuxcnc.JOG_STOP, jjog, 0)
    settled()
    drain()

h = hal.component("test-ui")
h.ready()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MANUAL)
c.wait_complete(30)
c.home(-1)
c.wait_complete(30)
wait_for("homing", lambda: all(s.homed[j] for j in range(3)))
drain()

# a joint jog in free mode
c.teleop_enable(0)
c.wait_complete(30)
wait_for("free mode", lambda: s.motion_mode == linuxcnc.TRAJ_MODE_FREE)
slow_down(True, "joint 0")

# a world jog in teleop mode
c.teleop_enable(1)
c.wait_complete(30)
wait_for("teleop mode", lambda: s.motion_mode == linuxcnc.TRAJ_MODE_TELEOP)
slow_down(False, "axis X")

# every servo cycle: the speed comes down to SLOW within the acceleration
# limit, it does not drop at once
time.sleep(0.5)
with open(LOG) as f:
    lines = f.read().split("\n")
samples = []
for line in lines[:-1]:
    v = line.split()
    if len(v) == 3:
        samples.append([float(x) for x in v[1:]])
print("%d samples" % len(samples))
for i, what in enumerate(("joint 0", "axis X")):
    vpeak = max(abs(v[i]) for v in samples) if samples else 0.0
    apeak = max((abs(samples[k][i] - samples[k - 1][i]) / SERVO
                 for k in range(1, len(samples))), default=0.0)
    jpeak = max((abs(samples[k][i] - 2 * samples[k - 1][i] + samples[k - 2][i]) / SERVO ** 2
                 for k in range(2, len(samples))), default=0.0)
    print("%s: velocity peak %.3f of %.3f, acceleration peak %.2f of %.2f, jerk peak %.0f"
          % (what, vpeak, FAST, apeak, ACC, jpeak))
    if vpeak > FAST * 1.001:
        error("%s ran at %.3f, over the %.3f asked" % (what, vpeak, FAST))
    if apeak > ACC * 1.001:
        error("%s accelerated at %.2f, over its limit of %.2f" % (what, apeak, ACC))
if not samples:
    error("no samples")
overruns = int(hal.get_value("sampler.0.overruns"))
if overruns:
    error("the sampler lost %d samples" % overruns)
if not errors:
    os.unlink(LOG)

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
