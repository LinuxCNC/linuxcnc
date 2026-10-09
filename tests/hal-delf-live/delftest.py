#!/usr/bin/env python3
# Hold the realtime thread inside a function and delf the function from
# another process. delf must not return before the thread has left the
# function. unloadrt of a component whose function is in the running
# thread removes the function the same way first; it works after stop too.
#
# Every wait is on a HAL state. A wait that takes more than 10 s of wall
# time means the machine is dying; the test then aborts.

import hal
import subprocess
import sys
import time


def halcmd(*args, ok=True):
    r = subprocess.run(["halcmd"] + list(args))
    if (r.returncode == 0) != ok:
        sys.exit("FAIL: halcmd %s returned %d" % (" ".join(args), r.returncode))


def wait(cond):
    deadline = time.monotonic() + 10
    while not cond():
        if time.monotonic() > deadline:
            sys.exit("FAIL: no progress in 10 s")
        time.sleep(0.001)


# The thread has completed a full pass once threadbeat advanced by 2.
# Python ints don't wrap.
def wait_beat(n):
    beat = hal.get_p("t.threadbeat")
    wait(lambda: hal.get_p("t.threadbeat") - beat >= n)


halcmd("loadrt", "delfvictim", "names=v")
halcmd("addf", "v", "t")
wait_beat(2)

hal.set_p("v.hold", True)
wait(lambda: hal.get_p("v.inside"))
# The thread is inside v and stays there for 500 periods. delf must
# wait for it to leave; once delf has returned, v.inside must be false.
halcmd("delf", "v", "t")
if hal.get_p("v.inside"):
    sys.exit("FAIL: delf returned while the thread was inside v")
print("ok: delf waits for the thread to leave the function")

halcmd("unloadrt", "delfvictim")
wait_beat(2)
print("ok: the thread runs after delf and unloadrt")

# unloadrt without delf while the thread is inside the function: unloadrt
# must delf first and wait, not unload code the thread is executing
halcmd("loadrt", "delfvictim", "names=u")
halcmd("addf", "u", "t")
wait_beat(2)
hal.set_p("u.hold", True)
wait(lambda: hal.get_p("u.inside"))
halcmd("unloadrt", "delfvictim")
wait_beat(2)
print("ok: unloadrt waits for the thread to leave the function")

# usual path: stop, then unload with the function still in the thread
halcmd("loadrt", "delfvictim", "names=w")
halcmd("addf", "w", "t")
wait_beat(2)
halcmd("stop")
halcmd("unloadrt", "delfvictim")
halcmd("start")
wait_beat(2)
print("ok: stop, then unloadrt works as before")
