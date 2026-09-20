#!/bin/bash
# Shared launcher for the halui pin tests. Each test directory's test.sh
# execs this from inside that directory.
#
# task rewrites the tool table (G10 L1, tool changes), so every run starts
# from a fresh copy of the shared one; tool.tbl is ignored by git.
cp ../_lib/tool.tbl tool.tbl
exec linuxcnc -r halui.ini
