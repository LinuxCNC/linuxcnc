#!/bin/bash

if [ "$UID" -ne "0" ] ; then
        echo "Script must be run as root!"
        exit 1
fi

while read PIN DIR JUNK ; do
        case "$PIN" in
        ""|\#*)	
		continue ;;
        *)
                echo "$PIN" > "/sys/class/gpio/export"
		echo "$DIR" > "/sys/class/gpio/gpio$PIN/direction"
                ;;
        esac

done <<- "EOF"
	92	out	# gpio2.24	P8.28	Z_Ena
	80	out	# gpio2.16	P8.36	J4.PWM
	77	out	# gpio2.13	P8.40	Y_Ena
	74	out	# gpio2.10	P8.41	X_Ena

#	gpmc_ad6	7	gpio1.6		P8.3	Enable
#	gpmc_ad2	7	gpio1.2		P8.5	Enable_n

#				gpio0.11		J9  - X-max limit
#				gpio0.10		J10 - X-min limit
#				gpio0.9			J11 - Y-max limit
#				gpio0.8			J12 - Y-min limit
#				gpio2.14		J13 - Z-max limit
#				gpio2.15		J14 - Z-min limit
EOF
