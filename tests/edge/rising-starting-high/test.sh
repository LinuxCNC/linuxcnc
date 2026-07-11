#!/bin/bash
# Converted to gomc resident-server model; see ../../hal-stream-driver.sh
. "$(dirname "$0")/../../hal-stream-driver.sh"
hal_start_server rising-starting-high.hal
hal_feed_streamer <<'DATA'
1
1
1
1
0
1
0
0
0
0
DATA
hal_sample 10
hal_run
