#! /usr/bin/bash

set -e

xvfb-run halrun do-test.hal &

exit 0
