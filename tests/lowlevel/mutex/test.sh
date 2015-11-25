#!/bin/sh
gcc -O -I ../../../include test.c -o test -DULAPI -std=gnu99 -pthread || exit 1
./test; exitval=$?
rm -f test
exit $exitval
