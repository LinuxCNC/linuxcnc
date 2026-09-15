#!/bin/bash

#
# This script will dump the current configuration memory to the command line.
# Use ./dump_config.sh >> test_pattern15.h to capture your configuration to a file. Replace 7i96 with your firmware name.

# Want to use --rpo from 0x0000 - 0x6100. 
#
ADDR=0
#END=0x6100 or decimal 24832

# format output in columns.
COLUMN=0 

echo "/*"
echo "  Header file containing configuration memory dump."
echo "  You may use tests/hm-test/dump_config.sh to create this file."
echo "*/"
echo ""
echo "#pragma once"
echo ""
echo "const uint32_t config_memory_dump[] = {"

while [ $ADDR -lt 24832 ] 
do
    echo -n "0x"

    mesaflash  --addr 10.10.10.10 --device 7i96 --rpo $ADDR  | tr "\n" ", "

    if [ $COLUMN -lt 7 ]; then
        COLUMN=$(( $COLUMN + 1 ))
    else 
        COLUMN=0
        echo ""
    fi

    ADDR=$(( $ADDR + 4 ))
done

echo "};"