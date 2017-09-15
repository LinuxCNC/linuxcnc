#!/usr/bin/env python
#!/usr/bin/env python
#    This is a component of AXIS, a front-end for LinuxCNC
#    Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net> and
#    Chris Radek <chris@timeguy.com>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

import sys, os
import linuxcnc

ini = linuxcnc.ini(sys.argv[1])

nproblems = 0
def report_problem(msg, *args):
    global nproblems
    nproblems += 1
    if args:
        print msg % args
    else:
        print msg


def get_int(section, key):
	return int(ini.find(section, key).split()[0])
def get_float(section, key):
	return float(ini.find(section, key).split()[0])

if ini.find("EMCMOT", "BASE_PERIOD") is None:
    period = 0.
else:
    period = get_int("EMCMOT", "BASE_PERIOD")
njoints = get_int("KINS", "JOINTS")

is_stepper = ini.find("JOINT_0", "STEPGEN_MAXVEL")
if is_stepper: print "Appears to be a stepper configuration"
else: print "Appears to be a servo configuration"

for i in range(njoints):
    joint = "JOINT_%d" % i
    if ini.find(joint,"INPUT_SCALE") is None: continue
    scale = get_float(joint, "INPUT_SCALE")
    if is_stepper:
        if ini.find(joint,"STEPGEN_MAXVEL") is None: continue
        vel = get_float(joint, "STEPGEN_MAXVEL")
        cycles_per_step = 2
        required_period = 1000000000 / vel / scale / cycles_per_step
        if abs(required_period) < abs(period):
            report_problem(
                "Max Velocity %g and scale %g require BASE_PERIOD below %d",
                vel, scale, int(required_period))

        vel = get_float(joint, "MAX_VELOCITY")
        headroom_vel = get_float(joint, "STEPGEN_MAXVEL")
        if headroom_vel < vel * 1.01:
            report_problem(
                "Less than 1%% velocity headroom from %g to %g",
                vel, headroom_vel)

        acc = get_float(joint, "MAX_ACCELERATION")
        headroom_acc = get_float(joint, "STEPGEN_MAXACCEL")
        if headroom_acc < acc * 1.01:
            report_problem(
                "Less than 1%% acceleration headroom from %g to %g",
                acc, headroom_acc)


if nproblems == 0:
    print "No problems found"
elif nproblems == 1:
    print "One problem found"
else:
    print "%d problems found" % nproblems

# vim:sw=4:sts=4:et
