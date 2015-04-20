#!/bin/bash

SLOTS=/sys/devices/bone_capemgr.*/slots
for DTBO in BB-BONE-REPLICAP ; do
	if grep -q $DTBO $SLOTS ; then
		echo $DTBO overlay found
	else
		echo $DTBO overlay not found
		exit 1
	fi
done;

if [ ! -r /sys/class/uio/uio0 ] ; then
	echo PRU control files not found in /sys/class/uio/uio0 >&2
	exit 1;
fi

