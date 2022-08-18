#!/bin/bash

$REALTIME start
python3 hallink.py

exec $REALTIME stop
