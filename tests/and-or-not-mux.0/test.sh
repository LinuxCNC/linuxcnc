#!/bin/bash
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server and-or-not-mux.hal
hal_feed_streamer <<'DATA'
0 0 0 0
0 1 0 0
1 1 0 0
1 0 0 0
0 0 1 0
0 1 1 0
1 1 1 0
1 0 1 0
0 0 0 1
0 1 0 1
1 1 0 1
1 0 0 1
0 0 1 1
0 1 1 1
1 1 1 1
1 0 1 1
DATA
hal_sample 16
hal_run
