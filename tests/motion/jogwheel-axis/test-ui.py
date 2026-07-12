#!/usr/bin/env python3

# Ported to the gomc REST/WS API (gmi client).  The userspace hal.component that
# fed the jogwheel-encoder pins is gone; drive axis.<a>.jog-* directly via
# halcmd and read the axis position from gmi.Stat.  linuxcnc_util is reused by
# copying gmi's constants onto the (command/stat-less) linuxcnc module.

import linuxcnc
import gmi
import gmi.constants as _gk
for _n in dir(_gk):
    if not _n.startswith('_'):
        setattr(linuxcnc, _n, getattr(_gk, _n))
import linuxcnc_util

import subprocess
import time
import sys
import math

_AXIDX = {'x': 0, 'y': 1, 'z': 2}


def _halcmd(*args):
    return subprocess.check_output(["halcmd", *args]).split()[-1].decode()


class HalShim:
    """Maps the old test-ui component pin names onto real motion pins/status.

    'axis-<a>-position'  -> gmi.Stat.actual_position[axis]
    'axis-<a>-jog-*'     -> halcmd getp/setp axis.<a>.jog-*
    """

    def __getitem__(self, name):
        parts = name.split('-')
        a = parts[1]
        field = '-'.join(parts[2:])
        if field == 'position':
            s.poll()
            return s.actual_position[_AXIDX[a]]
        return float(_halcmd("getp", "axis.%s.%s" % (a, field)))

    def __setitem__(self, name, val):
        parts = name.split('-')
        a = parts[1]
        field = '-'.join(parts[2:])
        if field in ('jog-counts', 'jog-enable'):
            val = int(val)
        subprocess.check_call(["halcmd", "setp", "axis.%s.%s" % (a, field), str(val)],
                              stdout=subprocess.DEVNULL)


h = HalShim()


def close_enough(a, b, epsilon=0.000001):
    return math.fabs(a - b) < epsilon


def jog_axis(axis_letter, counts=1, scale=0.001):
    timeout = 5.0

    start_pos = {}
    for a in 'xyz':
        start_pos[a] = h['axis-%c-position' % a]

    target = h['axis-%c-position' % axis_letter] + (counts * scale)

    h['axis-%c-jog-scale' % axis_letter] = scale
    h['axis-%c-jog-enable' % axis_letter] = 1
    h['axis-%c-jog-counts' % axis_letter] = int(h['axis-%c-jog-counts' % axis_letter]) + counts

    start_time = time.time()
    while not close_enough(h['axis-%c-position' % axis_letter], target) and (time.time() - start_time < timeout):
        time.sleep(0.010)

    h['axis-%c-jog-enable' % axis_letter] = 0

    print("axis %c jogged from %.6f to %.6f (%d counts at scale %.6f)" % (axis_letter, start_pos[axis_letter], h['axis-%c-position' % axis_letter], counts, scale))

    success = True
    for a in 'xyz':
        pin_name = 'axis-%c-position' % a
        if a == axis_letter:
            if not close_enough(h[pin_name], target):
                print("axis %c didn't get to target (start=%.6f, target=%.6f, got to %.6f)" % (axis_letter, start_pos[axis_letter], target, h['axis-%c-position' % axis_letter]))
                success = False
        else:
            if not close_enough(h[pin_name], start_pos[a]):
                print("axis %c moved from %.6f to %.6f but should not have!" % (a, start_pos[a], h[pin_name]))
                success = False

    l.wait_for_axis_to_stop(axis_letter)

    if not success:
        sys.exit(1)


#
# connect to LinuxCNC
#

c = gmi.Command()
s = gmi.Stat()
e = gmi.ErrorChannel()

l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)

c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)


# must home to use Teleop mode

c.home(-1)
c.wait_complete()

l.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])

c.mode(linuxcnc.MODE_MANUAL)
time.sleep(0.5)


#
# run the test
#

jog_axis('x', counts=1, scale=0.001)
jog_axis('y', counts=10, scale=-0.025)
jog_axis('z', counts=-100, scale=0.100)

sys.exit(0)
