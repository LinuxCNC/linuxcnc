#!/bin/bash
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server flipflop.hal
hal_feed_streamer <<'DATA'
1 1 0 0
1 0 0 0
0 1 0 0
0 0 0 0
1 1 0 0
0 1 0 0
0 0 1 0
1 1 1 0
0 1 1 1
0 1 0 1
DATA
hal_sample 10
hal_run
