#!/usr/bin/env python3

# The slots of stat.gcodes and stat.mcodes that hold a mode. The others hold
# the sequence number (issue #271) or the last block's one-shot codes: G10
# (issue #269), M2, which blanks the next preview, and M6 or M61, which has
# no Q to restate. G20/G21 (gcodes[5]) and the tool length offset
# (gcodes[9]) are restated with their values instead.
MODAL_GCODES = (3, 6, 14, 7, 4, 10, 16, 8, 11, 13, 15)
MODAL_MCODES = (2, 4, 5, 6, 7, 8, 9)

def preview_initcodes(s, inifile):
    """The machine's state as G-code for a preview interpreter to start
    from: the startup code, the units, the tool in the spindle, the
    position, the active modes and the tool length offset in force."""
    wrapped = {a: inifile.getbool("AXIS_" + a, "WRAPPED_ROTARY", fallback=False)
               for a in "ABC"}
    s.poll()
    codes = []
    startup = inifile.getstring("EMC", "RS274NGC_STARTUP_CODE", fallback="") or \
        inifile.getstring("RS274NGC", "RS274NGC_STARTUP_CODE", fallback="")
    if startup:
        codes.append(startup)
    codes.append("G%d" % (20 + (s.linear_units == 1)))
    codes.append("G90")
    codes.append("T%d M6" % s.tool_in_spindle)
    axes = [(i, "XYZABCUVW"[i]) for i in range(9) if s.axis_mask & (1 << i)]
    position = "G53 G0"
    for i, axis in axes:
        pos = s.position[i] % 360.0 if wrapped.get(axis) else s.position[i]
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
