#!/bin/bash

cp -f tool.tbl.original tool.tbl

# A program long enough that the interpreter is still reading while the
# test probes: tiny moves, far more of them than task reads ahead.
{
    echo "G1 F1"
    for _ in $(seq 5000); do
        echo "X0.001"
        echo "X0"
    done
    echo "M2"
} > reading.ngc

exec linuxcnc -r task-planner.ini
