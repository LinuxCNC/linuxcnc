# try flood commands
import time
import linuxcnc
c = linuxcnc.command()



c.flood(linuxcnc.FLOOD_ON)
time.sleep(2)
c.flood(linuxcnc.FLOOD_OFF)
time.sleep(2)
c.flood(linuxcnc.FLOOD_ON)
