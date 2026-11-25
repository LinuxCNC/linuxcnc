#!/bin/bash
# Run the Imperial config
if linuxcnc -r heading-in.ini; then
    echo "Completed successfully" > result 
fi

