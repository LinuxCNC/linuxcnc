#!/bin/bash
if linuxcnc -r test.ini; then
    echo "Completed successfully" > result
fi
