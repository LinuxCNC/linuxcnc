#!/bin/sh
rm -f bitops
set -e
gcc -I../../src/rtapi bitops.c -o bitops
./bitops
