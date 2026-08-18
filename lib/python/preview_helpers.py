#!/usr/bin/env python3

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
    for i in (3,6,14,7,5,4,9,12,10,16,8,11,13,15):
        if active_gcodes[i] > -1:
            initcode = initcode + 'G' + str(active_gcodes[i]/10) + ' '
    return unitcode, initcode
