#!/bin/bash

$REALTIME start
linuxcnc-python hallink.py

$REALTIME stop

exit $?
