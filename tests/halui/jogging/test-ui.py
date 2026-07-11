#!/usr/bin/env python3

# Ported to the gomc REST/WS API. The classic test drove halui.* pins through a
# userspace `python-ui` HAL component wired in postgui.hal; gomc has no
# userspace HAL components, so we drive the halui/joint pins directly via
# halcmd, and use the gmi client for task state/mode. The `h[...]` shim below
# maps the old logical pin names to the real HAL pins/signals.

import gmi
from gmi.constants import *

import subprocess
import time
import sys
import os

timeout = 10.0

sys.stdout = os.fdopen(sys.stdout.fileno(), 'w')
program_start = time.time()

def log(msg):
    print("%.3f: %s" % (time.time() - program_start, msg)); sys.stdout.flush()

def _setp(pin, val):
    subprocess.call(["halcmd", "setp", pin, str(val)])

def _get(cmd, name):
    out = subprocess.check_output(["halcmd", cmd, name]).decode()
    return out.split()[-1]

def _getp_bit(pin):  return 1 if _get("getp", pin) in ("TRUE", "1") else 0
def _getp_flt(pin):  return float(_get("getp", pin))
def _gets_flt(sig):  return float(_get("gets", sig))

_POS_SIG = {0: "Xpos", 1: "Ypos", 2: "Zpos"}

class H:
    # dict-like shim: h[name] read/write maps to real HAL pins/signals.
    def __getitem__(self, k):
        if k.endswith("-homed"):    return _getp_bit("halui.joint.%s.is-homed" % k.split("-")[1])
        if k.endswith("-selected"): return _getp_bit("halui.joint.%s.is-selected" % k.split("-")[1])
        if k.endswith("-position"): return _gets_flt(_POS_SIG[int(k.split("-")[1])])
        if k.endswith("-velocity"): return _getp_flt("joint.%s.vel-cmd" % k.split("-")[1])
        if k == "motion-mode-is-joint": return _getp_bit("halui.mode.is-joint")
        raise KeyError(k)
    def __setitem__(self, k, v):
        if k.endswith("-home"):      _setp("halui.joint.%s.home" % k.split("-")[1], v)
        elif k.endswith("-select"):  _setp("halui.joint.%s.select" % k.split("-")[1], v)
        elif k.endswith("-jog-plus"):  _setp("halui.joint.%s.plus" % k.split("-")[1], v)
        elif k.endswith("-jog-minus"): _setp("halui.joint.%s.minus" % k.split("-")[1], v)
        elif k == "jog-selected-minus": _setp("halui.joint.selected.minus", v)
        elif k == "jog-selected-plus":  _setp("halui.joint.selected.plus", v)
        elif k == "motion-mode-joint":  _setp("halui.mode.joint", v)
        else: raise KeyError(k)

h = H()

def introspect(h):
    os.system("halcmd show pin halui"); os.system("halcmd show sig")

def home_joint(name):
    log("    homing %s" % name)

    h[name + '-home'] = 1
    start = time.time()
    while (h[name + '-homed'] == 0) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[name + '-homed'] == 0:
        log("failed to home %s in halui" % name)
        introspect(h)
        sys.exit(1)

    h[name + '-home'] = 0


def select_joint(name, deassert=True):
    log("    selecting %s" % name)

    h[name + '-select'] = 1
    start = time.time()
    while (h[name + '-selected'] == 0) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[name + '-selected'] == 0:
        log("failed to select %s in halui" % name)
        introspect(h)
        sys.exit(1)

    if deassert:
        h[name + '-select'] = 0


def jog_minus(name, target):
    start_position = h[name + '-position']
    log("    jogging %s negative: to %.3f" % (name, target))

    h['jog-selected-minus'] = 1

    start = time.time()
    while (h[name + '-position'] > target) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[name + '-position'] > target:
        log("failed to jog %s to %.3f (timed out at %.3f after %.3f seconds)" % (name, target, h[name + '-position'], timeout))
        introspect(h)
        sys.exit(1)

    h['jog-selected-minus'] = 0

    log("    jogged %s negative past target %.3f" % (name, target))

    return True


def jog_plus(name, target):
    start_position = h[name + '-position']
    log("    jogging %s positive: to %.3f" % (name, target))

    h['jog-selected-plus'] = 1

    start = time.time()
    while (h[name + '-position'] < target) and ((time.time() - start) < timeout):
        time.sleep(0.1)

    if h[name + '-position'] < target:
        log("failed to jog %s to %.3f (timed out at %.3f after %.3f seconds)" % (name, target, h[name + '-position'], timeout))
        introspect(h)
        sys.exit(1)

    h['jog-selected-plus'] = 0

    log("    jogged %s positive past target %.3f" % (name, target))

    return True


def wait_for_pin_value(pin_name, target_value, timeout=2.0):
    start_time = time.time()
    while h[pin_name] != target_value:
        if (time.time() - start_time) > timeout:
            log("Error: pin %s didn't reach target value %s!" % (pin_name, target_value))
            log("pin value is %s at timeout after %.3f seconds" % (h[pin_value], timeout))
            sys.exit(1)
        time.sleep(0.1)


def wait_for_joint_to_stop(joint_number):
    pos_pin = 'joint-%d-position' % joint_number
    vel_pin = 'joint-%d-velocity' % joint_number
    start_time = time.time()
    prev_pos = h[pos_pin]
    while (time.time() - start_time) < timeout:
        time.sleep(0.1)
        new_pos = h[pos_pin]
        if new_pos == prev_pos and h[vel_pin] == 0.0:
            log("joint %d stopped jogging" % joint_number)
            return
        prev_pos = new_pos
    log("Error: joint didn't stop jogging!")
    log("joint %d is at %.3f %.3f seconds after reaching target (prev_pos=%.3f, val=%.3f)" % (joint_number, h[pos_pin], timeout, prev_pos, h[vel_pin]))
    sys.exit(1)


def jog_joint(joint_number, target):
    success = True

    joint = []
    for j in range(0,3):
        joint.append(h['joint-%d-position' % j])

    name = 'joint-%d' % joint_number

    log("jogging %s to %.3f" % (name, target))
    select_joint(name)

    if h[name + '-position'] > target:
        jog_minus(name, target)
    else:
        jog_plus(name, target)

    for j in range(0,3):
        pin_name = 'joint-%d-position' % j
        if j == joint_number:
            if joint[j] == h[pin_name]:
                log("joint %d didn't move but should have!" % j)
                success = False
        else:
            if joint[j] != h[pin_name]:
                log("joint %d moved from %.3f to %.3f but should not have!" % (j, joint[j], h[pin_name]))
                success = False

    wait_for_joint_to_stop(joint_number)

    if not success:
        sys.exit(1)



# ---- task command/status via gmi ----
c = gmi.Command()
s = gmi.Stat()
c.state(STATE_ESTOP_RESET)
c.state(STATE_ON)
c.wait_complete()

# Select joints 1 and 2, but do not de-assert the .select pins.
select_joint('joint-1', deassert=False)
select_joint('joint-2', deassert=False)

# Home all the joints.  This should work fine.
home_joint('joint-0')
home_joint('joint-1')
home_joint('joint-2')

# Deassert the .select pins for joints 1 and 2.
h['joint-1-select'] = 0
h['joint-2-select'] = 0

# The machine is homed and all the .joint.N.select pins are deasserted.
c.mode(MODE_MANUAL)
c.wait_complete()

h['motion-mode-joint'] = 1
wait_for_pin_value('motion-mode-is-joint', 1, 2.0)
h['motion-mode-joint'] = 0


#
# First some simple single-axis jog & stop.  Test each axis jogging in each
# direction, one at a time.
#
# These jog_joint() functions will exit with a return value of 1 if
# something goes wrong, signalling test failure.
#

jog_joint(0, -0.5)
jog_joint(0, 0.0)

jog_joint(1, -0.5)
jog_joint(1, 0.0)

jog_joint(2, -0.5)
jog_joint(2, 0.0)


#
# Next try selecting a joint and jogging it with the "jog selected" pins,
# then changing which joint is selected.  The expected behavior when
# changing which joint is selected is that the old joint stops jogging and
# the new one starts jogging.
#
# We do this while jogging yet a third joint using its private, per-joint
# jog pins, to verify that it is unaffected by all the drama.
#

name0 = 'joint-0'
name1 = 'joint-1'
name2 = 'joint-2'

start_position0 = h[name0 + '-position']
start_position1 = h[name1 + '-position']
start_position2 = h[name2 + '-position']

print("%s starting at %.3f" % (name0, start_position0))
print("%s starting at %.3f" % (name1, start_position1))
print("%s starting at %.3f" % (name2, start_position2))


#
# start joint0 jogging in the positive direction
#

print("jogging %s positive using the private per-joint jog pins" % name0)
h[name0 + '-jog-plus'] = 1

# wait for this joint to come up to speed
start = time.time()
while (h[name0 + '-velocity'] <= 0) and ((time.time() - start) < timeout):
    time.sleep(0.1)
if h[name0 + '-velocity'] <= 0:
    print("%s did not start jogging" % name0)
    sys.exit(1)


#
# start the selected joint1 jogging in the negative direction
#

print("jogging selected joint (%s) negative" % (name1))
select_joint(name1)
h['jog-selected-minus'] = 1

# wait for this joint to start moving
start = time.time()
while (h[name1 + '-velocity'] >= 0) and ((time.time() - start) < timeout):
    time.sleep(0.1)
if h[name1 + '-velocity'] >= 0:
    print("%s did not start jogging" % name1)
    sys.exit(1)

if h[name0 + '-velocity'] <= 0:
    print("%s stopped jogging" % name0)
    sys.exit(1)

if h[name1 + '-position'] >= start_position1:
    print("%s was selected but did not jog negative (start=%.3f, end=%.3f)" % (name1, start_position1, h[name1 + '-position']))
    sys.exit(1)

if h[name2 + '-position'] != start_position2:
    print("%s was not selected but moved (start=%.3f, end=%.3f)" % (name2, start_position2, h[name2 + '-position']))
    sys.exit(1)

print("%s was selected and jogged, %s was not selected and stayed still" % (name1, name2))


start_position1 = h[name1 + '-position']
start_position2 = h[name2 + '-position']

start_velocity1 = h[name1 + '-velocity']
start_velocity2 = h[name2 + '-velocity']

print("selecting %s" % name2)
select_joint(name2)

wait_for_joint_to_stop(1)

if h[name0 + '-velocity'] <= 0:
    print("%s stopped jogging" % name0)
    sys.exit(1)

if h[name1 + '-velocity'] != 0:
    print("%s was deselected but did not stop (start_vel=%.3f, end_vel=%.3f)" % (name1, start_velocity1, h[name1 + '-velocity']))
    sys.exit(1)

if h[name2 + '-velocity'] >= 0:
    print("%s was selected but did not move (start_vel=%.3f, end_vel=%.3f)" % (name2, start_velocity2, h[name2 + '-velocity']))
    sys.exit(1)

print("%s was deselected and stopped, %s was selected and jogged" % (name1, name2))


start_velocity1 = h[name1 + '-velocity']
start_velocity2 = h[name2 + '-velocity']

print("stopping jog")
h['jog-selected-minus'] = 0

wait_for_joint_to_stop(2)

if h[name0 + '-velocity'] <= 0:
    print("%s stopped jogging" % name0)
    sys.exit(1)

if h[name1 + '-velocity'] != 0:
    print("%s started moving again (start_vel=%.3f, end_vel=%.3f)" % (name1, start_velocity1, h[name1 + '-velocity']))
    sys.exit(1)

if h[name2 + '-velocity'] != 0:
    print("%s did not stop (start_vel=%.3f, end_vel=%.3f)" % (name2, start_velocity2, h[name2 + '-velocity']))
    sys.exit(1)

print("%s stopped" % name2)


sys.exit(0)

