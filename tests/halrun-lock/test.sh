#!/bin/sh

# Assert the lock STATE (the "current lock value N (hex)" protocol lines and
# which HAL_LOCK_* flags are set), not halcmd's human-readable descriptions.
# Stripping the "- loading of new components is locked" prose and its variable
# column spacing keeps the test from breaking on wording/formatting changes that
# carry no behavioural signal.
halrun -f halrun.hal | grep -i lock | sed -E 's/^(  HAL_LOCK_[A-Z]+).*/\1/'

