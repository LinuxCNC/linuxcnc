#!/usr/bin/env python3
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.

# Which axes are angles, per [AXIS_<letter>] TYPE (default A B C): the
# Python side of src/emc/ini/axis_kinds.hh.  inifile is anything with
# find(section, variable), a linuxcnc.ini among them.

LETTERS = "XYZABCUVW"

def angular(inifile):
    """A list of nine bools, X first: True where the axis is an angle."""
    kinds = [letter in "ABC" for letter in LETTERS]
    if inifile is None:
        return kinds
    for i, letter in enumerate(LETTERS):
        value = inifile.find("AXIS_" + letter, "TYPE")
        if value is None:
            continue
        value = value.strip().upper()
        if value == "ANGULAR":
            kinds[i] = True
        elif value == "LINEAR":
            kinds[i] = False
    return kinds

def wrapped(inifile):
    """A list of nine bools, X first: True where the axis is an angle and
    [AXIS_<letter>] WRAPPED_ROTARY is set, as the interpreter reads it."""
    kinds = angular(inifile)
    result = [False] * 9
    if inifile is None:
        return result
    for i, letter in enumerate(LETTERS):
        value = inifile.find("AXIS_" + letter, "WRAPPED_ROTARY")
        if kinds[i] and value is not None:
            result[i] = value.strip().upper() in ("1", "TRUE", "YES", "ON")
    return result

def unit_factors(inifile, factor, count=9):
    """A list of count factors, X first: factor for a length, 1 for an
    angle.  A tenth entry, the rotation column of an offset table, is an
    angle."""
    kinds = angular(inifile) + [True] * (count - 9)
    return [1 if kinds[i] else factor for i in range(count)]
