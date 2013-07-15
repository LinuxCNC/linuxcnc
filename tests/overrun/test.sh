#!/bin/sh
TMPDIR=`mktemp -d /tmp/overrun.XXXXXX`
trap "rm -rf $TMPDIR" 0 1 2 3 9 15

TEST_HAL=$TMPDIR/test.hal
echo loadusr -w echo overrun > $TMPDIR/test.hal

realtime stop

! runtests $TMPDIR 2>&1

realtime start
