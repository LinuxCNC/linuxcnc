#!/bin/bash -e
rm -f motion-samples.log

linuxcnc -r test.ini
