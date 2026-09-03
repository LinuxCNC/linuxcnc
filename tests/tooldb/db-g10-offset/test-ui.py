#!/usr/bin/env python3

# With an [EMCIO]DB_PROGRAM configured and a nonrandom toolchanger, no tool
# table file is written, so G10 L1/L10/L11 offsets are persisted only if the
# db program is notified ('p' command).  Each G10 variant gets its own tool so
# that the values can be checked again after a restart (verify-ui.py).

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

# unload the tool: the G10 L10/L11 checks below expect an empty spindle
c.mdi("T0 M6")
c.wait_complete()

# 3) G10 L10 computes the offset from the current position in the active
#    coordinate system.  Z ends up at 2.0 with no offsets in effect, so
#    asking for Z0.5 stores an offset of 2.0 - 0.5.
c.mdi("G0 Z2")
c.wait_complete()

c.mdi("G10 L10 P4 Z0.5")
c.wait_complete()

check("tool 4 zoffset after G10 L10", tool_table_zoffset(s, 4), 1.5)

got = db_zoffset(4, timeout=5)
if got is None:
    fail("db program was not notified of the G10 L10 offset for tool 4")
check("db zoffset for tool 4 after G10 L10", got, 1.5)

# 4) G10 L11 computes the offset in the G59.3 fixture system instead, so
#    shifting the active system (G54) must not change the result: Z is still
#    2.0 in G59.3, and asking for Z0.25 stores 2.0 - 0.25 (not 1.0 - 0.25).
c.mdi("G10 L2 P1 Z1")
c.wait_complete()

c.mdi("G10 L11 P5 Z0.25")
c.wait_complete()

check("tool 5 zoffset after G10 L11", tool_table_zoffset(s, 5), 1.75)

got = db_zoffset(5, timeout=5)
if got is None:
    fail("db program was not notified of the G10 L11 offset for tool 5")
check("db zoffset for tool 5 after G10 L11", got, 1.75)

# leave the G54 offset as it was found
c.mdi("G10 L2 P1 Z0")
c.wait_complete()

sys.exit(0)
