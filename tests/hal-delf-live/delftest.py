#!/usr/bin/env python3
# Hold the realtime thread inside a function and remove the function
# (delf, then unloadrt) from another process. The removal must not
# complete while the thread is still inside, and the thread must keep
# running afterwards.
#
# While delf/unloadrt waits it holds the HAL mutex, so this script talks
# to the victim through its own component pins only (no mutex needed).

import hal
import subprocess
import sys
import time

TIMEOUT = 10.0
HOLD = 1.0      # how long the thread is held inside the function

c = hal.component("delftest")
c.newpin("hold", hal.Type.BOOL, hal.Dir.OUT)
c.newpin("inside", hal.Type.BOOL, hal.Dir.IN)
c.ready()


def halcmd(*args):
    subprocess.run(["halcmd"] + list(args), check=True)


def wait(cond, what):
    end = time.monotonic() + TIMEOUT
    while not cond():
        if time.monotonic() > end:
            raise RuntimeError("timeout: " + what)
        time.sleep(0.001)


# The thread has completed a full pass once threadbeat advanced by 2.
# Python ints don't wrap. A stopped thread ends in the timeout.
def wait_beat(n):
    beat = hal.get_p("t.threadbeat")
    wait(lambda: hal.get_p("t.threadbeat") - beat >= n,
         "thread t did not advance threadbeat by %d" % n)


def remove_while_inside(cmd):
    halcmd("loadrt", "delfvictim", "names=v")
    halcmd("net", "delf-hold", "delftest.hold", "v.hold")
    halcmd("net", "delf-inside", "v.inside", "delftest.inside")
    halcmd("addf", "v", "t")
    c["hold"] = True
    wait(lambda: c["inside"], "thread did not enter v")

    p = subprocess.Popen(["halcmd"] + cmd)
    end = time.monotonic() + HOLD
    while time.monotonic() < end:
        if p.poll() is not None:
            c["hold"] = False
            sys.exit("FAIL: '%s' returned while the thread was inside v"
                     % " ".join(cmd))
        time.sleep(0.01)
    c["hold"] = False
    if p.wait(TIMEOUT) != 0:
        sys.exit("FAIL: '%s' failed" % " ".join(cmd))
    wait_beat(2)
    if cmd[0] == "delf":
        halcmd("unloadrt", "delfvictim")
    halcmd("delsig", "delf-hold")
    halcmd("delsig", "delf-inside")
    print("ok: %s waits for the thread to leave the function" % " ".join(cmd))


wait_beat(2)
remove_while_inside(["delf", "v", "t"])
remove_while_inside(["unloadrt", "delfvictim"])
