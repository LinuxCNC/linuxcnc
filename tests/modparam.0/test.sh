#!/bin/bash
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server modparam.0.hal
halcmd list param '*maxaccel'
halcmd list pin '*maxaccel'
