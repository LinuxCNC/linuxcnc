#!/bin/bash
# The C client library is usable from C and C++ only if the installed
# header needs nothing from the tree, so compile against $HEADERS and link
# against $LIBDIR exactly as an out-of-tree UI would.

set -e

cc -std=gnu11 -Wall -Wextra -Werror -I"${HEADERS}" -DULAPI \
    -o client-test client-test.c -L"${LIBDIR}" \
    -Wl,-rpath,"${LIBDIR}" -llinuxcnc_client -lm

rm -f client-test.var client-test.var.bak client-test.result

# On a failure "linuxcnc" pops up a Tk message box when DISPLAY is set, and
# a headless or CI run would wait in it forever.
unset DISPLAY

# client-test.ini names this program as its DISPLAY, so linuxcnc starts it,
# waits for it and passes its exit status on.
if ! linuxcnc -r client-test.ini > client-test.result 2>&1; then
    cat client-test.result
    echo "linuxcnc exited with a failure"
    exit 1
fi

cat client-test.result
if grep -q '^FAIL' client-test.result; then
    echo "the C client test reported failures"
    exit 1
fi
if ! grep -q '^PASS' client-test.result; then
    echo "the C client test did not run"
    exit 1
fi

rm -f client-test client-test.var client-test.var.bak client-test.result
exit 0
