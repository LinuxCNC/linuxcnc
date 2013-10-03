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
import lineardeltakins
import sys

EFFECTOR_OFFSET = CARRIAGE_OFFSET = 30
MIN_JOINT = -236
MAX_JOINT = 300

for setting in sys.argv[1:]: exec setting

c = hal.component("rostock")
c.newpin("joint0", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint3", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("R", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("L", hal.HAL_FLOAT, hal.HAL_IN)
c['R'], c['L'] = lineardeltakins.get_geometry()
c.ready()

class DeltaTranslate(Collection):
    def __init__(self, parts, comp):
        self.comp = comp
        self.parts = parts
        self.x = self.y = self.z = 0

    def apply(self):
        glPushMatrix()
        lineardeltakins.set_geometry(self.comp['R'], self.comp['L'])        
        f = lineardeltakins.forward(self.comp['joint0'], self.comp['joint1'], self.comp['joint2'])
        if f is not None:
            self.x = x = f[0]
            self.y = y = f[1]
            self.z = z = f[2]
        else:
            x = self.x
            y = self.y
            z = self.z
        glTranslatef(x, y, z)

    def unapply(self):
        glPopMatrix()

class TriangularPrismZ:
    def __init__(self, z0, z1, h):
        self.z0 = z0
        self.z1 = z1
        self.h = h

    def draw(self):
        x0 = 0
        x1 = -self.h*sin(pi/3)
        x2 = self.h*sin(pi/3)
        y0 = self.h 
        y1 = y2 = -self.h*cos(pi/3)
        z0 = self.z0
        z1 = self.z1

        glBegin(GL_TRIANGLES)
        glNormal3f(0, 0, 1)
        glVertex3f(x0, y0, z1)
        glVertex3f(x1, y1, z1)
        glVertex3f(x2, y2, z1)

        glNormal3f(0, 0, -1)
        glVertex3f(x2, y2, z0)
        glVertex3f(x1, y1, z0)
        glVertex3f(x0, y0, z0)

        glNormal3f(-cos(pi/3), sin(pi/3), 0)
        glVertex3f(x0, y0, z1)
        glVertex3f(x1, y1, z0)
        glVertex3f(x1, y1, z1)
        glVertex3f(x0, y0, z1)
        glVertex3f(x0, y0, z0)
        glVertex3f(x1, y1, z0)

        glNormal3f(0, -1, 0)
        glVertex3f(x1, y1, z1)
        glVertex3f(x2, y2, z0)
        glVertex3f(x2, y2, z1)
        glVertex3f(x1, y1, z1)
        glVertex3f(x1, y1, z0)
        glVertex3f(x2, y2, z0)

        glNormal3f(cos(pi/3), sin(pi/3), 0)
        glVertex3f(x2, y2, z1)
        glVertex3f(x0, y0, z0)
        glVertex3f(x0, y0, z1)
        glVertex3f(x2, y2, z1)
        glVertex3f(x2, y2, z0)
        glVertex3f(x0, y0, z0)

        glEnd()
def build_joint(angle, joint):
    return Rotate([
        Translate([
            HalTranslate([
                CylinderZ(MIN_JOINT, 5, MAX_JOINT, 5),
                HalTranslate([
                    Box(-CARRIAGE_OFFSET, -20, -20, 0, 20, 20)
                ], c, joint, 0, 0, 1)
            ], c, "R", 1, 0, 0)
        ], EFFECTOR_OFFSET + CARRIAGE_OFFSET, 0, 0)
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
        o = CARRIAGE_OFFSET
        oo = .4 * o
        x0 = self.platform.x + c*o
        sx = oo * -s
        y0 = self.platform.y + s*o
        sy = oo * c
        z0 = self.platform.z
        r2 = self.component['R'] + CARRIAGE_OFFSET
        x1 = r2 * cos(self.angle)
        y1 = r2 * sin(self.angle)
        z1 = self.component[self.joint]

        d = x1-x0, y1-y0, z1-z0
        mag = sqrt(sum(di*di for di in d))
        dx, dy, dz = (di/mag for di in d)
        L = self.component['L']
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
        gluCylinder(self.q, 5, 5, L, 32, 1)
	# bottom cap
	glRotatef(180,1,0,0)
	gluDisk(self.q, 0, 5, 32, 1)
	glRotatef(180,1,0,0)
	# the top cap needs flipped and translated
	glTranslatef(0,0, L)
	gluDisk(self.q, 0, 5, 32, 1)
       
tooltip = Capture()
tool = DeltaTranslate([
    Translate([
        Color((.5,.5,.5,0), [
            Translate([tooltip], 0,0,-40),
            Rotate([TriangularPrismZ(0, 5, EFFECTOR_OFFSET*2)], 180, 0, 0, 1),
            CylinderZ(-40, 0, -30, 10),
            CylinderZ(-30, 10, 10, 10)
        ])
    ], 0, 0, -5)], c)

red = (1,.5,.5,0)
green = (.5,1,.5,0)
blue = (.5,.5,1,0)

joint0 = Color(red, [build_joint(90, "joint0")])
joint1 = Color(green, [build_joint(210, "joint1")])
joint2 = Color(blue, [build_joint(330, "joint2")])

work = Capture()

strut0 = Color(red, [Strut(tool, 90, c, "joint0")])
strut1 = Color(green, [Strut(tool, 210, c, "joint1")])
strut2 = Color(blue, [Strut(tool, 330, c, "joint2")])

table = CylinderZ(MIN_JOINT-5, 300, MIN_JOINT, 300)

model = Collection([table, joint0, joint1, joint2, tool, strut0, strut1, strut2, work])
main(model, tooltip, work, 1500)
