#!/bin/bash
. "$(dirname "$0")/../../hal-stream-driver.sh"
hal_start_server min-max-overshoot.hal
hal_sample 800 -t
hal_run
