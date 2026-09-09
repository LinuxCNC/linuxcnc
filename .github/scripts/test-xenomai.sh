#! /bin/sh

#We can not really test the xenomai runtime as long as the kernel is not running
#but this would need a VM.
#What we can do is test if the libraries where built. Due to configure autodetect,
#this can silently fail.

FILES="lib/liblinuxcnc-uspace-xenomai.so.0 lib/liblinuxcnc-uspace-xenomai-evl.so.0"
for FILE in $FILES; do
   if [ ! -f "$FILE" ]; then
	echo "$FILE not found"
	exit 1
   fi
done
echo "Xenomai libraries where built"
exit 0
