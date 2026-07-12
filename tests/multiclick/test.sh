#!/bin/bash
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server multiclick.hal
grep -vE '^[[:space:]]*#|^[[:space:]]*$' input-signals | hal_feed_streamer
hal_sample 105
hal_run
