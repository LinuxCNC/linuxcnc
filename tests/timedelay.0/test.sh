#!/bin/bash
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server timedelay.hal
hal_sample 64 -t
hal_run
