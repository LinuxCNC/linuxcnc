#!/bin/bash
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server alias.hal
halcmd list pin
