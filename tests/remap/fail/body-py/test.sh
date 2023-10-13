#!/bin/bash
export PYTHONUNBUFFERED=1
! rs274 -i test.ini -n 2  -g test.ngc 2>&1
