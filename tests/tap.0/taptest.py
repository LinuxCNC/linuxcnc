#!/usr/bin/env python3

import hal
import time

BEATTIMEOUT = 10.0


# RT to non-RT synchronized wait to prevent race conditions.
# The minimum wait should normally be >= 2 because you can catch the
# threadbeat variable's increment event, which in itself does not run
# the cycle. When you see an increment by 2, then you can be sure that
# the cycle ran at least once.
def waitThreadBeat(n):
    assert n > 0, "Number of threadbeat wait cycles must be >= 1"
    beat = hal.get_p('fast.threadbeat')
    starttime = time.time()
    while hal.get_p('fast.threadbeat') - beat < n:
        time.sleep(0.001)
        if time.time() - starttime > BEATTIMEOUT:
            raise RuntimeError("waitThreadBeat: Timeout after {} seconds".format(BEATTIMEOUT))


def check(pin, want, msg):
    got = hal.get_p(pin)
    assert got == want, "%s: %s is %r, want %r" % (msg, pin, got, want)
    print("ok: %s: %s=%r" % (msg, pin, got))


# initial state: everything low
check("tap.0.out", 0, "initial out")
check("tap.0.io", 0, "initial io")

# HAL logic drives in high; out and io follow
hal.set_p("tap.0.in", True)
waitThreadBeat(2)
check("tap.0.out", 1, "in rise drives out")
check("tap.0.io", 1, "in rise drives io")

# UI forces output off via io while in stays high
hal.set_p("tap.0.io", False)
waitThreadBeat(2)
check("tap.0.out", 0, "io forced off drives out")
check("tap.0.in", 1, "in still high")

# UI forces output back on
hal.set_p("tap.0.io", True)
waitThreadBeat(2)
check("tap.0.out", 1, "io forced on drives out")

# UI forces off again; HAL logic takes control back on the next in edge
hal.set_p("tap.0.io", False)
waitThreadBeat(2)
check("tap.0.out", 0, "forced off again")
hal.set_p("tap.0.in", False)
waitThreadBeat(2)
check("tap.0.out", 0, "in fall keeps out low")
check("tap.0.io", 0, "in fall drives io")
hal.set_p("tap.0.in", True)
waitThreadBeat(2)
check("tap.0.out", 1, "in rise retakes control")

print("PASS")
