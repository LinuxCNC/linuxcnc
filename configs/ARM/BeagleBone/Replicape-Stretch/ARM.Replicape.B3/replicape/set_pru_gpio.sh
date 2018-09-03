#!/bin/sh

if [ ! -r /sys/class/gpio/gpio61 ] ; then
# Output-Ports (pru))
 	for Port in 45 44 23 26 47 46 27 22 61 60
  	do
	  echo "$Port" > /sys/class/gpio/export
#wait until exported gpio is available
	  while [ ! -r /sys/class/gpio/gpio${Port} ];
	    do
	      :
	    done
#set to output low
	  echo "low" > /sys/class/gpio/gpio${Port}/direction
	done;
fi
