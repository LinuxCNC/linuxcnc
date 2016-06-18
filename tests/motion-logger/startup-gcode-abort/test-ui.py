#!/usr/bin/env python

import linuxcnc
import sys


#
# connect to LinuxCNC
#

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()


#
# Immediately abort!  Github Issue #49 
#

print "UI abort"
sys.stdout.flush()

c.abort()
c.wait_complete()

print "UI done with abort"
sys.stdout.flush()

