#!/usr/bin/env python3
from math import atan2, degrees, hypot

import axis_kinds

# The slots of stat.gcodes and stat.mcodes that hold a mode. The others hold
# the sequence number (issue #271) or the last block's one-shot codes: G10
# (issue #269), M2, which blanks the next preview, and M6 or M61, which has
# no Q to restate. The tool length offset (gcodes[9]) and the tilted work
# plane (gcodes[12]) are restated with their values instead.
MODAL_GCODES = (3, 6, 14, 7, 5, 4, 10, 16, 8, 11, 13, 15)
MODAL_MCODES = (2, 4, 5, 6, 7, 8, 9)

def preview_initcodes(s, inifile):
    """The machine's state as G-code for a preview interpreter to start
    from: the startup code, the units, the tilted work plane, the tool
    in the spindle, the position, the active modes and the tool length
    offset in force."""
    wrapped = axis_kinds.wrapped(inifile)
    s.poll()
    codes = []
    startup = inifile.getstring("EMC", "RS274NGC_STARTUP_CODE", fallback="") or \
        inifile.getstring("RS274NGC", "RS274NGC_STARTUP_CODE", fallback="")
    if startup:
        codes.append(startup)
    codes.append("G%d" % (20 + (s.linear_units == 1)))
    plane = workplane_code(s)
    if plane:
        codes.append(plane)
    codes.append("G90")
    codes.append("T%d M6" % s.tool_in_spindle)
    axes = [(i, "XYZABCUVW"[i]) for i in range(9) if s.axis_mask & (1 << i)]
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
    else:
        codes.append("G43.1" + "".join(" %s%.8f" % (axis, s.tool_offset[i])
                                        for i, axis in axes))
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
    it and a later block may reselect it but not change it."""
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
    return "G%s G68.2 P1 X%.12f Y%.12f Z%.12f I%.12f J%.12f K%.12f" % (
        system // 10 if system % 10 == 0 else system / 10, o[0], o[1], o[2], degrees(i), degrees(j), degrees(k))
