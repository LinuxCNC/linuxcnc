#!/bin/bash
# Copyright 2014
# Charles Steinkuehler <charles@steinkuehler.net>
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

dtbo_err () {
	echo "Error loading device tree overlay file: $DTBO" >&2
	exit 1
}

pin_err () {
	echo "Error exporting pin:$PIN" >&2
	exit 1
}

dir_err () {
	echo "Error setting direction:$DIR on pin:$PIN" >&2
	exit 1
}

SLOTS=/sys/devices/bone_capemgr.*/slots

# Make sure required device tree overlay(s) are loaded
for DTBO in cape-universal cape-bone-iio ; do

	if grep -q $DTBO $SLOTS ; then
		echo $DTBO overlay found
	else
		echo Loading $DTBO overlay
		sudo -A bash -c "echo $DTBO > $SLOTS" || dtbo_err
		sleep 1
	fi
done;

if [ ! -r /sys/devices/ocp.*/helper.*/AIN0 ] ; then
	echo Analog input files not found in /sys/devices/ocp.*/helper.* >&2
	exit 1;
fi

if [ ! -r /sys/class/uio/uio0 ] ; then
	echo PRU control files not found in /sys/class/uio/uio0 >&2
	exit 1;
fi

# Export GPIO pins:
# One pin needs to be exported to enable the low-level clocks for the GPIO
# modules (there is probably a better way to do this)
# 
# Any GPIO pins driven by the PRU need to have their direction set properly
# here.  The PRU does not do any setup of the GPIO, it just yanks on the
# pins and assumes you have the output enables configured already
# 
# Direct PRU inputs and outputs do not need to be configured here, the pin
# mux setup (which is handled by the device tree overlay) should be all
# the setup needed.
# 
# Any GPIO pins driven by the hal_bb_gpio driver do not need to be
# configured here.  The hal_bb_gpio module handles setting the output
# enable bits properly.  These pins _can_ however be set here without
# causing problems.  You may wish to do this for documentation or to make
# sure the pin starts with a known value as soon as possible.

sudo $(which config-pin) -f - <<- EOF

	P8.07	out	# gpio2.2	Enable System
	P8.10	in	# gpio2.4	XLIM
	P8.11	out	# gpio1.13	X_Dir
	P8.12	out	# gpio1.12	X_Step
	P8.13	out	# gpio0.23	PWM0/SPINDLE
	P8.14	in	# gpio0.26	YLIM
	P8.15	out	# gpio1.15	Y_Dir
	P8.16	out	# gpio1.14	Y_Step
	P8.18	in	# gpio2.1	ZLIM
	P8.19	out	# gpio0.22	PWM1
	P9.14	out	# gpio1.18	PWM2
	P9.15	out	# gpio1.16	Z_Step
	P9.23	out	# gpio1.17	Z_Dir
#	P9.17	out	# gpio0.5	SCS
#	P9.18	in	# gpio0.4	SDI
#	P9.21	out	# gpio0.3	SDO
#	P9.22	out	# gpio0.2	SCK
	P9.13	out	# gpio0.30	A_Dir
	P9.11	out	# gpio0.31	A_Step
	P8.09	in	# gpio2.5	STOPin
EOF

