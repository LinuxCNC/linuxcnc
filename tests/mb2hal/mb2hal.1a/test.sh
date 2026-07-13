#!/bin/bash
# XFAIL: the classic test compared mb2hal's INI-DEBUG dump on stdout (each parsed
# [MB2HAL_INIT]/[TRANSACTION] key printed as "mb2hal ... DEBUG: [SECTION] [KEY]
# [VALUE]").  In gomc mb2hal is a cmod whose diagnostics go to the server log via
# slog in a different form ("parse_transaction_section N OK", not the [KEY] [VALUE]
# dump), so the classic `expected` cannot be reproduced.  mb2hal itself loads and
# creates its pins fine — that is covered by the 1b/2b variants.  This runs mb2hal
# and surfaces what it logged, which does not match the classic dump.
rm -f server.log
gomc-server -r -f mb2hal.hal >server.log 2>&1 &
SRV=$!
trap '[ -n "$SRV" ] && kill $SRV 2>/dev/null; wait 2>/dev/null' EXIT
for i in $(seq 50); do halcmd show comp 2>/dev/null | grep -q mb2hal && break; sleep 0.1; done
sleep 0.3
grep "component=mb2hal" server.log | sed -E 's/^time=[^ ]+ level=[A-Z]+ //'
