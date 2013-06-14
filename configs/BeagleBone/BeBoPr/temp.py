#!/usr/bin/python

# This is a hack.  
# If you can't figure out how to get it to run, you shouldn't be using it
# Hints: 
# * What temperature is it really?  Think in terms of ADC units
# * The hal temp output is actually a heater control signal, not a temp.

import hal, time
import glob

h = hal.component("temp")
h.newpin("set", hal.HAL_U32, hal.HAL_IN)
h.newpin("adc", hal.HAL_U32, hal.HAL_OUT)
h.newpin("temp", hal.HAL_FLOAT, hal.HAL_OUT)
h.ready()
FileName = glob.glob ('/sys/devices/ocp.*/helper.*/AIN1')
try:
    Err = 0.0
    P = 0
    while 1:
	f = open(FileName[0], 'r')
        ADC_IN = int(f.readline())
        P = ADC_IN - h['set']
        Err =  ( P / 256.0 ) + 0.5
        h['temp'] = min( max( Err, 0.0 ), 1.0 )
        h['adc'] = ADC_IN
	f.close()
	time.sleep(0.050)
except KeyboardInterrupt:
    raise SystemExit

