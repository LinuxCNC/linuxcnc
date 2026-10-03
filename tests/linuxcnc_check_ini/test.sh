#!/bin/bash
# Check the exit value of linuxcnc_check_ini with and without -e/--error on
# an INI file that only produces warnings.

run() {
    msgs=$(linuxcnc_check_ini "$@" 2>&1 > /dev/null)
    echo "$* -> $?"
    echo "$msgs" | grep -c ": warning: "
}

run warn.ini
run -e warn.ini
run --error warn.ini
run -h
exit 0
