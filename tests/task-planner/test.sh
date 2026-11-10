#!/bin/bash

# The probe helper is built with the run-in-place tree only.
if ! command -v test_task_probe > /dev/null; then
    echo "test_task_probe not found, skipping (run-in-place builds only)"
    exit 0
fi

cp -f tool.tbl.original tool.tbl

# A program long enough that the interpreter is still reading while the
# test probes: tiny moves, far more of them than task reads ahead.
{
    echo "G1 F1"
    for i in $(seq 5000); do
        echo "X0.001"
        echo "X0"
    done
    echo "M2"
} > reading.ngc

exec linuxcnc -r task-planner.ini
