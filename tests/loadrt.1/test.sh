#!/bin/bash
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server loadrt.1.hal
halcmd list funct
halcmd list pin
