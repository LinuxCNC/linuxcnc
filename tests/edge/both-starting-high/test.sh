#!/bin/bash
# Converted to gomc resident-server model; see ../../filestream-driver.sh
. "$(dirname "$0")/../../filestream-driver.sh"
cat > in.txt <<'DATA'
1
1
1
1
0
0
1
0
0
0
DATA
fs_run both-starting-high.hal
