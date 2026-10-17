#!/bin/bash
set -e
halcompile --install msgline.comp >/dev/null
timeout -k 5 60 halrun -f msgline.hal > halrun.out 2>&1
# the messages of msgline, each on a line of its own
grep '^msgline:' halrun.out
