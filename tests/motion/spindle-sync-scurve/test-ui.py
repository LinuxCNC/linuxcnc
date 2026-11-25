#!/usr/bin/env python3
#
# Runs sync.ngc with PLANNER_TYPE = 1 and samples Z against the spindle, see README.

import linuxcnc
import linuxcnc_util
import hal

import os
import sys
import time

GCODETIMEOUT = 120   # Max seconds for the gcode program to run

PITCH = 1.5          # K word of the synchronized moves, mm/rev
JERK = 30000.0       # [TRAJ]MAX_LINEAR_JERK, mm/s^3
DT = 0.0005          # [EMCMOT]SERVO_PERIOD, s

# line numbers in sync.ngc
L_G1, L_G33, L_TAP = 5, 7, 9

# sampler field order, see sample.hal
LINE, REVS, ZPOS = range(3)

VEL_RIPPLE_MAX = 1.0     # mm/s, Z velocity spread while tracking a steady spindle
TRACKING_MAX = 0.01      # mm, spread of Z against the spindle mid move
JERK_MARGIN = 3.0        # a trapezoidal G1 would exceed this by orders of magnitude

failures = []


def check(what, cond, detail=""):
    print(("ok   " if cond else "FAIL ") + what + (": " + str(detail) if detail else ""))
    if not cond:
        failures.append(what)


def moving_part(data, line):
    """The samples of a motion line, without the rest after Z stopped."""
    part = [d for d in data if d[LINE] == line]
    while len(part) > 1 and part[-1][ZPOS] == part[-2][ZPOS]:
        part.pop()
    while len(part) > 1 and part[0][ZPOS] == part[1][ZPOS]:
        part.pop(0)
    return part


def velocity_ripple(part):
    """Spread of Z velocity in the middle of a move, averaged over 5 ms."""
    mid = part[len(part) // 4: -(len(part) // 4)]
    n = 10
    vel = [(mid[i + n][ZPOS] - mid[i][ZPOS]) / (n * DT) for i in range(len(mid) - n)]
    return max(vel) - min(vel)


def tracking_span(part):
    """Spread of Z against 'Z advances PITCH per revolution', mid move."""
    mid = part[len(part) // 4: -(len(part) // 4)]
    err = [m[ZPOS] + PITCH * m[REVS] for m in mid]
    return max(err) - min(err)


def max_jerk(part):
    z = [p[ZPOS] for p in part]
    return max(abs(z[i + 3] - 3 * z[i + 2] + 3 * z[i + 1] - z[i]) / DT ** 3
               for i in range(len(z) - 3))


ngcfile = None
for i in range(1, len(sys.argv) - 1):
    if sys.argv[i] == "-ngc":
        ngcfile = sys.argv[i + 1]
if not ngcfile or not os.path.exists(ngcfile):
    print("Missing NGC-file; run with: test-ui.py -ngc ngcfile.ngc")
    sys.exit(1)

h = hal.component("python-ui")
sampler = hal.stream(h, hal.sampler_base, "sff")
h.ready()

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.home(-1)
c.wait_complete()
l.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])

c.mode(linuxcnc.MODE_AUTO)
c.program_open(ngcfile)
hal.set_p("sampler.0.enable", "1")
c.auto(linuxcnc.AUTO_RUN, 1)

samples = []
messages = []


def drain():
    while sampler.readable:
        samples.append(sampler.read())
    err = e.poll()
    if err:
        messages.append(err[1])


# wait for the program to leave idle first: on a busy machine it may not
# have started yet, and the loop below would end at once with no samples
start = time.time()
while True:
    drain()
    s.poll()
    if s.interp_state != linuxcnc.INTERP_IDLE:
        break
    if time.time() - start > GCODETIMEOUT:
        print("Timeout waiting for the program to start")
        sys.exit(1)
    time.sleep(0.005)

while True:
    drain()
    s.poll()
    if s.interp_state == linuxcnc.INTERP_IDLE and s.queue == 0:
        break
    if time.time() - start > GCODETIMEOUT:
        print("Timeout waiting for the program to finish")
        sys.exit(1)
    time.sleep(0.005)
time.sleep(0.2)
drain()
hal.set_p("sampler.0.enable", "0")

overruns = hal.get_value("sampler.0.overruns")
print("{} samples, overruns {}".format(len(samples), overruns))
for m in messages[:5]:
    print("message: " + m.strip())

# a dropped sample is a jump in Z between two samples, which the checks
# below would read as velocity ripple or jerk
check("no sampler overruns", overruns == 0, "{} overruns".format(overruns))

check("no Ruckig limit errors", not any("invalid limits" in m for m in messages),
      "{} messages".format(len(messages)))
check("no other errors", all("invalid limits" in m or "realtime delay" in m for m in messages))

for name, line in (("G33", L_G33), ("G33.1", L_TAP)):
    part = moving_part(samples, line)
    if len(part) < 200:
        check(name + " ran", False, "{} samples".format(len(part)))
        continue
    # the line of a rigid tap also holds the reversal and the retract
    bottom = min(range(len(part)), key=lambda i: part[i][ZPOS])
    part = part[:bottom]
    ripple = velocity_ripple(part)
    check("{} Z velocity ripple < {} mm/s".format(name, VEL_RIPPLE_MAX),
          ripple < VEL_RIPPLE_MAX, "{:.3f} mm/s".format(ripple))
    span = tracking_span(part)
    check("{} Z tracks the spindle within {} mm".format(name, TRACKING_MAX),
          span < TRACKING_MAX, "{:.4f} mm".format(span))

part = moving_part(samples, L_G1)
if len(part) < 200:
    check("G1 ran", False, "{} samples".format(len(part)))
else:
    jerk = max_jerk(part)
    check("G1 is still jerk limited", jerk < JERK_MARGIN * JERK, "{:.0f} mm/s^3".format(jerk))

if failures:
    print("Test Failed")
    sys.exit(1)

print("Completed successfully")
sys.exit(0)
