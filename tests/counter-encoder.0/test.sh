#!/bin/bash
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server counter-encoder.hal
hal_sample 3500
hal_run
