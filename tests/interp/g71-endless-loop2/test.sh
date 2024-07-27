#!/bin/bash
#
# Demonstrate endless loop when an additional segment is added in the G71 call.
# This is a subset of a file that was supplied in
# https://github.com/LinuxCNC/linuxcnc/issues/2844 and which worked up to and
# including 2.9.1~pre1. It entered an endless loop after
# https://github.com/LinuxCNC/linuxcnc/pull/2677

rs274 -g g71-endless-loop2.ngc | awk '{$1=""; print}' > result &
pid=$!

# Give it 5 seconds to complete
count=5
while [ 0 -lt $count ] && kill -0 $pid > /dev/null 2>&1 ; do
    sleep 1
    count=$(($count - 1))
done

if kill -0 $pid > /dev/null 2>&1; then
    kill -9 $pid
    echo "error: g71-endless-loop2.ngc program seem to be stuck, killing"
    exit 1
fi

exit 0
