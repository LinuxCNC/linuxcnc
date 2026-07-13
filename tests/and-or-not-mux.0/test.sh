#!/bin/bash
. "$(dirname "$0")/../filestream-driver.sh"
cat > in.txt <<'DATA'
0 0 0 0
0 1 0 0
1 1 0 0
1 0 0 0
0 0 1 0
0 1 1 0
1 1 1 0
1 0 1 0
0 0 0 1
0 1 0 1
1 1 0 1
1 0 0 1
0 0 1 1
0 1 1 1
1 1 1 1
1 0 1 1
DATA
fs_run and-or-not-mux.hal
