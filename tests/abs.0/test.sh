#!/bin/bash
# Drive a resident gomc-server to feed abs through a streamer/sampler.
# See ../hal-stream-driver.sh for the mechanism (gomc has no userspace comps).
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server abs.hal
hal_feed_streamer <<DATA
0
0.25
-0.25
1
-1
64
-64
DATA
hal_sample 7
hal_run
