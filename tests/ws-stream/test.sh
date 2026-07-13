#!/bin/bash
# Exercise the WebSocket sampler/streamer path end-to-end via the halstreamer /
# halsampler clients (kept as coverage after the file-based tests migrated to
# filestream).  Feed 5 mixed-type samples through a streamer->sampler loopback.
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server ws-stream.hal
hal_feed_streamer <<DATA
1.5 1 100 -5
2.25 0 200 -10
-3.5 1 4294967295 2147483647
0 0 0 -2147483648
42 1 7 7
DATA
hal_sample 5
hal_run
