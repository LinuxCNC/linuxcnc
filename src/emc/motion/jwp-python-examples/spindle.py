# verify spindle commands work during pause
import time
import linuxcnc
c = linuxcnc.command()

c.spindle(linuxcnc.SPINDLE_OFF)
c.brake(linuxcnc.BRAKE_ENGAGE)
time.sleep(2)
c.brake(linuxcnc.BRAKE_RELEASE)
c.spindle(linuxcnc.SPINDLE_REVERSE)
c.spindle(linuxcnc.SPINDLE_INCREASE)
c.spindle(linuxcnc.SPINDLE_INCREASE)

