#!/usr/bin/env python3
# Hold the realtime thread inside a function and delf the function from
# another process. delf must not return before the thread has left the
# function. Then unloadrt the component and check the thread still runs.
#
# No wall-clock timeouts here: every wait is on a HAL state. test.sh
# bounds the whole test.

import hal
import subprocess
import sys
import time


def halcmd(*args):
    subprocess.run(["halcmd"] + list(args), check=True)


def wait(cond):
    while not cond():
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
