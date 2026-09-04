#!/usr/bin/env python3
from math import atan2, degrees, hypot

def create_unitcode_and_initcode(s, inifile):
    a_axis_wrapped = inifile.getbool("AXIS_A", "WRAPPED_ROTARY", fallback=False)
    b_axis_wrapped = inifile.getbool("AXIS_B", "WRAPPED_ROTARY", fallback=False)
    c_axis_wrapped = inifile.getbool("AXIS_C", "WRAPPED_ROTARY", fallback=False)

    s.poll()
    # create unitcode and initcode reflecting currently active modal gcodes
    unitcode = "G%d" % (20 + (s.linear_units == 1))
    initcode = "G53 G0 "
    for i in range(9):
        if s.axis_mask & (1<<i):
            axis = "XYZABCUVW"[i]
            if (axis == "A" and a_axis_wrapped) or\
               (axis == "B" and b_axis_wrapped) or\
               (axis == "C" and c_axis_wrapped):
                pos = s.position[i] % 360.000
            else:
                pos = s.position[i]
            position = "%s%.8f " % (axis, pos)
            initcode += position
    active_gcodes = s.gcodes
    # 12 is the tilted work plane, restated with its words in unitcode
    for i in (3,6,14,7,5,4,9,10,16,8,11,13,15):
        if active_gcodes[i] > -1:
            initcode = initcode + 'G' + str(active_gcodes[i]/10) + ' '
    plane = workplane_code(s)
    if plane:
        unitcode = unitcode + ' ' + plane
    return unitcode, initcode

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
