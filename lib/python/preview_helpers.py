#!/usr/bin/env python3

import axis_kinds

def create_unitcode_and_initcode(s, inifile):
    wrapped = axis_kinds.wrapped(inifile)

    s.poll()
    # create unitcode and initcode reflecting currently active modal gcodes
    unitcode = "G%d" % (20 + (s.linear_units == 1))
    initcode = "G53 G0 "
    for i in range(9):
        if s.axis_mask & (1<<i):
            axis = "XYZABCUVW"[i]
            if wrapped[i]:
                pos = s.position[i] % 360.000
            else:
                pos = s.position[i]
            position = "%s%.8f " % (axis, pos)
            initcode += position
    active_gcodes = s.gcodes
    for i in (3,6,14,7,5,4,9,12,10,16,8,11,13,15):
        if active_gcodes[i] > -1:
            initcode = initcode + 'G' + str(active_gcodes[i]/10) + ' '
    return unitcode, initcode
