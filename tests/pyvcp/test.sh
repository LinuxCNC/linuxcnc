#! /usr/bin/bash

set -e

xvfb-run halrun -s do-test.hal
sed -i '/mypanel/!d' result

exit 0
