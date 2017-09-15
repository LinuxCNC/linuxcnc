#!/usr/bin/env python
#    Copyright (C) 2012 Jeff Epler <jepler@unpythonic.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

distances = [-10,1,-.1,0,0,.1,1,10]
radii = [.1, 1, 10]
arcangles = range(0, 360, 45)
aangles = [-361,-89,-1,1,91,359]
chance_angular = .5
chance_angular_helix = .5
chance_lineaxis = .2

def feed():
    "Half the time, change the feed rate"
    if random() < .5: return
    print "F%d" % randrange(100, 1000, 10),

def torture_arc(x, y, z, a, b, c, u, v, w):
    "Generate a random arc (helix)"
    plane = randrange(3)
    print "G%d" % (plane+17),

    direction = randrange(2)
    print "G%d" % (direction+2),

    feed()

    theta = choice(arcangles)
    theta1 = choice(arcangles)
    r = choice(radii)
    q = choice(distances)

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

    if random() < chance_angular_helix:
        a = a + choice(aangles)
        print "A%f" % a,

    if random() < chance_angular_helix:
        b = b + choice(aangles)
        print "B%f" % b,

    if random() < chance_angular_helix:
        c = c + choice(aangles)
        print "C%f" % c,

    if random() < chance_angular_helix:
        u = u + randrange(-500, 500)/100.
        print "U%f" % u,

    if random() < chance_angular_helix:
        v = v + randrange(-500, 500)/100.
        print "V%f" % v,

    if random() < chance_angular_helix:
        w = w + randrange(-500, 500)/100.
        print "W%f" % w,

    print "X%f Y%f Z%f" % (x, y, z)

    return x, y, z, a, b, c, u, v, w

def torture_line(x, y, z, a, b, c, u, v, w):
    "Generate a random traverse or straight feed"
    kind = randrange(2)
    p = ""
    p += "G%d " % kind

    if random() < chance_lineaxis:
        x = x + randrange(-10, 11)/2.
        p += "X%f " % x

    if random() < chance_lineaxis:
        y = y + randrange(-10, 11)/2.
        p += "Y%f " % y

    if random() < chance_lineaxis:
        z = z + randrange(-10, 11)/2.
        p += "Z%f " % z

    if random() < chance_lineaxis:
        a = a + randrange(-180, 80)/2.
        p += "A%f " % a

    if random() < chance_lineaxis:
        b = b + randrange(-180, 80)/2.
        p += "B%f " % b

    if random() < chance_lineaxis:
        c = c + randrange(-180, 80)/2.
        p += "C%f " % c

    if random() < chance_lineaxis:
        u = u + randrange(-500, 500)/100.
        p += "U%f " % u

    if random() < chance_lineaxis:
        v = v + randrange(-500, 500)/100.
        p += "V%f " % v

    if random() < chance_lineaxis:
        w = w + randrange(-500, 500)/100.
        p += "W%f " % w

    if len(p) > 4:
        if kind == 1: feed()
        print p

    return x, y, z, a, b, c, u, v, w

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
        a = b = c = u = v = w = 0
        if chance_angular or chance_angular_helix:
            print "G0 X0 Y0 A0 B0 C0 U0 V0 W0 Z20"
        else:
            print "G0 X0 Y0 Z20"
        print "F100"

        while 1:
            x, y, z, a, b, c, u, v, w = choice(funcs)(x, y, z, a, b, c, u, v, w)
            if (R(x) or R(y) or R(z,-10,30) or
                R(a,-30000,30000) or R(b, -30000,30000) or R(c, -30000,30000) or
                R(u) or R(v) or R(w)):
               break

# Do ten runs then print m2
torture_main(20)
print "m2"
