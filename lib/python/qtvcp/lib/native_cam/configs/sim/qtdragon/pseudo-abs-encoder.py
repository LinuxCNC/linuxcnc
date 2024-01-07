#!/usr/bin/env python3

import hal, time
import sys, os, math

num_chans = 9
for s in sys.argv:
    p = s.split('=')
    if p[0] == 'num_chans': num_chans=int(p[1])
    
h = hal.component("pseudo_abs_encoder")

for i in range(0,num_chans):
    h.newpin('encoder-position-%i' % i, hal.HAL_FLOAT, hal.HAL_IN)
    h['encoder-position-%i' % i] = -1e99
    h.newpin('offset-%i' % i, hal.HAL_FLOAT, hal.HAL_OUT)
    h['offset-%i' % i] = 0
    h.newpin('res-offset-%i' % i, hal.HAL_FLOAT, hal.HAL_IN)
    h['res-offset-%i' % i] = 0
    h.newpin('scale-%i' % i, hal.HAL_FLOAT, hal.HAL_IN)
    h['scale-%i' % i] = 1.0
    
h.newpin('sleep_time', hal.HAL_FLOAT, hal.HAL_OUT)
    
old = [0.0] * num_chans
flags = [False] * num_chans
try:
    f = open('pseudo_abs_encoder.var', 'r')
    for i in range(0,num_chans):
        v = f.readline()
        old[i] = float(v)
    f.close()
    # Guarantee file freshness
    #os.remove(f.name)
except IOError:
    print("position file not found. set HOME_ABSOLUTE_ENCODER to zero and home normally")

h.ready()

try:
    while 1:
        sleep_time = 3600
        for i in range(0, num_chans):
            if flags[i] == False: sleep_time = 0.1
            s = 1.0 if h['scale-%i' % i] == 0 else h['scale-%i' % i]
            p = h['encoder-position-%i' % i]
            a, b = math.modf(p / s)
            c, d = math.modf(old[i] / s)
            
            if p > -1e98 and flags[i] == False:
                # transform new position
                if a > 0.5: #3th-4th quad
                    a = a - 1
                    b = b + 1
                elif a < -0.5: #1th-2th quad
                    a = 1 + a
                    b = b - 1
                # tranform old position
                if c > 0.5: #3th-4th quad
                    c = c - 1
                    d = d + 1
                elif c < -0.5: #1th-2th quad
                    c = 1 + c
                    d = d - 1
                # correct the n turns
                n = d - b
                if c - a > 0.5:
                    n = n + 1
                elif c - a < -0.5:
                    n = n - 1
                # set the offset
                h['offset-%i' % i] = s * n - h['res-offset-%i' % i]

                flags[i] = True
        h.sleep_time = sleep_time
        time.sleep(sleep_time)
        
except KeyboardInterrupt:
    print('writing file')
    f = open('pseudo_abs_encoder.var', 'w')
    for i in range(0, num_chans):
        p = h['encoder-position-%i' % i]
        o = h['offset-%i' % i]
        if flags[i]:
            f.write('%0.6f\n' % (o + p))
        else:
            f.write('0.0\n')
    f.close()
    raise SystemExit
