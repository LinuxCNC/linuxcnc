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

PRU=/sys/class/uio/uio0
echo -n "Waiting for $PRU "

while [ ! -r $PRU ]
do
    echo -n "."
    sleep 1
done
echo OK

if [ ! -r $PRU ] ; then
	echo PRU control files not found in $PRU >&2
	exit 1;
fi

# Using Official overlay, setup pin muxing to match what the HAL file expects

# Setup PWM outputs to use GPIO pins
#for FILE in /sys/devices/ocp.*/bebopr_pwm_J[934]_pinmux.*/state ; do
#	if [ -f $FILE ] ; then
#		sudo -A su -c "echo gpio > $FILE" || bebopr_err pwm: $FILE
#	fi
#done

# Setup LED with no trigger, so we can drive it with the GPIO pin
# Change this if you want the LED tied to something else (like heartbeat)
#FILE=/sys/devices/ocp.*/bebopr_leds.*/leds/bebopr\:status_led/trigger
#if [ -f $FILE ] ; then
#	sudo -A su -c "echo none > $FILE" || bebopr_err led: $FILE
#fi

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

	P8.03	low		# gpio1.06	Enable
	P8.05	high	# gpio1.02	Enable_n
	P8.07	high	# gpio2.02	Enable_n (ECO location)
	P8.08	high	# gpio2.03	
	P8.09	in		# gpio2.05	
	P8.10	in		# gpio2.04	
	P8.13	low		# gpio0.23	J2.PWM0-Heater
	P8.14	in		# gpio0.26	
	P8.17	in		# gpio0.27	
	P8.18	in		# gpio2.01	
	P8.19	low		# gpio0.22	J3.PWM1-Heater
	P8.20	out		# gpio1.31	E_ENA
	P8.21	out		# gpio1.30	E_DIR
	P8.25	out		# gpio1.00	STATUS_LED
	P8.27	out		# gpio2.22	Z_Step
	P8.28	out 	# gpio2.24	Z_Ena
	P8.29	out 	# gpio2.23	Z_Dir
	P8.29	out 	# gpio2.23	Z_Dir
	P8.30	out		# gpio0.10	E_Step
	P8.31	in		# gpio0.10	X_Min
	P8.32	in		# gpio0.11	X_Max
	P8.33	in		# gpio0.09	Y_Max
	P8.35	in		# gpio0.08	Y_Min
	P8.36	out		# gpio2.16	J4.PWM
	P8.37	in		# gpio2.14	Z_Max
	P8.38	in		# gpio2.15	Z_Min
	P8.39	out		# gpio2.12	Z_Min
	P8.40	out		# gpio2.13	Y_Ena
	P8.41	out		# gpio2.10	X_Ena
	P8.42	out		# gpio2.11	Y_Step
	P8.43	out		# gpio2.08	X_Step
	P8.44	out		# gpio2.09	X_Dir
	P8.45	low		# gpio2.06	PWM1
	P8.46	low		# gpio2.07	PWM0
	P9.14	low		# gpio1.18	J4.PWM2-Heater
#	P9.36	in		# THRM2
#	P9.38	in		# THRM1
#
EOF

