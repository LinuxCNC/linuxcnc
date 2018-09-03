#!/bin/bash

realtime start
python2 hallink.py

realtime stop

exit $?
