#!/usr/bin/env python3
# MDI lines queued behind a remap that yields INTERP_EXECUTE_FINISH run
# after it, in the order they were sent: see README.

import linuxcnc
import sys
import time

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MDI)

# each line reports through the operator messages, which task sends in the
# order it runs them; the remap reports before and after its yield
posts = ["post %d" % n for n in range(1, 7)]
for cmd in ["(debug,pre 1)", "(debug,pre 2)", "M400"] + ["(debug,%s)" % p for p in posts]:
    c.mdi(cmd)
want = ["pre 1", "pre 2", "M400 before the yield", "M400 after the yield"] + posts

got = []
deadline = time.time() + 30
while len(got) < len(want) and time.time() < deadline:
    m = e.poll()
    if not m:
        time.sleep(0.01)
        continue
    print("channel:", m)
    if m[0] in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
        break
    got.append(m[1].strip())
c.wait_complete()

print("ran:", ", ".join(got))
print("in order" if got == want else "not in order, wanted: " + ", ".join(want))
sys.exit(0)
