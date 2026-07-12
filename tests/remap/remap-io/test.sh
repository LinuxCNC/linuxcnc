#!/bin/bash -e
export PYTHONUNBUFFERED=1
do_test() {
    gomc-server -r "$1" >server.log 2>&1 &
    SRV=$!
    for i in $(seq 100); do halcmd show comp 2>/dev/null | grep -q milltask && break; sleep 0.1; done
    sleep 0.5
    ./test-ui.py | grep -i m6
    kill $SRV 2>/dev/null; wait 2>/dev/null
}
echo "**********  Testing python remaps"
do_test test-py.ini
echo
echo "**********  Testing ngc remaps"
do_test test-ngc.ini
