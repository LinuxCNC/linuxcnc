#!/bin/bash

Error[0]="hm2/hm2_test\.0: invalid cookie"
Error[1]="hm2/hm2_test\.0: invalid config name"
Error[2]="hm2/hm2_test\.0: invalid IDROM type"
Error[3]="hm2/hm2_test\.0: invalid IDROM type"
Error[4]="hm2/hm2_test\.0: invalid IDROM PortWidth"
Error[5]="hm2/hm2_test\.0: invalid IDROM PortWidth"
Error[6]="hm2/hm2_test\.0: IDROM IOPorts is 0 but llio num_ioport_connectors is \d+"
Error[7]="hm2/hm2_test\.0: IDROM IOWidth is \d+, but IDROM IOPorts is \d+ and IDROM PortWidth is \d+"
Error[8]="hm2/hm2_test\.0: IDROM IOPorts is \d+ but llio num_ioport_connectors is \d+"
Error[9]="hm2/hm2_test\.0: IDROM ClockLow is \d+, that's too low"
Error[10]="hm2/hm2_test\.0: IDROM ClockHigh is \d+, that's too low"
Error[11]="hm2/hm2_test\.0: pin 0 primary tag is 0 \(end-of-list sentinel\), expected 144!"

TEST_PATTERN=0
while [ ! -z "${Error[$TEST_PATTERN]}" ]; do
    export TEST_PATTERN
    halrun -f broken-load-test.hal
    ./check-dmesg "${Error[$TEST_PATTERN]}"
    TEST_PATTERN=$(($TEST_PATTERN+1))
done

