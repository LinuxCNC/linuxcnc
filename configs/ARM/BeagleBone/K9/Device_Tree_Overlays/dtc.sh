#!/bin/sh

dtc -O dtb -o BB-LCNC-K93D-00A0.dtbo -b 0 -@ BB-LCNC-K93D-00A0.dts && \
cp BB-LCNC-K93D-00A0.dtbo /lib/firmware/

dtc -O dtb -o BB-LCNC-K9wHDMI-00A0.dtbo -b 0 -@ BB-LCNC-K9wHDMI-00A0.dts && \
cp BB-LCNC-K9wHDMI-00A0.dtbo /lib/firmware/

