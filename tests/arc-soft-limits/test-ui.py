#!/usr/bin/env python3
"""Reject circular moves that exceed soft limits before motion, including in MDI (issue #3839)."""

import math
import time

import linuxcnc
import linuxcnc_util


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()
machine = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)


def errors():
    messages = []
    while True:
        error = e.poll()
        if error is None:
            return messages
        messages.append(error[1])


def drain_after_abort():
    # Motion errors are forwarded one per task cycle.  Wait for the trailing
    # diagnostics before starting another case, whose error channel must be empty.
    quiet_until = time.monotonic() + 0.1
    deadline = time.monotonic() + 2
    while time.monotonic() < deadline:
        if errors():
            quiet_until = time.monotonic() + 0.1
        elif time.monotonic() >= quiet_until:
            return
        time.sleep(0.01)
    raise AssertionError("error channel did not settle after abort")


def mdi(command):
    c.mdi(command)
    result = c.wait_complete(10)
    messages = errors()
    assert result == linuxcnc.RCS_DONE, (command, result, messages)
    assert not messages, (command, messages)


def rejected(command, axis, direction):
    expected = "would exceed {}'s {} limit".format(axis, direction)
    s.poll()
    start = s.position[:3]
    assert not errors()
    c.mdi(command)
    result = c.wait_complete(5)
    # Error-channel delivery and the status channel are asynchronous.
    messages = []
    deadline = time.monotonic() + 1
    while time.monotonic() < deadline:
        messages.extend(errors())
        if any("Circular move" in message and expected in message
               for message in messages):
            break
        time.sleep(0.01)
    s.poll()
    position = s.position[:3]
    diagnostic = (command, result, start, position, messages)
    assert all(abs(a - b) < 1e-8 for a, b in zip(start, position)), diagnostic
    assert any("Circular move" in message and expected in message
               for message in messages), diagnostic
    print("Rejected before motion: " + command, flush=True)
    c.abort()
    c.wait_complete(5)
    drain_after_abort()


def origin():
    mdi("G17 G90 G10 L2 P1 X0 Y0 Z0 R0")
    mdi("G54 G0 X0 Y0 Z0")


def queued_program(filename, reject=False):
    # Hold the machine at the origin while both commands reach motion.
    # The circle must be checked from the queued line's endpoint.
    origin()
    c.feedrate(0)
    c.mode(linuxcnc.MODE_AUTO)
    c.wait_complete(5)
    c.program_open(filename)
    c.auto(linuxcnc.AUTO_RUN, 0)
    deadline = time.monotonic() + 5
    messages = []
    expected = "would exceed X's negative limit"
    while time.monotonic() < deadline:
        messages.extend(errors())
        s.poll()
        if reject and any("Circular move" in message and expected in message
                          for message in messages):
            break
        if not reject and (messages or s.queue >= 2):
            break
        time.sleep(0.01)
    assert all(abs(p) < 1e-8 for p in s.position[:3]), s.position
    if reject:
        assert any("Circular move" in message and expected in message
                   for message in messages), (filename, s.queue, messages)
        c.abort()
        c.wait_complete(5)
        drain_after_abort()
    else:
        assert not messages, (filename, messages)
        assert s.queue >= 2, (filename, s.queue)
    c.feedrate(1)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        s.poll()
        if s.interp_state == linuxcnc.INTERP_IDLE and s.inpos:
            break
        time.sleep(0.01)
    assert s.interp_state == linuxcnc.INTERP_IDLE and s.inpos, filename
    if not reject:
        assert abs(s.position[0] + 8) < 1e-7, s.position
        assert not errors()
    c.mode(linuxcnc.MODE_MDI)
    c.wait_complete(5)
    errors()
    print("Queued start checked: " + filename, flush=True)


machine.wait_for_linuxcnc_startup()
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MANUAL)
c.wait_complete(5)
c.home(-1)
machine.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(5)
mdi("G21 G90 G17 G40 G49 G54 G61 G94 F2000")

# The reported reproduction: the end point is the start point.
rejected("G2 I1000 F2000", "X", "positive")

# Both directions, all planes, positive and negative limits.  All endpoints
# are inside the limits and would pass the old endpoint-only check.
for command, axis, direction in [
        ("G17 G2 I6", "X", "positive"),
        ("G17 G3 I-6", "X", "negative"),
        ("G17 G2 J6", "Y", "positive"),
        ("G17 G3 J-6", "Y", "negative"),
        ("G18 G2 K6", "Z", "positive"),
        ("G18 G3 K-6", "Z", "negative"),
        ("G19 G2 J6", "Y", "positive"),
        ("G19 G3 K-6", "Z", "negative"),
        ("G17 G2 I6 P3 Z2", "X", "positive")]:
    rejected(command, axis, direction)

origin()
# Tangency is allowed.  Also check a multi-turn helix that stays in bounds.
mdi("G2 I5")
mdi("G3 I5")
mdi("G2 I4 P3 Z2")
origin()
# A short part of an enormous circle is legal; checking the entire circle's
# bounding box would incorrectly reject it.
mdi("G2 X{:.12f} Y5 I1000".format(1000 - math.sqrt(1000**2 - 5**2)))

# Partial arcs with legal endpoints but an interior extremum out of range.
mdi("G0 X9 Y-2")
rejected("G3 X9 Y2 I-1 J2", "X", "positive")
mdi("G0 X-9 Y-2")
rejected("G2 X-9 Y2 I1 J2", "X", "negative")

# A small radius mismatch is allowed by the interpreter, producing a spiral.
origin()
rejected("G2 X-0.01 Y0 I5", "X", "positive")
mdi("G2 X0.01 Y0 I5")

# G10 rotation tilts the G18 plane in machine coordinates.  The second arc
# is a helix whose maximum Y is between the usual circle quadrant angles.
origin()
mdi("G10 L2 P1 R45")
mdi("G18 G2 I6")
rejected("G18 G2 I6 Y6", "Y", "positive")
mdi("G18 G2 I4 Y2")

queued_program("queued-valid.ngc")
queued_program("queued-invalid.ngc", reject=True)

print("Arc soft-limit checks passed", flush=True)
