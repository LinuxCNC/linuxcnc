#!/bin/bash
set -e

# A name that is exported under a different HAL name must still compile.
rm -f halname_mangled.c
halcompile --preprocess halname_mangled.comp 2>&1
test -f halname_mangled.c || echo 'halcompile failed to produce halname_mangled.c'

# Two declarations that mangle to one HAL name must be rejected, not left to
# fail at loadrt as "HAL: ERROR: duplicate variable".
rm -f halname_collision.c
if halcompile --preprocess halname_collision.comp 2>&1; then
    echo 'halcompile erroneously accepted halname_collision.comp'
fi
test ! -f halname_collision.c || echo 'halcompile erroneously produced halname_collision.c'
