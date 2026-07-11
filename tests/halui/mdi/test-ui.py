#!/usr/bin/env python3

# Ported to the gomc REST/WS API. halui.mdi-command-NN and halui.mode.is-* are
# driven directly via halcmd (the userspace python-ui HAL component + postgui
# nets are gone); task state/mode via the gmi client.

import gmi
from gmi.constants import *
import subprocess, time, sys, os, math

program_start = time.time()
def log(msg):
    print("%.3f: %s" % (time.time() - program_start, msg)); sys.stdout.flush()

def _setp(pin, val): subprocess.call(["halcmd", "setp", pin, str(val)])
def _get(cmd, name): return subprocess.check_output(["halcmd", cmd, name]).decode().split()[-1]
def _bit(pin): return 1 if _get("getp", pin) in ("TRUE", "1") else 0
_POS = {0: "Xpos", 1: "Ypos", 2: "Zpos"}

class H:
    def __getitem__(self, k):
        if k.endswith("-position"): return float(_get("gets", _POS[int(k.split("-")[1])]))
        if k == "is-manual": return _bit("halui.mode.is-manual")
        if k == "is-auto":   return _bit("halui.mode.is-auto")
        if k == "is-mdi":    return _bit("halui.mode.is-mdi")
        raise KeyError(k)
    def __setitem__(self, k, v):
        if k.startswith("mdi-"): _setp("halui.mdi-command-%02d" % int(k.split("-")[1]), v)
        else: raise KeyError(k)
h = H()

def wait_for_joint_to_stop_at(joint, target):
    timeout, tol = 10.0, 0.0001
    start = time.time(); curr_pos = 0
    while (time.time() - start) < timeout:
        prev_pos = curr_pos; curr_pos = h['joint-%d-position' % joint]
        vel = curr_pos - prev_pos
        if (math.fabs(curr_pos - target) < tol) and (vel == 0):
            log("joint %d stopped at %.3f" % (joint, target)); return
        time.sleep(0.1)
    log("timeout waiting for joint %d to stop at %.3f (pos=%.3f)" % (joint, target, curr_pos)); sys.exit(1)

def wait_for_task_mode(target):
    start = time.time()
    while (time.time() - start) < 10.0:
        s.poll()
        if s.task_mode == target: return
        time.sleep(0.1)
    log("timeout waiting for task mode %d (it's %d)" % (target, s.task_mode)); sys.exit(1)

def wait_for_halui_mode(pin_name):
    start = time.time()
    while (time.time() - start) < 10.0:
        if h[pin_name]:
            print("halui reports mode {}".format(pin_name)); return
        time.sleep(0.1)
    print("timeout waiting for halui mode {}".format(pin_name)); sys.exit(1)

c = gmi.Command(); s = gmi.Stat(); e = gmi.ErrorChannel()
c.state(STATE_ESTOP_RESET); c.state(STATE_ON); c.wait_complete()

log("setting mode to Manual"); c.mode(MODE_MANUAL); wait_for_halui_mode('is-manual')
log("running MDI command 0"); h['mdi-0'] = 1
wait_for_joint_to_stop_at(0, -1); wait_for_joint_to_stop_at(1, 0); wait_for_joint_to_stop_at(2, 0)
h['mdi-0'] = 0; wait_for_task_mode(MODE_MANUAL); wait_for_halui_mode('is-manual')

log("setting mode to Auto"); c.mode(MODE_AUTO); wait_for_halui_mode('is-auto')
log("running MDI command 1"); h['mdi-1'] = 1
wait_for_joint_to_stop_at(0, 1); wait_for_joint_to_stop_at(1, 0); wait_for_joint_to_stop_at(2, 0)
h['mdi-1'] = 0; wait_for_task_mode(MODE_AUTO); wait_for_halui_mode('is-auto')

log("setting mode to MDI"); c.mode(MODE_MDI); wait_for_halui_mode('is-mdi')
log("running MDI command 2"); h['mdi-2'] = 1
wait_for_joint_to_stop_at(0, 1); wait_for_joint_to_stop_at(1, 2); wait_for_joint_to_stop_at(2, 0)
h['mdi-2'] = 0; s.poll(); wait_for_task_mode(MODE_MDI); wait_for_halui_mode('is-mdi')

log("running MDI command 3"); h['mdi-3'] = 1
wait_for_joint_to_stop_at(0, 1); wait_for_joint_to_stop_at(1, 2); wait_for_joint_to_stop_at(2, 3)
h['mdi-3'] = 0; wait_for_task_mode(MODE_MDI); wait_for_halui_mode('is-mdi')

log("running MDI command 0"); h['mdi-0'] = 1
wait_for_joint_to_stop_at(0, -1); wait_for_joint_to_stop_at(1, 0); wait_for_joint_to_stop_at(2, 0)
h['mdi-0'] = 0; wait_for_task_mode(MODE_MDI); wait_for_halui_mode('is-mdi')

sys.exit(0)
