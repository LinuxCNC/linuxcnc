#!/bin/bash -e
linuxcnc -r test.ini
if grep -q '^[^+].*Segmentation fault' stderr; then
    exit 1
fi
