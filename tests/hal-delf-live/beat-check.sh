#!/bin/sh
# exit 0 when thread 't' is still executing passes
a=$(halcmd getp t.threadbeat)
sleep 0.1
b=$(halcmd getp t.threadbeat)
echo "threadbeat $a -> $b"
[ "$b" -gt "$a" ]
