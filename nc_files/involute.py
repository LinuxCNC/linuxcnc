from math import *

print "G20 F60"
print "G64 P0.001"
print "G0 X0 Y0 Z0"
a=.1
for i in range(100):
    t = i/10.
    x = a * (cos(t) + t * sin(t))
    y = a * (sin(t) - t * cos(t))
    print "G1 X%f Y%f" % (x,y)
print "M2"

