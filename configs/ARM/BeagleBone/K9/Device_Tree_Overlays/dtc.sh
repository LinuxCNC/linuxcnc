#!/bin/sh

dtc -O dtb -o BB-BLACK-LCNC-K9-00A0.dtbo -b 0 -@ BB-BLACK-LCNC-K9-00A0.dts && \
cp BB-BLACK-LCNC-K9-00A0.dtbo /lib/firmware/

