#!/bin/sh
rm -f rtapi_test
#set -e
gcc -g -DULAPI \
    -I../../include \
    rtapi_test.c \
    ../../lib/liblinuxcnculapi.so -o rtapi_test


realtime stop
./rtapi_test
if [ $? -ne 1 ]; then
    echo "rtapi_test: expected 1, got " $?
    exit 1;
fi
realtime start
./rtapi_test
if [ $? -ne 0 ]; then
    echo "rtapi_test: expected 0, got " $?
    exit 1;
fi

realtime stop
exit 0
