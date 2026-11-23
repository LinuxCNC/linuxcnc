#!/bin/sh
# An exception raised by a canon callback must reach gcode.parse's caller
# unchanged, not be replaced by a generic RuntimeError (issue #2492).
set -e
exec python3 ./test_errors.py
