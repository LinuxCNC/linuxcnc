#!/bin/bash
set -e

halcompile --install hexfwdcheck.c >/dev/null

# both run, so a failure in one does not hide the other
status=0
halrun -f hexfwdcheck.hal || status=1
halrun -f hexfwdcheck-screw.hal || status=1
exit $status
