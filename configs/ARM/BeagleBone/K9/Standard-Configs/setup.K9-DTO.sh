#!/bin/bash
#This source file is provided under MIT License terms.
#Copyright (c) 2013 Calypso Ventures, Inc.
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in
#all copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#THE SOFTWARE.

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
for DTBO in BB-BLACK-LCNC-K9 ; do

	if grep -q $DTBO $SLOTS ; then
		echo $DTBO overlay found
	else
		echo Loading $DTBO overlay
		sudo -A su -c "echo $DTBO > $SLOTS" || dtbo_err
		sleep 1
	fi
done;

if [ ! -r /sys/class/uio/uio0 ] ; then
	echo PRU control files not found in /sys/class/uio/uio0 >&2
	exit 1;
fi

# Export GPIO pins
# This really only needs to be done to enable the low-level clocks for the GPIO
# modules.  There is probably a better way to do this...
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
	66	in	# J8.7  gpio2_2 
	67	in	# J8.8  gpio2_3 
	69	in	# J8.9  gpio2_5 
	67	in	# J8.10 gpio2_4 
	45	in	# J8.11 gpio1_13
	44	in	# J8.12 gpio1_12
	23	out	# J8.13 gpio0_23
	26	out	# J8.14 gpio0_26
	47	in	# J8.15 gpio1_15
	46	in	# J8.16 gpio1_14
	27	out	# J8.17 gpio0_27
	65	out	# J8.18 gpio2_1 
	22	out	# J8.19 gpio0_22
# eMMC pins 		  J8.20 
#			  thru 
#			  J8.25

	61	in	# J8.26 gpio1_29

# Start HDMI pins
#	86	out	# J8.27 gpio2_22
#	88	in	# J8.28 gpio2_24
#	87	out	# J8.29 gpio2_23
#	89	in	# J8.30 gpio2_25
#	10	in	# J8.31 gpio0_10
#	11	in	# J8.32 gpio0_11
#	9	in	# J8.33 gpio0_9 
#	81	out	# J8.34 gpio2_17
#	9	in	# J8.35 gpio0_8 
#	80	out	# J8.36 gpio2_16
#	78	in	# J8.37 gpio2_14
#	76	out	# J8.39 gpio2_12
#	77	out	# J8.40 gpio2_13
#	74	out	# J8.41 gpio2_10
#	75	out	# J8.42 gpio2_11
#	72	out	# J8.43 gpio2_8 
#	73	out	# J8.44 gpio2_9 
#	70	out	# J8.45 gpio2_6 
#	71	out	# J8.46 gpio2_7 
# end HDMI pins 

	30	in	# J9.11 gpio0_30
	60	in	# J9.12 gpio1_28
	31	out	# J9.13 gpio0_31
	50	in	# J9.14 gpio1_18
	48	in	# J9.15 gpio1_16
	51	in	# J9.16 gpio1_19
	5	out	# J9.17 gpio0_5 
	4	out	# J9.18 gpio0_4 
	3	out	# J9.21 gpio0_3 
	2	in	# J9.22 gpio0_2 
	49	in	# J9.23 gpio1_17
	15	in	# J9.24 gpio0_15
	117	out	# J9.25 gpio3_21
	14	in	# J9.26 gpio0_14
	115	out	# J9.27 gpio3_19
	113	out	# J9.28 gpio3_17    K9 use of BBB HDMI Audio pin
	111	out	# J9.29 gpio3_15    K9 use of BBB HDMI Audio pin
	112	out	# J9.30 gpio3_16
	110	out	# J9.31 gpio3_14    K9 use of BBB HDMI Audio pin

	20	in	# J9.41 gpio0_20    set = in => other signal on pin used
	116	out	# J9.41 gpio3_20
	7	in	# J9.42 gpio0_7     set = in => other signal on pin used
	114	out	# J9.42 gpio3_18
EOF

