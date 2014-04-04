#!/bin/bash

realtime start
python hallink.py

realtime stop

exit $?
