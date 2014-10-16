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

# Export GPIO pins
export_gpio () {
	while read PIN DIR JUNK ; do
	        case "$PIN" in
	        ""|\#*)	
			continue ;;
	        *)
			[ -r /sys/class/gpio/gpio$PIN ] && continue
	                sudo -A su -c "echo $PIN > /sys/class/gpio/export" || pin_err
			sudo -A su -c "echo $DIR > /sys/class/gpio/gpio$PIN/direction" || dir_err
	                ;;
	        esac

	done
}

# Make sure required device tree overlay(s) are loaded
ACTIVE=""
for DTBO in cape-bebopr-pp cape-bebopr-brdg BB-LCNC-BEBOPRBR ; do

	BEBOPR=`grep ${DTBO} ${SLOTS} | egrep "[0-9]+: 5[4567]:P-"`
	if [ -n "${BEBOPR}" ] ; then
		SLOT=`echo ${BEBOPR} | cut -d':' -f1`
		echo -n "Cape \"${DTBO}\" with EEPROM in slot ${SLOT} is "
		echo ${BEBOPR} | grep -q "0: 54:P---F"
		if [ $? -eq 0 ] ; then
			echo "active"
			ACTIVE=${SLOT}
			break
		else
			echo "not active"
		fi
	fi

	BEBOPR=`grep ${DTBO} ${SLOTS} | egrep "[0-9]+: ff:P-O-L"`
	if [ -n "${BEBOPR}" ] ; then
		SLOT=`echo ${BEBOPR} | cut -d':' -f1`
		echo "Overlay \"${DTBO}\" in slot ${SLOT} is active"
		ACTIVE=${SLOT}
		break
	fi
done

if [ -z "${ACTIVE}" ] ; then
	echo "Need to load overlay"

# Try loading an overlay. Start with the ones using most resources
# and most likely to fail on older capes.

	for DTBO in cape-bebopr-pp:R3 cape-bebopr-brdg:R2 BB-LCNC-BEBOPRBR ; do
		echo -n "Loading overlay \"${DTBO}\" ... "
		if sudo -A su -c "echo $DTBO > $SLOTS" ; then
			echo "Success"
			break
		else
			echo "Failed"
		fi
	done
	sleep 1

fi

if [ ! -r /sys/devices/ocp.*/44e0d000.tscadc/tiadc/iio:device0/in_voltage5_raw ] ; then
	echo "Analog input files not found in /sys/devices/ocp.*/44e0d000.tscadc/tiadc/iio:device0/" >&2
	exit 1;
fi

if [ ! -r /sys/class/uio/uio0 ] ; then
	echo PRU control files not found in /sys/class/uio/uio0 >&2
	exit 1;
fi

# Using Official overlay, setup pin muxing to match what the HAL file expects

# Setup PWM outputs to use GPIO pins
for FILE in /sys/devices/ocp.*/bebopr_pwm_J[234]_pinmux.*/state ; do
	if [ -f $FILE ] ; then
		sudo -A su -c "echo gpio > $FILE" || bebopr_err pwm: $FILE
	fi
done

# Setup LED with no trigger, so we can drive it with the GPIO pin
# Change this if you want the LED tied to something else (like heartbeat)
FILE=/sys/devices/ocp.*/bebopr_leds.*/leds/bebopr\:status_led/trigger
if [ -f $FILE ] ; then
	sudo -A su -c "echo none > $FILE" || bebopr_err led: $FILE
fi

# Export PWM GPIO pins which are not exported by the overlay
export_gpio <<- EOF
	23	low	# P8.13		gpio0.23	PWM0
	22	low	# P8.19		gpio0.22	PWM1
	50	low	# p9.14		gpio1.18	PWM2
EOF
