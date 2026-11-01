#!/bin/bash
set -e
halcompile --install msgline.comp >/dev/null
timeout -k 5 60 halrun -f msgline.hal > halrun.out 2> halrun.err
# the messages of msgline, each on a line of its own, then rtapi_print()
grep '^msgline:' halrun.err
grep '^msgline:' halrun.out
