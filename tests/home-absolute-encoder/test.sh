#!/bin/bash

# Seed the position file with a stale position that has nothing to do
# with the absolute encoder value.
rm -f position.txt
for i in $(seq 16); do echo 3.0; done > position.txt

# First run starts from the seeded position file.
linuxcnc -r test.ini || exit 1

# Second run starts from the position saved by the first run.
linuxcnc -r test.ini
