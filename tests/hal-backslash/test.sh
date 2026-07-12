#!/bin/bash
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server backslash.hal
halcmd show sig
