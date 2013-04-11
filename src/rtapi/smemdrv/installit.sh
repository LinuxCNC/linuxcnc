#!/bin/sh
#
# Create an entry in /dev for the shared memory device.

#
# Remove the old device and driver (if they exist)
grep embshmem /proc/devices > /dev/null  && /sbin/rmmod ./embshmem.ko 
[ -e /dev/embshmem0 ] && rm -f /dev/embshmem0

#
# Install the new driver
/sbin/insmod ./embshmem.ko $* || exit 1

#
# Work out the major device number for the new driver
major=$(awk "\$2==\"embshmem\" {print \$1}" /proc/devices)
echo major is $major

#
# Create the new entry, and allow anyone to access it.
mknod /dev/embshmem0 c $major 0
chmod 666  /dev/embshmem0
