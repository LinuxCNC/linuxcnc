#!/bin/bash -e
for i in $(seq 20); do
    gomc-server -r test.ini >server.log 2>&1 &
    SRV=$!
    for j in $(seq 100); do halcmd show comp 2>/dev/null | grep -q milltask && break; sleep 0.1; done
    sleep 0.3
    ./test-ui.py
    kill $SRV 2>/dev/null; wait 2>/dev/null
    if grep -qiE 'Segmentation fault|panic|SIGSEGV' server.log stderr 2>/dev/null; then exit 1; fi
done
