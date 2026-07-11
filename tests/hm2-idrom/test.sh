#!/bin/bash

# gomc port: hm2_test is a userspace cmod that logs IDROM validation errors to
# gomc-server's stderr (not the kernel dmesg), and gomc's one-shot halrun does
# not substitute $(TEST_PATTERN) from the environment — so we generate a HAL
# file per pattern with a literal test_pattern=N, capture stderr, and grep it
# for the expected message.  gomc logs the message body without the classic
# "hm2/hm2_test.0: " prefix (that is a separate component= tag), so we strip the
# prefix before grepping but print the full pattern to match ./expected.

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
Error[11]="hm2/hm2_test\.0: pin 0 primary tag is 0 \(end-of-list sentinel\), expected 144 pins!"
Error[12]="hm2/hm2_test\.0: [^:]+: invalid pin number 0"
Error[13]="hm2/hm2_test\.0: invalid IDROM PortWidth 24, this board has 5 pins per connector, aborting load"
Error[14]="hm2/hm2_test\.0: IDROM IOPorts is 0 but llio num_ioport_connectors is \d+"

result=0
TEST_PATTERN=0
while [ ! -z "${Error[$TEST_PATTERN]}" ]; do
    printf 'loadrt hostmot2 debug_idrom=1 debug_pin_descriptors=1 debug_module_descriptors=1\nloadrt hm2_test test_pattern=%s\n' "$TEST_PATTERN" > gen-load-test.hal
    halrun -f gen-load-test.hal >halrun-stdout 2>halrun-stderr
    body=$(printf '%s' "${Error[$TEST_PATTERN]}" | sed 's|^[^:]*: ||')
    if grep -qP "$body" halrun-stderr; then
        echo "${Error[$TEST_PATTERN]}"
    else
        echo "test pattern $TEST_PATTERN did not produce error '${Error[$TEST_PATTERN]}'" >&2
        cat halrun-stderr >&2
        result=1
    fi
    TEST_PATTERN=$(($TEST_PATTERN+1))
done
exit $result
