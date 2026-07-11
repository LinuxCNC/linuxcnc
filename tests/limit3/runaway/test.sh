#!/bin/bash
. "$(dirname "$0")/../../hal-stream-driver.sh"
hal_start_server runaway.hal
hal_sample 1500 -t
hal_run
