#!/bin/bash
# Copyright 2013
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
		sudo -A su -c "echo $DTBO > $SLOTS" || dtbo_err
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

	P8.07	in	# X Max
	P8.08	in	# X Min
	P8.09	in	# Y Max
	P8.10	in	# Y Min
	P8.11	low	# FET 1 : Heated Bed
	P8.12	low	# X Dir
	P8.13	low	# X Step
	P8.14	low	# Y Dir
	P8.15	low	# Y Step
	P8.16	high	# eMMC Enable
	P8.17	in	# ESTOP
	P8.18	low	# Z Dir
	P8.19	low	# Z Step

# eMMC signals, uncomment *ONLY* if you have disabled the on-board eMMC!
# Machinekit images disable eMMC and HDMI audio by default in uEnv.txt:
#  capemgr.disable_partno=BB-BONELT-HDMI,BB-BONE-EMMC-2G
#	P8.22	low	# Servo 4
#	P8.23	low	# Servo 3
#	P8.24	low	# Servo 2
#	P8.25	low	# Servo 1

	P8.26	high	# ESTOP Out

	P9.11	in	# Z Max
	P9.12	low	# E0 Dir
	P9.13	in	# Z Min
	P9.14	high	# Axis Enable, active low
	P9.15	low	# FET 2 : E0
	P9.16	low	# E0 Step
	P9.17	low	# E1 Step
	P9.18	low	# E1 Dir
#	P9.19	low	# I2C SCL
#	P9.20	low	# I2C SDA
	P9.21	low	# FET 4 : E1
	P9.22	low	# FET 6
	P9.23	low	# Machine Power
	P9.24	low	# E2 Step
	P9.25	low	# LED
	P9.26	low	# E2 Dir
	P9.27	low	# FET 3 : E2
	P9.28	low	# SPI CS0
	P9.29	low	# SPI MISO
	P9.30	low	# SPI MOSI
	P9.31	low	# SPI SCLK

	P9.41	low	# FET 5
	P9.91	in	# Reserved, connected to P9.41

	P9.42	low	# SPI CS1
	P9.92	in	# Reserved, connected to P9.42
EOF

