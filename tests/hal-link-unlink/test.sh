#!/bin/bash

$REALTIME start
python3 hallink.py

$REALTIME stop

exit $?
