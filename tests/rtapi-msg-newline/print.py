#!/usr/bin/env python3
# Make msgline print from the realtime thread and wait until it has.
# A wait that takes more than 10 s of wall time means the machine is
# dying; the test then aborts.

import hal
import sys
import time

hal.set_p("msgline.0.print", "1")
deadline = time.monotonic() + 10
while hal.get_p("msgline.0.print"):
    if time.monotonic() > deadline:
        sys.exit("FAIL: the thread did not run msgline.0 in 10 s")
    time.sleep(0.01)
