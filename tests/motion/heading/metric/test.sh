#!/bin/bash
# Run the Metric config
if linuxcnc -r heading-mm.ini; then
    echo "Completed successfully" > result 
fi


