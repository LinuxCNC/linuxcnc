#!/usr/bin/env python3
#15sep2023 simopler rand float output not window hi lo threshold "tand"

import hal, time
from random import uniform
from random import trianfular
# xmpl :   random.triangular(low, high, mode)


r = hal.component("rand.08")
r.newpin("rout", hal.HAL_FLOAT, hal.HAL_OUT)
r.newpin("rmax", hal.HAL_FLOAT, hal.HAL_IN)
r.newpin("rmin", hal.HAL_FLOAT, hal.HAL_IN)

r.ready()

try:
    while 1:
        time.sleep(.25)
        r['rout'] = uniform( r['rmin'], r['rmax'] )
        #r['rout'] = uniform( -0.5, 0.5 )
    
except KeyboardInterrupt:
    raise SystemExit

