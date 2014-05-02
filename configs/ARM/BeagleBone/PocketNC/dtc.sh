#!/bin/sh

dtc -O dtb -o PocketNCdriver-00A0.dtbo -b 0 -@ PocketNCdriver-00A0.dts && \
cp PocketNCdriver-00A0.dtbo /lib/firmware/

