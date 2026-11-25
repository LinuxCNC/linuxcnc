#!/bin/bash
set -e

halcompile --install servo_delay.comp >/dev/null
linuxcnc -r test.ini
