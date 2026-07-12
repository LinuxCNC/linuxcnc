#!/bin/bash
. "$(dirname "$0")/../../hal-stream-driver.sh"
hal_start_server constraints.hal
hal_sample 5100 -t
hal_run
