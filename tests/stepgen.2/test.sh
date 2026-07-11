#!/bin/bash
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server stepgen.hal
hal_sample 3500
hal_run
