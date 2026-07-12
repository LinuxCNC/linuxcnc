#!/bin/bash
cp -f orig.ngc test.ngc
cp -f subs/orig-sub.ngc subs/sub.ngc
gomc-server -r interp.ini >/tmp/gomc-subret.log 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT
for i in $(seq 100); do halcmd show comp 2>/dev/null | grep -q milltask && break; sleep 0.1; done
sleep 0.5
./test-ui.py
