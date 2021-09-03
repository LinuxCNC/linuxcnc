#!/usr/bin/env python3

import linuxcnc
import sys
import time


#
# connect to LinuxCNC
#

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


#
# Immediately abort!  Github Issue #49 
#

time.sleep(1)

print("UI abort")
sys.stdout.flush()

c.abort()
c.wait_complete()

print("UI done with abort")
sys.stdout.flush()

