#!/bin/bash
. "$(dirname "$0")/../../hal-stream-driver.sh"
hal_start_server limit-max-velocity.hal
hal_sample 800 -t
hal_run
