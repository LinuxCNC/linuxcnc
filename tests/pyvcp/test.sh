#! /usr/bin/bash

set -e

xvfb-run halrun -s do-test.hal &

exit 0
