#!/usr/bin/env python3
# vim: sts=4 sw=4 et
"""
[DISPLAY]CYCLE_TIME handling shared by the GUIs and VCPs.

The value is in seconds: from 0.001 up to but not including 1.0. A value
of 1.0 or more is read as milliseconds. Anything else (zero, negative,
non-numeric) is reported and replaced by the default of 0.150 s. A
missing value silently uses the default.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""

import math
import sys

DEFAULT_MS = 150

_reported = set()


def parse_cycle_time(value):
    """Convert a raw [DISPLAY]CYCLE_TIME string to milliseconds.

    Returns (milliseconds, message). message is None unless the value
    is present but invalid, in which case milliseconds is DEFAULT_MS.
    """
    if value is None:
        return DEFAULT_MS, None
    try:
        t = float(value)
    except (TypeError, ValueError):
        t = math.nan
    if math.isfinite(t):
        if 0.001 <= t < 1.0:
            return int(math.floor(t * 1000.0 + 0.5)), None
        if t >= 1.0:
            return int(math.floor(t + 0.5)), None
    return DEFAULT_MS, ("[DISPLAY]CYCLE_TIME = {} is invalid, expected seconds "
                        "(0.001 to below 1.0) or milliseconds (1 or more): "
                        "using {:.3f} s".format(value, DEFAULT_MS / 1000.0))


def display_cycle_time(inifile, warn=None):
    """Read [DISPLAY]CYCLE_TIME from a linuxcnc.ini object, in milliseconds.

    warn is called with the message for an invalid value. Without warn
    the message goes to stderr, unless another reader in the same process
    already reported it.
    """
    ms, message = parse_cycle_time(inifile.find("DISPLAY", "CYCLE_TIME"))
    if message:
        if warn is not None:
            warn(message)
        elif message not in _reported:
            print(message, file=sys.stderr)
        _reported.add(message)
    return ms
