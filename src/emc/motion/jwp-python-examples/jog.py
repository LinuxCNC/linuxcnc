import time
import linuxcnc
c = linuxcnc.command()

c.jog(linuxcnc.JOG_INCREMENT, 0, 1, 0.1)
time.sleep(1)
c.jog(linuxcnc.JOG_INCREMENT, 0, 1, -0.1)
time.sleep(1)

c.jog(linuxcnc.JOG_CONTINUOUS, 0, 1)
time.sleep(2)

c.jog(linuxcnc.JOG_STOP, 0)

c.jog(linuxcnc.JOG_CONTINUOUS, 0, -1)
time.sleep(2)
c.jog(linuxcnc.JOG_STOP, 0)
