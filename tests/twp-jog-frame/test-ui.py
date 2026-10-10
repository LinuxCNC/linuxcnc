#!/usr/bin/env python3
# World jogs of X, Y and Z along the tilted work plane and along the tool:
# see README.
import hal
import linuxcnc
import math
import os
import sys
import time

JOINTS = 6
X, Y, Z = 0, 1, 2
B, C = 4, 5
VEL_Y = 50.0
LOG = "samples.log"
SERVO = 0.001

ini = linuxcnc.ini("test.ini")
AXIS_VEL = [float(ini.find("AXIS_%s" % l, "MAX_VELOCITY")) for l in "XYZ"]
AXIS_ACC = [float(ini.find("AXIS_%s" % l, "MAX_ACCELERATION")) for l in "XYZ"]

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

errors = 0

def error(what):
    global errors
    errors += 1
    print("*** ERROR %s" % what)

def drain(expect=None):
    # the messages since the last call; any error is a failure unless it
    # carries the text expected
    said = []
    while True:
        m = e.poll()
        if not m:
            break
        print("channel:", m)
        said.append(m[1])
        if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR) \
           and not (expect and expect in m[1]):
            error("reported: %s" % m[1].strip())
    return said

def settled():
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        s.poll()
        now = list(s.position)
        if s.inpos and not s.queue and now == last \
           and not hal.get_value("motion.jog-is-active"):
            return now
        last = now
        time.sleep(0.05)
    error("timed out waiting for the machine to stop")
    return last

def mdi(*cmds):
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete(30)
    for cmd in cmds:
        c.mdi(cmd)
        c.wait_complete(60)
    settled()
    drain()

def teleop():
    c.mode(linuxcnc.MODE_MANUAL)
    c.wait_complete(30)
    c.teleop_enable(1)
    c.wait_complete(30)
    deadline = time.time() + 10
    while time.time() < deadline:
        s.poll()
        if s.motion_mode == linuxcnc.TRAJ_MODE_TELEOP:
            return
        time.sleep(0.05)
    error("motion did not enter teleop mode")

def frame(which):
    c.jog_frame(which)
    c.wait_complete(30)
    time.sleep(0.05)
    s.poll()
    if s.jog_frame != which:
        error("the jog frame is %d, not %d" % (s.jog_frame, which))
    if int(hal.get_value("motion.jog-frame")) != which:
        error("motion.jog-frame is %s, not %d" % (hal.get_value("motion.jog-frame"), which))

def column(m, k):
    return [m[3 * i + k] for i in range(3)]

def rot_z(deg, v):
    t = math.radians(deg)
    return [math.cos(t) * v[0] - math.sin(t) * v[1],
            math.sin(t) * v[0] + math.cos(t) * v[1], v[2]]

def dot(a, b):
    return sum(p * q for p, q in zip(a, b))

def unit(v):
    n = math.sqrt(dot(v, v))
    return [p / n for p in v]

def fmt(v):
    return " ".join("%.6f" % p for p in v)

def plane_axes():
    # the plane's axes in world coordinates, from status: the plane's own
    # rotation turned by the XY rotation it was defined under
    s.poll()
    r = s.g68_rotation
    return [rot_z(s.rotation_xy, column(r, k)) for k in range(3)]

def tool_axes():
    # the tool frame's axes in world coordinates, from status
    s.poll()
    t = s.tool_frame
    if t is None:
        error("no tool frame in status")
        return [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
    return [column(t, k) for k in range(3)]

def jog_incr(axis, dist, speed, want, what):
    # an incremental jog of a world letter, and the world move it must make
    s.poll()
    before = list(s.position)
    c.jog(linuxcnc.JOG_INCREMENT, 0, axis, speed if dist > 0 else -speed, abs(dist))
    after = settled()
    moved = [after[i] - before[i] for i in range(3)]
    print("%s: moved %s, want %s" % (what, fmt(moved), fmt(want)))
    if max(abs(m - w) for m, w in zip(moved, want)) > 1e-5:
        error("%s moved %s, not %s" % (what, fmt(moved), fmt(want)))
    for a in (B, C):
        if abs(after[a] - before[a]) > 1e-9:
            error("%s turned a rotary from %.6f to %.6f" % (what, before[a], after[a]))
    drain()
    return after

def cruise(axes, speed, what):
    # continuous jogs of the given letters at once, read in the cruise
    s.poll()
    before = list(s.position)
    for a in axes:
        c.jog(linuxcnc.JOG_CONTINUOUS, 0, a, speed)
    time.sleep(0.4)
    vel = [hal.get_value("axis.%s.teleop-vel-cmd" % l) for l in "xyz"]
    for a in axes:
        c.jog(linuxcnc.JOG_STOP, 0, a)
    after = settled()
    moved = [after[i] - before[i] for i in range(3)]
    print("%s: velocity %s, moved %s" % (what, fmt(vel), fmt(moved)))
    drain()
    return vel, moved

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete(30)
c.home(-1)
c.wait_complete(60)
settled()
drain()

s.poll()
if s.jog_frame != linuxcnc.JOG_FRAME_MACHINE:
    error("the jog frame starts at %d, not the machine's" % s.jog_frame)

# the plane, defined under an XY rotation of 30 degrees: its axes turned by
# both are what the jog must follow
mdi("G12.1 P0", "G10 L2 P1 R30", "G54", "G68.2 X0 Y0 Z0 I0 J30 K0")
W = plane_axes()
print("plane axes in world: X %s, Y %s, Z %s" % tuple(fmt(v) for v in W))
teleop()
frame(linuxcnc.JOG_FRAME_PLANE)
jog_incr(X, 10, 100, [10 * p for p in W[0]], "plane X +10")
jog_incr(Y, -5, 40, [-5 * p for p in W[1]], "plane Y -5")
jog_incr(Z, 3, 100, [3 * p for p in W[2]], "plane Z +3")

# the world planners hold where the machine is: a machine jog after the
# frame jogs starts from there
frame(linuxcnc.JOG_FRAME_MACHINE)
jog_incr(X, 2, 100, [2, 0, 0], "machine X +2 after the plane jogs")

# a plane whose X runs mostly along the slow Y: the jog along it goes no
# faster than Y allows, and two frame axes jogged at once keep every world
# axis inside its own speed
mdi("G69", "G10 L2 P1 R0", "G54", "G68.2 X0 Y0 Z0 I60 J0 K0")
W = plane_axes()
teleop()
frame(linuxcnc.JOG_FRAME_PLANE)
vel, moved = cruise([X], 200, "plane X at 200")
if abs(abs(vel[Y]) - VEL_Y) > 1e-6 or abs(vel[X] - VEL_Y * W[0][0] / W[0][1]) > 1e-6:
    error("plane X ran at %s, not at the speed Y allows" % fmt(vel))
if abs(dot(unit(moved), W[0]) - 1) > 1e-9:
    error("plane X moved along %s, not %s" % (fmt(unit(moved)), fmt(W[0])))

# a jog of plane Y asked for while plane X runs waits until X has stopped,
# then runs on its own
s.poll()
before = list(s.position)
c.jog(linuxcnc.JOG_CONTINUOUS, 0, X, 200)
time.sleep(0.3)
c.jog(linuxcnc.JOG_CONTINUOUS, 0, Y, 200)
time.sleep(0.3)
vel = [hal.get_value("axis.%s.teleop-vel-cmd" % l) for l in "xyz"]
print("plane X with plane Y waiting: velocity %s" % fmt(vel))
if abs(dot(unit(vel), W[0]) - 1) > 1e-9:
    error("plane X with plane Y asked for ran along %s, not %s" % (fmt(unit(vel)), fmt(W[0])))
c.jog(linuxcnc.JOG_STOP, 0, X)
time.sleep(0.4)
vel = [hal.get_value("axis.%s.teleop-vel-cmd" % l) for l in "xyz"]
print("plane Y after plane X stopped: velocity %s" % fmt(vel))
if abs(dot(unit(vel), W[1]) - 1) > 1e-9 or abs(abs(vel[Y]) - VEL_Y) > 1e-6:
    error("plane Y after plane X ran at %s, not along %s at the speed Y allows" % (fmt(vel), fmt(W[1])))
c.jog(linuxcnc.JOG_STOP, 0, Y)
settled()
drain()

# the jog stops where the turned axis meets the box, without tripping a
# soft limit, and goes no further out from there; plane Y asked for at
# once waits, and from the box has nowhere to go
s.poll()
before = list(s.position)
c.jog(linuxcnc.JOG_CONTINUOUS, 0, X, 200)
c.jog(linuxcnc.JOG_CONTINUOUS, 0, Y, 200)
time.sleep(0.2)
at_box = settled()
c.jog(linuxcnc.JOG_STOP, 0, X)
c.jog(linuxcnc.JOG_STOP, 0, Y)
print("ran into the box at %s" % fmt(at_box[:3]))
s.poll()
if not 99.9 < at_box[Y] < 100 or s.task_state != linuxcnc.STATE_ON:
    error("the jog along plane X ended at Y %.6f, task state %d" % (at_box[Y], s.task_state))
moved = [at_box[i] - before[i] for i in range(3)]
if abs(dot(unit(moved), W[0]) - 1) > 1e-9:
    error("the jog to the box went along %s, not %s" % (fmt(unit(moved)), fmt(W[0])))
drain()
jog_incr(X, 1, 100, [0, 0, 0], "plane X +1 out of the box")
jog_incr(X, -1, 100, [-p for p in W[0]], "plane X -1 back in")

# the frame stays while a jog is under way
c.jog(linuxcnc.JOG_CONTINUOUS, 0, Y, -20)
time.sleep(0.2)
c.jog_frame(linuxcnc.JOG_FRAME_MACHINE)
c.wait_complete(30)
time.sleep(0.1)
said = drain("cannot change while jogging")
s.poll()
if s.jog_frame != linuxcnc.JOG_FRAME_PLANE:
    error("the jog frame changed to %d while jogging" % s.jog_frame)
if not any("cannot change while jogging" in m for m in said):
    error("no word of the refused change")
c.jog(linuxcnc.JOG_STOP, 0, Y)
settled()
drain()

# the tool frame, the head laid over and turned: X, Y and Z of the tool as
# the kinematics reports it.  The carriage goes where the tip, swung out
# by the head, stays inside the short Y travel
mdi("G69", "G53.7 G0 J0=0 J1=-40 J2=0 J3=30 J4=45")
teleop()
frame(linuxcnc.JOG_FRAME_TOOL)
T = tool_axes()
print("tool axes in world: X %s, Y %s, Z %s" % tuple(fmt(v) for v in T))
jog_incr(Z, 5, 100, [5 * p for p in T[2]], "tool Z +5")
jog_incr(X, 5, 100, [5 * p for p in T[0]], "tool X +5")
jog_incr(Y, -4, 40, [-4 * p for p in T[1]], "tool Y -4")

# a plane in effect leaves the tool frame alone: the jogs still follow the
# frame the kinematics reports, not the plane's axes
before = tool_axes()
mdi("G68.2 X0 Y0 Z0 I60 J0 K0")
W = plane_axes()
teleop()
T = tool_axes()
print("tool axes under the plane: X %s, Y %s, Z %s" % tuple(fmt(v) for v in T))
if any(abs(p - q) > 1e-9 for a, b in zip(T, before) for p, q in zip(a, b)):
    error("the plane changed the tool frame in status")
if abs(dot(T[0], W[0])) > 1 - 1e-6:
    error("the plane's X lies along the tool's X, which tells the frames apart nowhere")
jog_incr(X, 5, 100, [5 * p for p in T[0]], "tool X +5 under the plane")
jog_incr(Y, 4, 40, [4 * p for p in T[1]], "tool Y +4 under the plane")
jog_incr(Z, -5, 100, [-5 * p for p in T[2]], "tool Z -5 under the plane")

# under the identity type the kinematics turns no frame: the tool frame is
# the machine's
mdi("G69", "G12.1 P1")
teleop()
s.poll()
if s.tool_frame is not None:
    error("a tool frame under the identity type: %s" % (s.tool_frame,))
jog_incr(Z, 2, 100, [0, 0, 2], "tool Z +2 under the identity type")
jog_incr(X, 2, 100, [2, 0, 0], "tool X +2 under the identity type")

# halui selects the frame and says which is in force
for name, which in (("plane", linuxcnc.JOG_FRAME_PLANE), ("machine", linuxcnc.JOG_FRAME_MACHINE),
                    ("tool", linuxcnc.JOG_FRAME_TOOL)):
    hal.set_p("halui.jog-frame.%s" % name, "1")
    deadline = time.time() + 5
    while time.time() < deadline:
        s.poll()
        if s.jog_frame == which:
            break
        time.sleep(0.05)
    hal.set_p("halui.jog-frame.%s" % name, "0")
    time.sleep(0.2)
    shown = [n for n in ("machine", "plane", "tool") if hal.get_value("halui.jog-frame.is-%s" % n)]
    if s.jog_frame != which or shown != [name]:
        error("halui.jog-frame.%s gave frame %d, halui shows %s" % (name, s.jog_frame, shown))
drain()

# every servo cycle of every jog above: no world axis over its own speed or
# acceleration, a frame jog's stop at the box included
time.sleep(0.5)
with open(LOG) as f:
    lines = f.read().split("\n")
samples = []
for line in lines[:-1]:
    v = line.split()
    if len(v) == 4:
        samples.append([float(x) for x in v[1:]])
print("%d samples" % len(samples))
for i, l in enumerate("XYZ"):
    vpeak = max(abs(v[i]) for v in samples) if samples else 0.0
    apeak = max((abs(samples[k][i] - samples[k - 1][i]) / SERVO
                 for k in range(1, len(samples))), default=0.0)
    print("%s: velocity peak %.3f of %.3f, acceleration peak %.2f of %.2f"
          % (l, vpeak, AXIS_VEL[i], apeak, AXIS_ACC[i]))
    if vpeak > AXIS_VEL[i] * 1.001:
        error("%s ran at %.3f, over its limit of %.3f" % (l, vpeak, AXIS_VEL[i]))
    if apeak > AXIS_ACC[i] * 1.001:
        error("%s accelerated at %.2f, over its limit of %.2f" % (l, apeak, AXIS_ACC[i]))
if not samples:
    error("no samples")
overruns = int(hal.get_value("sampler.0.overruns"))
if overruns:
    error("the sampler lost %d samples" % overruns)
if not errors:
    os.unlink(LOG)

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
