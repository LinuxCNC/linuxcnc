#!/bin/bash
# Round-trip check of `halcmd save`: load a config into a resident gomc-server and
# dump it back. (Classic used $REALTIME start + halcmd -f; gomc-server is the RT.)
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server setup.hal
halcmd save
