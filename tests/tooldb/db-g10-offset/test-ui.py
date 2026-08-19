#!/usr/bin/env python3

# With an [EMCIO]DB_PROGRAM configured and a nonrandom toolchanger, no tool
# table file is written, so G10 L1/L10/L11 offsets are persisted only if the
# db program is notified ('p' command).

import sys

from dbtest_lib import start_machine, check, fail, tool_table_zoffset, db_zoffset

(c, s) = start_machine()

# initial values come from the db program
check("tool 3 zoffset at startup", tool_table_zoffset(s, 3), 2.5)
check("db zoffset for tool 3 at startup", db_zoffset(3), 2.5)

# 1) G10 L1 on a tool that is not in the spindle
c.mdi("G10 L1 P3 Z-12.345")
c.wait_complete()

check("tool 3 zoffset after G10 L1", tool_table_zoffset(s, 3), -12.345)

got = db_zoffset(3, timeout=5)
if got is None:
    fail("db program was not notified of the G10 L1 offset for tool 3")
check("db zoffset for tool 3 after G10 L1", got, -12.345)

# 2) G10 L1 on the tool that is in the spindle
c.mdi("T2 M6")
c.wait_complete()
s.poll()
if s.tool_in_spindle != 2:
    fail("expected tool 2 in spindle, got %d" % s.tool_in_spindle)

c.mdi("G10 L1 P2 Z-6.789")
c.wait_complete()

check("tool 2 zoffset after G10 L1", tool_table_zoffset(s, 2), -6.789)
check("spindle zoffset after G10 L1", tool_table_zoffset(s, 0), -6.789)

got = db_zoffset(2, timeout=5)
if got is None:
    fail("db program was not notified of the G10 L1 offset for tool 2")
check("db zoffset for tool 2 after G10 L1", got, -6.789)

# unload the tool so the db program sees a clean shutdown
c.mdi("T0 M6")
c.wait_complete()

sys.exit(0)
