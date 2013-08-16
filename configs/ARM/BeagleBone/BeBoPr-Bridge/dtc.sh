#!/bin/sh

dtc -O dtb -o BB-LCNC-BEBOPRBR-00A0.dtbo -b 0 -@ BB-LCNC-BEBOPRBR-00A0.dts && \
cp BB-LCNC-BEBOPRBR-00A0.dtbo /lib/firmware/

