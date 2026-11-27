#!/usr/bin/env python3
from math import atan2, degrees, hypot

import axis_kinds

# The slots of stat.gcodes and stat.mcodes that hold a mode. The others hold
# the sequence number (issue #271) or the last block's one-shot codes: G10
# (issue #269), M2, which blanks the next preview, and M6 or M61, which has
# no Q to restate. Cutter radius compensation (gcodes[4]) is left out, because
# the blocks after it would be refused while it is on. The tool length offset
# (gcodes[9]), the tilted work plane (gcodes[12]) and the G92 offsets
# (gcodes[16]) are restated with their values instead.
MODAL_GCODES = (3, 6, 14, 7, 5, 10, 8, 11, 13, 15)
MODAL_MCODES = (2, 4, 5, 6, 7, 8, 9)

def preview_initcodes(s, inifile):
    """The machine's state as G-code for a preview interpreter to start
    from: the startup code, the units, the G92 offsets, the tilted work
    plane, the tool in the spindle, the position, the active modes and the
    tool length offset in force."""
    wrapped = axis_kinds.wrapped(inifile)
    s.poll()
    codes = []
    startup = inifile.getstring("EMC", "RS274NGC_STARTUP_CODE", fallback="") or \
        inifile.getstring("RS274NGC", "RS274NGC_STARTUP_CODE", fallback="")
    if startup:
        codes.append(startup)
    codes.append("G%d" % (20 + (s.linear_units == 1)))
    axes = [(i, "XYZABCUVW"[i]) for i in range(9) if s.axis_mask & (1 << i)]
    # The G92 offsets as G52 sets them, by value: the startup code may clear
    # them, and G92.3 would take them from the preview's parameters.  They
    # come in machine units, ahead of the plane, which sits inside them and
    # refuses a change to them
    if any(s.g92_offset[i] for i, axis in axes):
        codes.append("G52" + "".join(" %s%.12f" % (axis, s.g92_offset[i]) for i, axis in axes))
    plane = workplane_code(s)
    if plane:
        codes.append(plane)
    codes.append("G90")
    codes.append("T%d M6" % s.tool_in_spindle)
    offset = "G43.1" + "".join(" %s%.8f" % (axis, s.tool_offset[i]) for i, axis in axes)
    # Under G43.4 or G43.5 the switch and the offset come ahead of the
    # position: the switch holds the joints and would move a point restated
    # before it. The bare code takes the loaded tool's length, and the G43.1
    # after it sets the machine's values and keeps the switch
    tcp = s.gcodes[9] in (434, 435)
    if tcp:
        codes.append("G%.1f" % (s.gcodes[9] * .1))
        codes.append(offset)
    position = "G53 G0"
    for i, axis in axes:
        pos = s.position[i] % 360.0 if wrapped[i] else s.position[i]
        position += " %s%.8f" % (axis, pos)
    codes.append(position)
    for i in MODAL_GCODES:
        g = s.gcodes[i]
        if g == -1:
            continue
        if g == 960: # Issue #1232
            codes.append("G96 S%.0f" % s.settings[2])
        else:
            codes.append("G%.1f" % (g * .1))
    # The offset itself, which a bare G43 would take from the preview's tool
    # table rather than the machine's
    if s.gcodes[9] == 490:
        codes.append("G49")
    elif not tcp:
        codes.append(offset)
    for i in MODAL_MCODES:
        if s.mcodes[i] != -1:
            codes.append("M%d" % s.mcodes[i])
    return codes

def workplane_code(s):
    """The tilted work plane in effect on the machine as one G68.2 block,
    or None.  The bare code in s.gcodes cannot restate it: G68.2 alone is
    a plane at the origin and G68.4 alone has nothing to build on.  The
    words are in machine units, so the block goes where the machine's G20
    or G21 is in force; a later unit change converts the plane.  The block
    selects the machine's coordinate system first, since the plane sits on
    it and a later block may reselect it but not change it, and the tool
    length offset, since under the machine frame kinematics the plane is
    cut through the machine's second feature coordinate system, which the
    interpreter takes from status and which comes after the tool."""
    if not getattr(s, "g68_active", 0):
        return None
    r = s.g68_rotation
    o = s.g68_offset
    # G68.2 P1 turns about X by I, then Y by J, then Z by K: Rz Ry Rx
    c = hypot(r[0], r[3])
    j = atan2(-r[6], c)
    if c > 1e-12:
        i = atan2(r[7], r[8])
        k = atan2(r[3], r[0])
    else:
        i = 0.0
        k = atan2(-r[1], r[4])
    system = next((g for g in s.gcodes[1:] if 540 <= g <= 593), 540)
    length = "G43 " if s.gcodes[9] == 430 else ""
    return "%sG%s G68.2 P1 X%.12f Y%.12f Z%.12f I%.12f J%.12f K%.12f" % (
        length, system // 10 if system % 10 == 0 else system / 10,
        o[0], o[1], o[2], degrees(i), degrees(j), degrees(k))
