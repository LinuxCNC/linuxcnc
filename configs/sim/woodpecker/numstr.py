#!/usr/bin/python
import hal
import time 
h = hal.component("numstr")
h.newpin("in", hal.HAL_S32, hal.HAL_IN)
h.ready()
tmp = 0
try:
    while 1:
       # check program data every 0.01 seconds
       time.sleep(0.01)
       if h['in'] == 0:
          continue
       else:
         if h['in'] > tmp:
            f = open('/home/jura/line-number.txt','w')
            f.write(str(int(h['in'])))
            f.close()
            tmp = h['in']
except KeyboardInterrupt:
    raise SystemExit
