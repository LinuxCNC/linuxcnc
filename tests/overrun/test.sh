#!/bin/sh
TMPDIR=`mktemp -d /tmp/overrun.XXXXXX`
trap "rm -rf $TMPDIR" 0 1 2 3 9 15

TEST_HAL=$TMPDIR/test.hal
echo loadusr -w echo overrun > $TMPDIR/test.hal

! runtests $TMPDIR 2>&1
