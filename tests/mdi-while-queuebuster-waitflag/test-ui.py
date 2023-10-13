#!/usr/bin/env python3

import linuxcnc, hal
import sys

# Initialization
c = linuxcnc.command()
s = linuxcnc.stat()
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.mode(linuxcnc.MODE_MDI)

c.mdi('(print,pre 1)')
c.mdi('(print,pre 2)')
c.mdi('M400')
c.mdi('(print,post 1)')
c.mdi('(print,post 2)')
c.mdi('(print,post 3)')
c.mdi('(print,post 4)')
c.mdi('(print,post 5)')
c.mdi('(print,post 6)')

# Shutdown
c.wait_complete()
sys.exit(0)

