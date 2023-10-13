#!/usr/bin/env python3
#
# Copyright (C) 2013-2016 Sebastian Kuzminsky <seb@highlab.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#

#
# This is a userspace, non-realtime component that interfaces the control
# box of a Scorbot ER-3 robot arm to the LinuxCNC HAL.
#
# Joint 0: rotation around the base
# Joint 1: shoulder
# Joint 2: elbow
# Joint 3: wrist (+ is wrist up & rotate hand)
# Joint 4: wrist (+ is wrist down & rotate hand)
# Joint 5: unused
# Joint 6: unused
# Joint 7: hand open/close (+ is close)
#

import hal
import serial
import time
import math

def serial_write(data):
    serial.write(data)

def serial_read(num_bytes):
    if not hasattr(serial_read, "warned"):
        serial_read.warned = False
    data = serial.read(num_bytes)
    if len(data) != num_bytes:
        serial_read.warned = True
        print("*** Error reading Scorbot-ER III serial port")
        print("*** Is the cable plugged in and the control box powered on?")
    else:
        serial_read.warned = False
    return data


port = '/dev/ttyS0'
serial = serial.serial_for_url(
    port,
    9600,
    bytesize = serial.EIGHTBITS,
    parity = serial.PARITY_NONE,
    stopbits = serial.STOPBITS_TWO,
    rtscts = False,
    xonxoff = False,
    timeout = 1
)

# disable "interrupts" from the Scorbot ER-3
serial_write('X')


h = hal.component("scorbot-er-3")
h.setprefix("scorbot-er-3")

# create pins, initialize robot
old_motor_pos_cmd = [None] * 8
old_counts = [None] * 8
old_motor_max_vel = [None] * 8
for joint in range(0, 8):
    h.newpin('joint%d.limit-sw' % joint, hal.HAL_BIT, hal.HAL_OUT)
    h.newpin('joint%d.motor-pos-cmd' % joint, hal.HAL_FLOAT, hal.HAL_IN)
    h.newpin('joint%d.scale' % joint, hal.HAL_FLOAT, hal.HAL_IN)
    h.newpin('joint%d.motor-max-vel' % joint, hal.HAL_S32, hal.HAL_IN)

    h['joint%d.motor-pos-cmd' % joint] = 0.0
    old_motor_pos_cmd[joint] = 0.0

    h['joint%d.scale' % joint] = 1.0

    old_counts[joint] = 0

    h['joint%d.motor-max-vel' % joint] = 9  # fastest speed
    old_motor_max_vel[joint] = 0 # force an update right away

h.ready()


# main loop
while True:
    for joint in range(0, 8):
        # the scorbot-er-3 servos are commanded in delta encoder counts
        # this component/driver gets input in the form of position (angular) and scale
        # position = counts / scale
        # counts = position * scale
        if not math.isnan(h['joint%d.motor-pos-cmd' % joint]):
            new_counts = int(h['joint%d.motor-pos-cmd' % joint] * h['joint%d.scale' % joint])
        else:
            new_counts = old_counts[joint]

        if new_counts != old_counts[joint]:
            delta = new_counts - old_counts[joint]

            serial_write('%dm%d\n\r' % (joint+1, delta))
            old_motor_pos_cmd[joint] = h['joint%d.motor-pos-cmd' % joint]
            old_counts[joint] = new_counts

        if h['joint%d.motor-max-vel' % joint] != old_motor_max_vel[joint]:
            serial_write('%dv%d\n' % (joint+1, h['joint%d.motor-max-vel' % joint]))
            old_motor_max_vel[joint] = h['joint%d.motor-max-vel' % joint]

        serial_write('%dl' % (joint+1))

    data = serial_read(8)
    for joint in range(0, 8):
        l = data[joint:joint+1]
        if l != None:
            h['joint%d.limit-sw' % joint] = bool(int(l))

    time.sleep(0.001)

