#!/usr/bin/env python3
# The HAL query API lives in the _hal extension module, but has to be
# reachable as hal.query. Check that all four ways of getting at it
# resolve to the same module object.
import sys

failures = 0

def check(cond, msg):
    global failures
    if cond:
        print("ok -", msg)
    else:
        print("FAIL -", msg)
        failures += 1

import _hal
check(hasattr(_hal, "query"), "import _hal; _hal.query")

import hal
check(hal.query is _hal.query, "hal.query attribute access")

from hal import query
check(query is _hal.query, "from hal import query")

import hal.query
check(sys.modules["hal.query"] is _hal.query, "import hal.query")
check(hal.query.__name__ == "_hal.query", "submodule keeps its own name")

if failures:
    print("%d FAILURES" % failures)
    sys.exit(1)
print("ALL TESTS PASSED")
