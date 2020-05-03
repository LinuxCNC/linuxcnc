#!/bin/bash

realtime start
linuxcnc-python hallink.py

realtime stop

exit $?
