#!/usr/bin/env python3
#
# Checks stat.spindle_sync_mode, _pitch and _index_pulse_angle_offset, and
# positionlogger.sync_points()
# while a program runs one move of each kind of spindle synch: none (G94 G1),
# velocity (G95 G1), position (G33 with D, G76 with D and both tapers).
#
# stat is polled every few milliseconds and each sample filed under the
# motion line it was taken on; two position loggers run at the same time,
# one made with sync=True and one without.

import linuxcnc
import linuxcnc_util

import os
import struct
import sys
import threading
import time

INTERPTIMEOUT = 5    # Max seconds to wait for the interpreter to start
GCODETIMEOUT = 120   # Max seconds for the gcode program to run

# line numbers in sync.ngc
L_G94, L_G95, L_G33, L_G76 = 6, 7, 9, 11

failures = []


def check(what, cond, detail=""):
    print(("ok   " if cond else "FAIL ") + what
          + (": " + str(detail) if detail and not cond else ""))
    if not cond:
        failures.append(what)


def close(a, b, tol=1e-6):
    return abs(a - b) < tol


ngcfile = None
for i in range(len(sys.argv) - 1):
    if sys.argv[i] == "-ngc":
        ngcfile = sys.argv[i + 1]
        break
if not ngcfile or not os.path.exists(ngcfile):
    print("Missing NGC-file; run with: test-ui.py -ngc ngcfile.ngc")
    sys.exit(1)

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)

c.home(-1)
c.wait_complete()
l.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])

s.poll()
check("constants: SPINDLE_SYNC_NONE/_VELOCITY/_POSITION are 0, 1, 2",
      (linuxcnc.SPINDLE_SYNC_NONE, linuxcnc.SPINDLE_SYNC_VELOCITY,
       linuxcnc.SPINDLE_SYNC_POSITION) == (0, 1, 2))
check("idle: no synch",
      (s.spindle_sync_mode, s.spindle_sync_pitch,
       s.spindle_sync_index_pulse_angle_offset) == (0, 0.0, 0.0))

COLORS = [(255, 255, 255, 255)] * 6
logger = linuxcnc.positionlogger(linuxcnc.stat(), *COLORS, "XYZ", sync=True)
plain = linuxcnc.positionlogger(linuxcnc.stat(), *COLORS, "XYZ")
try:
    linuxcnc.positionlogger(linuxcnc.stat(), *COLORS, "XYZ", bogus=1)
    check("positionlogger rejects an unknown keyword", False)
except TypeError:
    check("positionlogger rejects an unknown keyword", True)
threads = [threading.Thread(target=lg.start, args=(0.002,))
           for lg in (logger, plain)]
for t in threads:
    t.start()

c.mode(linuxcnc.MODE_AUTO)
c.program_open(ngcfile)
c.auto(linuxcnc.AUTO_RUN, 1)

start = time.time()
while time.time() - start < INTERPTIMEOUT:
    s.poll()
    if s.interp_state != linuxcnc.INTERP_IDLE:
        break
    time.sleep(0.01)
if s.interp_state == linuxcnc.INTERP_IDLE:
    print("Timed out starting interpreter")
    sys.exit(1)

# motion line -> set of (mode, pitch, index pulse angle offset) seen on it
seen = {}
start = time.time()
s.poll()
while s.interp_state != linuxcnc.INTERP_IDLE and time.time() - start < GCODETIMEOUT:
    seen.setdefault(s.motion_line, set()).add(
        (s.spindle_sync_mode, round(s.spindle_sync_pitch, 6),
         round(s.spindle_sync_index_pulse_angle_offset, 6)))
    time.sleep(0.002)
    s.poll()
if s.interp_state != linuxcnc.INTERP_IDLE:
    print("Timed out running the GCode program")
    sys.exit(1)

for lg in (logger, plain):
    lg.stop()
for t in threads:
    t.join()

NONE = linuxcnc.SPINDLE_SYNC_NONE
VEL = linuxcnc.SPINDLE_SYNC_VELOCITY
POS = linuxcnc.SPINDLE_SYNC_POSITION

# --- stat ----------------------------------------------------------------

check("G94 G1: never synched", seen.get(L_G94) == {(NONE, 0.0, 0.0)},
      seen.get(L_G94))
g95 = seen.get(L_G95, set())
check("G95 G1 F0.2: velocity synch at 0.2 mm/rev, angle offset 0",
      (VEL, 0.2, 0.0) in g95 and not any(m == POS for m, _, _ in g95), g95)
g33 = seen.get(L_G33, set())
check("G33 K1.5 D90: position synch at 1.5 mm/rev, 90 degrees",
      (POS, 1.5, 90.0) in g33 and not any(m == VEL for m, _, _ in g33), g33)
g76 = {x for x in seen.get(L_G76, set()) if x[0] == POS}
check("G76 P2 D45: position synch at 2 mm/rev and 45 degrees on the thread",
      (POS, 2.0, 45.0) in g76, g76)
check("... D45 on the entry taper, 0 on the exit taper, as the interpreter "
      "passes them", {a for _, _, a in g76} == {45.0, 0.0}, g76)
check("... no velocity synch in a G76",
      not any(x[0] == VEL for x in seen.get(L_G76, set())), seen.get(L_G76))
s.poll()
check("program end: no synch",
      (s.spindle_sync_mode, s.spindle_sync_pitch,
       s.spindle_sync_index_pulse_angle_offset) == (0, 0.0, 0.0))

# --- position logger -----------------------------------------------------

check("a logger made without sync=True has no sync_points",
      plain.sync_points() is None)
pbuf, npts, _ = logger.points()
sbuf, snpts = logger.sync_points()
check("sync_points has one 12 byte entry per point",
      snpts == npts and len(sbuf) == 12 * npts and npts > 0, (snpts, npts, len(sbuf)))
check("points() keeps its 32 byte layout", len(pbuf) == 32 * npts, len(pbuf))
syncs = [struct.unpack_from("=B3xff", sbuf, 12 * i) for i in range(snpts)]
pts = [struct.unpack_from("=fff", pbuf, 32 * i) for i in range(npts)]

check("the logged velocity synch is G95's 0.2 mm/rev",
      any(m == VEL for m, _, _ in syncs)
      and all(close(p, 0.2) and a == 0.0 for m, p, a in syncs if m == VEL),
      {x for x in syncs if x[0] == VEL})
pos = {(round(p, 4), round(a, 4)) for m, p, a in syncs if m == POS}
check("the logged position synchs are the G33's and the G76's",
      (1.5, 90.0) in pos and (2.0, 45.0) in pos, pos)
check("no synch is logged with a pitch or angle offset", all(
    p == 0.0 and a == 0.0 for m, p, a in syncs if m == NONE))

# Every synch start has a point of its own. An entry describes the line that
# ends at its point, so the G33's first entry is the line from where the G33
# starts, X10 Z5 (the G0 before it ends there), to where it ends, Z-10.
starts = [i for i in range(1, snpts) if syncs[i] != syncs[i - 1]]
g33start = [i for i in starts if syncs[i][0] == POS and close(syncs[i][1], 1.5, 1e-4)]
check("the G33 starts a point of its own", len(g33start) == 1, g33start)
if g33start:
    i = g33start[0]
    x0, _, z0 = pts[i - 1]
    x1, _, z1 = pts[i]
    check("... its line runs from X10 Z5 to X10 Z-10",
          close(x0, 10, 0.05) and close(z0, 5, 0.05)
          and close(x1, 10, 0.05) and close(z1, -10, 0.05),
          (pts[i - 1], pts[i]))
# G76 L3: each pass starts with the entry taper, so each pass is a synch
# start; with E and L3 there are at least two passes in this cycle.
g76starts = [i for i in starts if syncs[i][0] == POS and syncs[i - 1][0] == NONE
             and not close(syncs[i][1], 1.5, 1e-4)]
check("each G76 pass starts a point of its own", len(g76starts) >= 2,
      len(g76starts))

_, pnpts, _ = plain.points()
check("the logger without sync has no more points than the one with it",
      0 < pnpts <= npts, (pnpts, npts))

if failures:
    print("%d check(s) failed" % len(failures))
    sys.exit(1)
sys.exit(0)
