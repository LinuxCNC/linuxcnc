#!/usr/bin/python
#    Copyright 2013 Jeff Epler <jepler@unpythonic.net>
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

from vismach import *
import hal
import rotarydeltakins
import sys

# allow overriding variables here, using the command line, like:
# loadusr rotarydelta SOMENAME=123
for setting in sys.argv[1:]: exec setting

c = hal.component("rotarydelta")
c.newpin("joint0", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("pfr", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("tl", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("sl", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("fr", hal.HAL_FLOAT, hal.HAL_IN)
c['pfr'], c['tl'], c['sl'], c['fr'] = rotarydeltakins.get_geometry()
c.ready()

class DeltaTranslate(Collection):
    def __init__(self, parts, comp):
        self.comp = comp
        self.parts = parts
        self.x = self.y = self.z = 0

    def apply(self):
        glPushMatrix()
        rotarydeltakins.set_geometry(self.comp['pfr'], self.comp['tl'], self.comp['sl'], self.comp['fr'])
        f = rotarydeltakins.forward(self.comp['joint0'], self.comp['joint1'], self.comp['joint2'])
        if f is not None:
            self.x = x = -f[0]
            self.y = y = f[1]
            self.z = z = -f[2]
        else:
            x = self.x
            y = self.y
            z = self.z
        glTranslatef(x, y, z)

    def unapply(self):
        glPopMatrix()

class HexPrismZ(CoordsBase):
    def draw(self):
	z0, z1, h = self.coords()
        h /= cos(pi/6)

        glBegin(GL_TRIANGLE_FAN)
        glNormal3f(0, 0, 1)
        glVertex3f(0, 0, z1)
        for i in range(7):
            d = (2*pi/6) * i
            glVertex3f(h * cos(d), h * sin(d), z1)
        glEnd()

        glBegin(GL_TRIANGLE_FAN)
        glNormal3f(0, 0, -1)
        glVertex3f(0, 0, z0)
        for i in range(7):
            d = (2*pi/6) * i
            glVertex3f(h * cos(d), h * sin(d), z0)
        glEnd()

        glBegin(GL_TRIANGLES)
        for i in range(6):
            d1 = (2*pi/6) * i
            cd1 = h * cos(d1)
            sd1 = h * sin(d1)

            d2 = (2*pi/6) * (i+1)
            cd2 = h * cos(d2)
            sd2 = h * sin(d2)

            glNormal3f(cos(d1), sin(d1), 0)
            glVertex3f(cd1, sd1, z1)
            glVertex3f(cd2, sd2, z0)
            glVertex3f(cd2, sd2, z1)
            glVertex3f(cd1, sd1, z1)
            glVertex3f(cd1, sd1, z0)
            glVertex3f(cd2, sd2, z0)
        glEnd()

def build_joint(angle, joint):
    return Rotate([
	HalTranslate([
	    CylinderY(-.5, 1, .5, 1),
	    HalRotate([
		CylinderX(c, 0, .5, 'tl', .5)
	    ], c, joint, 1, 0, 1, 0)
	], c, "pfr", 1, 0, 0)
    ], angle, 0, 0, 1)

class Strut:
    def __init__(self, platform, angle, component, joint):
        self.platform = platform
        self.angle = radians(angle)
        self.component = component
        self.joint = joint
        self.q = gluNewQuadric()

    def draw(self):
        c = cos(self.angle)
        s = sin(self.angle)
        o = self.component['fr']
        oo = .2 * o
        x0 = self.platform.x + c*o
        sx = oo * -s
        y0 = self.platform.y + s*o
        sy = oo * c
        z0 = self.platform.z
        j = -radians(self.component[self.joint])
        r2 = self.component['pfr'] + self.component['tl'] * cos(j)
        x1 = r2 * cos(self.angle)
        y1 = r2 * sin(self.angle)
	z1 = self.component['tl'] * sin(j)

        d = x1-x0, y1-y0, z1-z0
        mag = sqrt(sum(di*di for di in d))
        dx, dy, dz = (di/mag for di in d)
        L = self.component['sl']
        theta = atan2(dz, hypot(dx,dy))
        phi = atan2(dy, dx)

        glPushMatrix()
        glTranslatef(x0+sx, y0+sy, z0)
        glRotatef(degrees(phi), 0, 0, 1)
        glRotatef(90-degrees(theta), 0, 1, 0)
        self.cylinder(L)
        glPopMatrix()

        glPushMatrix()
        glTranslatef(x0-sx, y0-sy, z0)
        glRotatef(degrees(phi), 0, 0, 1)
        glRotatef(90-degrees(theta), 0, 1, 0)
        self.cylinder(L)
        glPopMatrix()

    def cylinder(self, L):
        gluCylinder(self.q, .5, .5, L, 32, 1)
	# bottom cap
	glRotatef(180,1,0,0)
	gluDisk(self.q, 0, .5, 32, 1)
	glRotatef(180,1,0,0)
	# the top cap needs flipped and translated
	glTranslatef(0,0, L)
	gluDisk(self.q, 0, .5, 32, 1)
       
tooltip = Capture()

tool = DeltaTranslate([
    Translate([
        Color((.5,.5,.5,0), [
            Translate([tooltip], 0,0,3.4),
            HexPrismZ(c, 0, .5, 'fr'),
            CylinderZ(3.4, 0, 2.5, .25),
            CylinderZ(2.5, .25, 0, .25)
        ])
    ], 0, 0, -.5)], c)

red = (1,.5,.5,0)
green = (.5,1,.5,0)
blue = (.5,.5,1,0)

joint0 = Color(red, [build_joint(-90, "joint0")])
joint1 = Color(green, [build_joint(30, "joint1")])
joint2 = Color(blue, [build_joint(150, "joint2")])

work = Capture()

strut0 = Color(red, [Strut(tool, -90, c, "joint0")])
strut1 = Color(green, [Strut(tool, 30, c, "joint1")])
strut2 = Color(blue, [Strut(tool, 150, c, "joint2")])

rotary = Rotate([Translate([
    Box(0, 0, 0, 2.5, 4, 5), #main body
    Box(-.5, .2, .2, 3, 3.8, 3.8), # main bearings
    Translate([CylinderZ(0, .3125/2, -1.25, .3125/2)], .4375, .46875, 0), # studs
    Translate([CylinderZ(0, .3125/2, -1.25, .3125/2)], .4375, 3.125+.46875, 0),
    Translate([CylinderZ(0, .3125/2, -1.25, .3125/2)], .4375+1.6375, .46875, 0),
    Translate([CylinderZ(0, .3125/2, -1.25, .3125/2)], .4375+1.6375, 3.125+.46875, 0),
    Translate([CylinderX(-.6, .63, 7, .63)], 0, 2, 1.875), # main shaft
    Box(.25, -.75, 2.65, 2.25, 4.75, 4.65), # drive bearings
    Translate([CylinderY(4, .25, 6, .25)], 1.25, 0, 3.65), # drive shaft
    Translate([CylinderY(5.3, 1, 6, 1)], 1.25, 0, 3.65), # big pulley
    Translate([CylinderY(-.6, 1.0625, 5.375, 1.0625)], 1.625, 0, 6.625), # motor
    ], -6, -2-c['pfr'], -1.875)], 180, 0, 1, 0)

platform = Collection([
        CylinderZ(2, c['pfr']+3.5, 3, c['pfr']+3.5),
        rotary,
        Rotate([rotary], 120, 0, 0, 1),
        Rotate([rotary], 240, 0, 0, 1),
        ])


#trotary = Translate([rotary], 0, 0, 10)
                    
yellow = (1, 1, 0, 0)

table = Color(yellow, [Collection([
    Box(8,8,-8,-8,-8,-7),
    Box(4,4,-7,-4,-4,2)
    ])])

model = Rotate([Collection([platform, table, joint0, joint1, joint2, tool,
                            strut0, strut1, strut2,
                            work])], 180, 0, 1, 0)
main(model, tooltip, work, 60)
