#!/usr/bin/env python3

# Ported to the gomc REST/WS API (gmi client).  Drive joint.<n>.jog-* directly
# via halcmd (the userspace jogwheel-encoder hal.component is gone) and read the
# joint position from gmi.Stat.joint_actual_position.

import linuxcnc
import gmi
import gmi.constants as _gk
for _n in dir(_gk):
    if not _n.startswith('_'):
        setattr(linuxcnc, _n, getattr(_gk, _n))

import subprocess
import time
import sys
import math


def _halcmd(*args):
    return subprocess.check_output(["halcmd", *args]).split()[-1].decode()


class HalShim:
    """'joint-<n>-position' -> gmi.Stat.joint_actual_position[n];
       'joint-<n>-jog-*'     -> halcmd getp/setp joint.<n>.jog-*."""

    def __getitem__(self, name):
        parts = name.split('-')
        n = int(parts[1])
        field = '-'.join(parts[2:])
        if field == 'position':
            s.poll()
            return s.joint_actual_position[n]
        return float(_halcmd("getp", "joint.%d.%s" % (n, field)))

    def __setitem__(self, name, val):
        parts = name.split('-')
        n = int(parts[1])
        field = '-'.join(parts[2:])
        if field in ('jog-counts', 'jog-enable'):
            val = int(val)
        subprocess.check_call(["halcmd", "setp", "joint.%d.%s" % (n, field), str(val)],
                              stdout=subprocess.DEVNULL)


h = HalShim()


def wait_for_joint_to_stop(joint_number):
    pos_pin = 'joint-%d-position' % joint_number
    start_time = time.time()
    timeout = 2.0
    prev_pos = h[pos_pin]
    while (time.time() - start_time) < timeout:
        time.sleep(0.1)
        new_pos = h[pos_pin]
        if new_pos == prev_pos:
            return
        prev_pos = new_pos
    print("Error: joint didn't stop jogging!")
    print("joint %d is at %.6f %.6f seconds after reaching target (prev_pos=%.6f)" % (joint_number, h[pos_pin], timeout, prev_pos))
    sys.exit(1)


def close_enough(a, b, epsilon=0.000001):
    return math.fabs(a - b) < epsilon


def jog_joint(joint_number, counts=1, scale=0.001):
    timeout = 5.0

    start_pos = {}
    for j in range(3):
        start_pos[j] = h['joint-%d-position' % j]

    target = h['joint-%d-position' % joint_number] + (counts * scale)

    h['joint-%d-jog-scale' % joint_number] = scale
    h['joint-%d-jog-enable' % joint_number] = 1
    h['joint-%d-jog-counts' % joint_number] = int(h['joint-%d-jog-counts' % joint_number]) + counts

    start_time = time.time()
    while not close_enough(h['joint-%d-position' % joint_number], target) and (time.time() - start_time < timeout):
        time.sleep(0.010)

    h['joint-%d-jog-enable' % joint_number] = 0

    print("joint jogged from %.6f to %.6f (%d counts at scale %.6f)" % (start_pos[joint_number], h['joint-%d-position' % joint_number], counts, scale))

    success = True
    for j in range(3):
        pin_name = 'joint-%d-position' % j
        if j == joint_number:
            if not close_enough(h[pin_name], target):
                print("joint %d didn't get to target (start=%.6f, target=%.6f, got to %.6f)" % (joint_number, start_pos[joint_number], target, h['joint-%d-position' % joint_number]))
                success = False
        else:
            if not close_enough(h[pin_name], start_pos[j]):
                print("joint %d moved from %.6f to %.6f but should not have!" % (j, start_pos[j], h[pin_name]))
                success = False

    wait_for_joint_to_stop(joint_number)

    if not success:
        sys.exit(1)


#
# connect to LinuxCNC
#

c = gmi.Command()
s = gmi.Stat()
e = gmi.ErrorChannel()

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MANUAL)
time.sleep(0.5)


#
# run the test
#

jog_joint(0, counts=1, scale=0.001)
jog_joint(1, counts=10, scale=-0.025)
jog_joint(2, counts=-100, scale=0.100)

sys.exit(0)
