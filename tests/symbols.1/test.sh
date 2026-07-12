#!/bin/bash
# Symbol export test: test_use1 must resolve a symbol exported by test_define1.
# Drive a resident gomc-server, run one servo cycle's worth, and read the two
# outputs (checkresult expects define.out nonzero, use.out zero).
. "$(dirname "$0")/../hal-stream-driver.sh"
hal_start_server dotest.hal
halcmd start
sleep 1
halcmd getp test_define1.out | awk '{print $NF}'
halcmd getp test_use1.out | awk '{print $NF}'
