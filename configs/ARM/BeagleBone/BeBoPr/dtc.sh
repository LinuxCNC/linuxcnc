#!/bin/sh

dtc -O dtb -o BB-LCNC-BEBOPR-00A0.dtbo -b 0 -@ BB-LCNC-BEBOPR-00A0.dts && \
cp BB-LCNC-BEBOPR-00A0.dtbo /lib/firmware/


