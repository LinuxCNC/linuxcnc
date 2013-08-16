#!/bin/sh

dtc -O dtb -o LCNC-BBB-K9S-00A0.dtbo -b 0 -@ LCNC-BBB-K9S-00A0.dts && \
cp LCNC-BBB-K9S-00A0.dtbo /lib/firmware/

