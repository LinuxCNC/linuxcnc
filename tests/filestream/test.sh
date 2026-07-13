#!/bin/bash
# Prove the filestream cmod: replay 5 mixed-type samples through a loopback and
# capture them; the captured file (printed by fs_run) is the test result.
. "$(dirname "$0")/../filestream-driver.sh"

cat > in.txt <<'DATA'
# comment line (must be skipped)
1.5 1 100 -5

2.25 0 200 -10
-3.5 1 4294967295 2147483647
0 0 0 -2147483648
42 1 7 7
DATA

fs_run loopback.hal
