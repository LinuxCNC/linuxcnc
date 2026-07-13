#!/bin/bash
# flipflop driven via the filestream cmod (was WS streamer+sampler).
. "$(dirname "$0")/../filestream-driver.sh"
cat > in.txt <<DATA
1 1 0 0
1 0 0 0
0 1 0 0
0 0 0 0
1 1 0 0
0 1 0 0
0 0 1 0
1 1 1 0
0 1 1 1
0 1 0 1
DATA
fs_run source.hal
