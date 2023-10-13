#!/bin/bash -e

rm -f out.motion-logger

linuxcnc -r mountaindew.ini
