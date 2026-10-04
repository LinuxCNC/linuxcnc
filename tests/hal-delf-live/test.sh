#!/bin/bash
# Repeatedly remove a function from a running thread and unload its
# component. Without thread quiescence in hal_del_funct_from_thread() this
# hangs or crashes rtapi_app (probabilistically, usually in the first tens
# of iterations).
set -e
${SUDO} halcompile --install delfvictim.comp

N=${DELF_ITER:-200}
TMPDIR=$(mktemp -d /tmp/hal-delf-live.XXXXXX)
trap 'rm -rf "$TMPDIR"' 0 1 2 3 15
HAL="$TMPDIR/loop.hal"

{
    echo "loadrt threads name1=t period1=100000"
    echo "start"
    for i in $(seq "$N"); do
        echo "loadrt delfvictim names=v"
        echo "addf v t"
        echo "loadusr -w sleep 0.002"
        echo "delf v t"
        echo "unloadrt delfvictim"
    done
    # the thread must still be beating after the loop
    echo "loadusr -w $(pwd)/beat-check.sh"
} > "$HAL"

set +e
timeout -k 5 $((N / 2 + 60)) halrun -f "$HAL"
RES=$?
set -e
if [ $RES -ne 0 ]; then
    echo "FAIL: halrun exited with $RES after delf/unloadrt in a running thread"
    exit 1
fi
echo "PASS: $N x loadrt/addf/delf/unloadrt with running thread"
