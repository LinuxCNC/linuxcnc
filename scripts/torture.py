"""
This program creates "torture test" ngc files.  These include arcs (helices)
in any plane, straight feeds, and traverses all in a random order with
differing feeds.

Run the resulting file in a stepper inifile with no headroom, and kick
things up a notch by using different max velocities and accelerations.
Turning feed override past 100% has also helped turn up bugs.
"""

from random import *
from math import sin, cos, pi

def feed():
    "Half the time, change the feed rate"
    if random() < .5: return
    print "F%d" % randrange(100, 1000, 10),

def torture_arc(x, y, z):
    "Generate a random arc (helix)"
    plane = randrange(3)
    print "G%d" % (plane+17),

    direction = randrange(2)
    print "G%d" % (direction+2),

    feed()

    theta = randrange(0, 360, 15) * pi / 180
    theta1 = randrange(0, 361, 15) * pi / 180
    r = randrange(1, 11)
    q = randrange(-10, 11)/2.

    print "(%d %d)" % (theta*180/pi, theta1*180/pi),
    if plane == 0: # XY plane
        ox = -cos(theta)*r
        oy = -sin(theta)*r
        print "I%f J%f" % (ox, oy),

        z = z + q
        x = x + ox + cos(theta1)*r
        y = y + oy + sin(theta1)*r

    elif plane == 1: # XZ plane
        ox = -cos(theta)*r
        oz = -sin(theta)*r
        print "I%f K%f" % (ox, oz),

        x = x + ox + sin(theta1)*r
        z = z + oz + cos(theta1)*r
        y = y + q

    else: # YZ plane
        oy = -cos(theta)*r
        oz = -sin(theta)*r
        print "J%f K%f" % (oy, oz),

        x = x + q
        y = y + oy + cos(theta1)*r
        z = z + oz + sin(theta1)*r

    print "X%f Y%f Z%f" % (x, y, z)

    return x, y, z

def torture_line(x, y, z):
    "Generate a random traverse or straight feed"
    kind = randrange(2)
    print "G%d" % kind,

    if kind == 1: feed()
    
    x = x + randrange(-10, 11)/2.
    y = y + randrange(-10, 11)/2.
    z = z + randrange(-10, 11)/2.

    print "X%f Y%f Z%f" % (x, y, z)

    return x, y, z

def torture_main(runs):
    """Generate random chains of movement several times, restarting near the
     center each time motion gets too far away."""
    funcs = [torture_line, torture_arc]
    #funcs = [torture_arc]
    def R(x,l=-50, k=50): return x < l or x > k

    print "g21"
    for i in range(runs):
        x = y = 0
        z = 20
        print "G0 X0 Y0 Z20"
        print "F100"

        while 1:
            x, y, z = choice(funcs)(x, y, z)
            if R(x) or R(y) or R(z,-10,30): break

# Do ten runs then print m2
torture_main(10)
print "m2"
