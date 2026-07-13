#!/bin/bash
# Check that the right range of values can be assigned to HAL pins of various
# types, regardless of the value's Python type.
#
# Classic drove this via the userspace Python `hal` binding.  gomc has no
# userspace components, so the s32/u32/float pins are created server-side by
# haljson and driven from the gmi python client over REST — where the same
# range coercion applies (haljson.writePin -> json int32/uint32/float64,
# rejecting out-of-range values).  (halcmd `setp` is NOT used here: it wraps
# silently instead of rejecting, so it would not exercise the range contract.)
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server module0.hal || exit 1
# NB: do not `exec` — that would replace the shell and skip hal_start_server's
# EXIT trap, leaking the resident server onto port 5080 into the next test.
./test.py
