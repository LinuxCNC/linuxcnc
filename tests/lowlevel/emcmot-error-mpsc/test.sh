#!/bin/sh
# Concurrency / correctness test for the emcmotError MPSC ring buffer.
# RIP-only: builds the test driver against the in-tree motion sources.

SCRIPTDIR="$(cd "$(dirname "$0")" && pwd)"
TOPDIR="$(cd "$SCRIPTDIR/../../.." && pwd)"
SRCDIR="${TOPDIR}/src/emc/motion"
INCDIR="${TOPDIR}/include"

if [ ! -f "${SRCDIR}/emcmotutil.c" ]; then
    echo "skip: motion sources not found (RIP build required)"
    exit 0
fi

gcc -O -pthread -DULAPI -Wall \
    -I"${INCDIR}" \
    -I"${SRCDIR}" \
    "${SCRIPTDIR}/test.c" \
    "${SRCDIR}/emcmotutil.c" \
    "${SRCDIR}/dbuf.c" \
    "${SRCDIR}/stashf.c" \
    -o "${SCRIPTDIR}/test" || { echo "compile failed"; exit 1; }

"${SCRIPTDIR}/test"
exitval=$?
rm -f "${SCRIPTDIR}/test"
exit $exitval
