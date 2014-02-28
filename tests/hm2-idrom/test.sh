#!/bin/bash


Error[0]="hm2/hm2_test.0: invalid cookie"
Error[1]="hm2/hm2_test\.0: invalid config name"
Error[2]="hm2/hm2_test\.0: invalid IDROM type"
Error[3]="hm2/hm2_test\.0: invalid IDROM type"
Error[4]="hm2/hm2_test\.0: invalid IDROM PortWidth"
Error[5]="hm2/hm2_test\.0: invalid IDROM PortWidth"
Error[6]="hm2/hm2_test\.0: IDROM IOPorts is 0 but llio num_ioport_connectors is [[:digit:]]+"
Error[7]="hm2/hm2_test\.0: IDROM IOWidth is [[:digit:]]+, but IDROM IOPorts is [[:digit:]]+ and IDROM PortWidth is [[:digit:]]+"
Error[8]="hm2/hm2_test\.0: IDROM IOPorts is [[:digit:]]+ but llio num_ioport_connectors is [[:digit:]]+"
Error[9]="hm2/hm2_test\.0: IDROM ClockLow is [[:digit:]]+, that's too low"
Error[10]="hm2/hm2_test\.0: IDROM ClockHigh is [[:digit:]]+, that's too low"
Error[11]="hm2/hm2_test\.0: pin 0 primary tag is 0 \(end-of-list sentinel\), expected 144!"
Error[12]="hm2/hm2_test\.0: hm2_set_pin_direction: invalid pin number 0"
Error[13]="hm2/hm2_test\.0: invalid IDROM PortWidth 24, this board has 5 pins per connector, aborting load"
Error[14]="hm2/hm2_test\.0: IDROM IOPorts is 0 but llio num_ioport_connectors is [[:digit:]]+"


# obtain one logfile per test, and search only that for the pattern

realtime stop

LOG=realtime.log
retval=0

# clean up after previous runs
ls ${LOG}.* >&/dev/null && rm -f ${LOG}.*

TEST_PATTERN=0
while [ ! -z "${Error[$TEST_PATTERN]}" ]; do

    MSGD_OPTS="--stderr" DEBUG=5 realtime start  >$LOG 2>&1

    halcmd loadrt hostmot2 >/dev/null 2>&1
    halcmd loadrt hm2_test test_pattern=$TEST_PATTERN >/dev/null 2>&1

    # NB: one test actually _does_ load successfully!
    halcmd unloadrt hm2_test  >/dev/null 2>&1
    halcmd unloadrt hostmot2  >/dev/null 2>&1

    realtime stop

    if ! egrep -qe "${Error[$TEST_PATTERN]}" ${LOG}; then
	echo failed test $TEST_PATTERN: pattern not found:
	echo     "${Error[$TEST_PATTERN]}"
	cp $LOG ${LOG}.$TEST_PATTERN # save logs from errored tests
	retval=1
    fi
    TEST_PATTERN=$(($TEST_PATTERN+1))
done
rm -f $LOG

# if there's a failure, dump the log to stdout for debugging in buildbot
if test $retval = 1; then
    for f in ${LOG}.*; do
	echo "************************************************************"
	echo "Failed test logfile $f:"
	cat $f
	echo
    done
fi

exit $retval