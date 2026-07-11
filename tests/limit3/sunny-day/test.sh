#!/bin/bash
. "$(dirname "$0")/../../hal-stream-driver.sh"
hal_start_server sunny-day.hal
hal_sample 4000 -t
hal_run
