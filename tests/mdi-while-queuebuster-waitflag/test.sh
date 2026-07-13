#!/bin/bash -e
# Fire the M400 queue-buster interleaved with plain MDIs, 20 times, and fail if
# the gomc-server ever crashes.  Only scan server.log: runtests runs this under
# `bash -x` with stderr captured, so grepping the test's own stderr would match
# the traced grep-pattern text itself (false positive).  A real crash — Go panic
# or C SIGSEGV — lands in server.log.
for i in $(seq 20); do
    gomc-server -r test.ini >server.log 2>&1 &
    SRV=$!
    for j in $(seq 100); do halcmd show comp 2>/dev/null | grep -q milltask && break; sleep 0.1; done
    sleep 0.3
    ./test-ui.py
    kill $SRV 2>/dev/null; wait 2>/dev/null
    if grep -qiE 'Segmentation fault|panic|SIGSEGV' server.log 2>/dev/null; then exit 1; fi
done
