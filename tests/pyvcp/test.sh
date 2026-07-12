#!/bin/bash
# Verify the pyvcp XML -> HAL-pin mapping: load the panel and dump its pins.
# LC_ALL=C for a stable, locale-independent sort order.
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server do-test.hal
halcmd show pin | grep mypanel | LC_ALL=C sort
