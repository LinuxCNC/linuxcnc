#!/bin/sh
# exit 0 once thread 't' has completed two more passes; a stuck thread
# never does and the timeout in test.sh fails the test
a=$(halcmd getp t.threadbeat) || exit 1
while :; do
    b=$(halcmd getp t.threadbeat) || exit 1
    [ "$b" -ge $((a + 2)) ] && exit 0
    sleep 0.01
done
