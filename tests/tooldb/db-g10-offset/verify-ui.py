#!/usr/bin/env python3

# Second run: the tool data now comes from the db program, and must carry the
# offsets written by test-ui.py in the previous run.

import sys

from dbtest_lib import start_machine, check, tool_table_zoffset

(c, s) = start_machine()

check("tool 3 zoffset after restart", tool_table_zoffset(s, 3), -12.345)
check("tool 2 zoffset after restart", tool_table_zoffset(s, 2), -6.789)
check("tool 1 zoffset after restart", tool_table_zoffset(s, 1), 0.5)

sys.exit(0)
