#!/bin/sh
if [ "$ACTION" = "remove" ]; then
	rm /dev/rtf/? /dev/rtf?
	rmdir /dev/rtf
else
	mkdir -p /dev/rtf
	seq 0 9 | while read i; do
		if ! [ -e /dev/rtf/$i ]; then
			mknod /dev/rtf/$i c 150 $i
			chmod 666 /dev/rtf/$i
		fi

		[ -e /dev/rtf$i ] || ln -s rtf/$i /dev/rtf$i
	done
fi
