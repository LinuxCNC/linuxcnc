#!/bin/bash

result=0

#
# Test pattern 15 uses a memory dump from a 7i96 as hostmod2 variables.
# This will get replaced using hm_test2_usr for any mesa firmware variables.
#
TEST_PATTERN=15
export TEST_PATTERN
#
# Convert each mbcc source file to binary.
#
for MBCCS in $(ls *.mbccs)
do
    MBCCB=$(echo "$MBCCS" | sed 's/mbccs$/mbccb/')
    mesambccc -o $MBCCB $MBCCS
done

# First test normal baudrates.
# test0 <mesamodbus baudrate="1200" parity="E" stopbits="1" drivedelay="2" duplex="half" suspend="false" >
# test1 <mesamodbus baudrate="19200" parity="O" stopbits="1" duplex="half" suspend="false" >
# test2 <mesamodbus baudrate="38400" parity="N" stopbits="2" duplex="half" suspend="false" >
# test3 <mesamodbus baudrate="115200" parity="E" stopbits="1" duplex="half" suspend="false" >
# test4 <mesamodbus baudrate="1000000" parity="E" stopbits="1" duplex="half" suspend="false" >
#
# Override icdelay.
#
# test5 
#
echo "hm2_modbus delay test." >> halrun-stdout 
#
# Run HAL with each test .mbccb
#
for MBCCB in $(ls *.mbccb)
do
    echo 
    echo "Processing file: $MBCCB" > halrun-stdout
    TEST_MBCCB=$MBCCB
    export TEST_MBCCB

    halrun -f modbus_test.hal >halrun-stdout 2>halrun-stderr

    arr=($(grep "hm2_modbus.0.baudrate" halrun-stdout))
    baudrate=$(echo "${arr[3]}" | sed 's/0x0\+/0/') 
    baudrate=$((16#$baudrate))
    echo "    baudrate = $baudrate" 

    arr=($(grep "hm2_modbus.0.drivedelay" halrun-stdout))
    drivedelay=$(echo "${arr[3]}" | sed 's/0x0\+/0/') 
    drivedelay=$((16#$drivedelay))
    echo "    drivedelay = $drivedelay" 

    arr=($(grep "hm2_modbus.0.icdelay" halrun-stdout))
    icdelay=$(echo "${arr[3]}" | sed 's/0x0\+/0/') 
    icdelay=$((16#$icdelay))
    echo "    icdelay = $icdelay" 

    arr=($(grep "hm2_modbus.0.parity" halrun-stdout))
    parity=$(echo "${arr[3]}" | sed 's/0x0\+/0/') 
    parity=$((16#$parity))
    echo "    parity = $parity" 

    arr=($(grep "hm2_modbus.0.rxdelay" halrun-stdout))
    rxdelay=$(echo "${arr[3]}" | sed 's/0x0\+/0/') 
    rxdelay=$((16#$rxdelay))
    echo "    rxdelay = $rxdelay" 

    arr=($(grep "hm2_modbus.0.stopbits" halrun-stdout))
    stopbits=$(echo "${arr[3]}" | sed 's/0x0\+/0/') 
    stopbits=$((16#$stopbits))
    echo "    stopbits = $stopbits" 

    arr=($(grep "hm2_modbus.0.txdelay" halrun-stdout))
    txdelay=$(echo "${arr[3]}" | sed 's/0x0\+/0/') 
    txdelay=$((16#$txdelay))
    echo "    txdelay = $txdelay" 

    #
    # Now test values (alternate calculation as independent check).
    #    
    if [ $baudrate -gt 19200 ]; then
        #
        # Calculate based on bits within 1750usec at this baudrate.
        #
        let "if_delay_predicted = ($baudrate * 7 + 3999)/4000"
#        echo "1750usec worth of bits." > halrun-stdout
        #
        # hm2_modbus enforces obscure pktuart limitations inside the calc_if()
        #
        if [ $if_delay_predicted -gt 1120 ]; then 
            echo "Enforce pktuart limitation at the modbus level." > halrun-stdout
            let "if_delay_predicted = 1020"
        fi
    else
        #
        # Calculate based on bpc.
        #
        if [ $parity -eq 0 ]; then
            # Parity - None
            let "bpc = (1 + 8) + $stopbits"
        else
            # Parity - Even or odd has one bit.
            let "bpc = (1 + 8 + 1) + $stopbits"
        fi
        let "if_delay_predicted = (bpc * 7 + 1)/2"
#        echo "3.5 chars worth of bits." > halrun-stdout
    fi

    let "delay_predicted = $if_delay_predicted - 1"
    if [ $delay_predicted -ne $rxdelay ]; then
        echo "ERROR rxdelay! predicted $delay_predicted vs $rxdelay" 
        let "result = $result + 1"
    fi
    let "delay_predicted = $if_delay_predicted + 1"
    if [ $delay_predicted -ne $txdelay ]; then
        echo "ERROR txdelay! predicted $delay_predicted vs $txdelay" 
        let "result = $result + 1"
    fi
done

echo "Number of errors $result"

exit $result
