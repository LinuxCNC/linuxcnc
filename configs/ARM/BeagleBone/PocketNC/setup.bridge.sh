#!/bin/bash

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
for DTBO in PocketNCdriver cape-bone-iio ; do

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

done <<- EOF
	66	out	# P8.7		gpio2.2		Enable_n (ECO location)
#	67	out	# P8.8		gpio2.3		X_Min
#	69	out	# P8.9		gpio2.5		X_Max
#	68	out	# P8.10		gpio2.4		Y_Min
	45	out	# P8.11		gpio1.13	X_Dir
	44	out	# P8.12		gpio1.12	X_Step
	23	out	# P8.13		gpio0.23	PWM0
#	26	out	# P8.14		gpio0.26	Y_Max
	47	out	# P8.15		gpio1.15	Y_Dir
	46	out	# P8.16		gpio1.14	Y_Step
#	27	out	# P8.17		gpio0.27	Z_Min
#	65	out	# P8.18		gpio2.1		Z_Max
	22	out	# P8.19		gpio0.22	PWM1
#	61	out	# p8.26		gpio1.29	Status
	50	out	# p9.14		gpio1.18	PWM2
	48	out	# p9.15		gpio1.16	Z_Step
#	5	out	# p9.17		gpio0.5		B_Dir
#	4	out	# p9.18		gpio0.4		B_Step
#	3	out	# p9.21		gpio0.3		A_Dir
#	2	out	# p9.22		gpio0.2		A_Step
	112	out	# p9.30		gpio3.16		B_Dir
	111	out	# p9.29		gpio3.15		B_Step
	113	out	# p9.21		gpio0.3		A_Dir
	115	out	# p9.22		gpio0.2		A_Step
	49	out	# P9.23		gpio1.17	Z_Dir
#	15	out	# P9.24		gpio0.15	Spindle
#	14	out	# P9.26		gpio0.14	Mtr_Ena
EOF
