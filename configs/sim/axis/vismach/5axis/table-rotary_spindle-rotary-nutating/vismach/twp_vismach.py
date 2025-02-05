# This is a modified version of 'vismach.py' to visualize Tilted Work Plane (TWP)
# Author: David mueller
# email: mueller_david@hotmail.com

#    Copyright 2007 John Kasunich and Jeff Epler
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

import copy
import sys, rs274.OpenGLTk, signal, hal
import tkinter

from OpenGL.GL import *
from OpenGL.GLU import *
from math import *
import glnav
import hal
import numpy as np


class Collection(object):
    def __init__(self, parts):
        self.parts = parts
        self.vol = 0

    def traverse(self):
        for p in self.parts:
            if hasattr(p, "apply"):
                p.apply()
            if hasattr(p, "capture"):
                p.capture()
            if hasattr(p, "draw"):
                p.draw()
            if hasattr(p, "traverse"):
                p.traverse()
            if hasattr(p, "unapply"):
                p.unapply()


class Translate(Collection):
    def __init__(self, parts, x, y, z):
        self.parts = parts
        self.where = x, y, z

    def apply(self):
        glPushMatrix()
        glTranslatef(*self.where)

    def unapply(self):
        glPopMatrix()


class Rotate(Collection):
    def __init__(self, parts, th, x, y, z):
        self.parts = parts
        self.where = th, x, y, z

    def apply(self):
        th, x, y, z = self.where
        glPushMatrix()
        glRotatef(th, x, y, z)

    def unapply(self):
        glPopMatrix()


class Scale(Collection):
    def __init__(self, parts, x, y, z):
        self.parts = parts
        self.scaleby = x, y, z

    def apply(self):
        glPushMatrix()
        glScalef(*self.scaleby)

    def unapply(self):
        glPopMatrix()


class HalTranslate(Collection):
    def __init__(self, parts, comp, var, x, y, z):
        self.parts = parts
        self.where = x, y, z
        self.comp = comp
        self.var = var

    def apply(self):
        x, y, z = self.where
        v = self.comp[self.var]
        glPushMatrix()
        glTranslatef(x*v, y*v, z*v)

    def unapply(self):
        glPopMatrix()


class HalRotate(Collection):
    def __init__(self, parts, comp, var, th, x, y, z):
        self.parts = parts
        self.where = th, x, y, z
        self.comp = comp
        self.var = var

    def apply(self):
        th, x, y, z = self.where
        glPushMatrix()
        glRotatef(th * self.comp[self.var], x, y, z)

    def unapply(self):
        glPopMatrix()


class HalRotateEuler(Collection):
    def __init__(self, parts, comp, th1, th2, th3, order=123):
        self.parts = parts
        self.comp = comp
        self.order = order
        self.th1 = th1
        self.th2 = th2
        self.th3 = th3

    def apply(self):
        order = str(int(self.comp[self.order]))
        th1 = self.comp[self.th1]
        th2 = self.comp[self.th2]
        th3 = self.comp[self.th3]

        glPushMatrix()
        if order == '131':
            glRotatef(th1, 1, 0, 0)
            glRotatef(th2, 0, 0, 1)
            glRotatef(th3, 1, 0, 0)
        elif order =='121':
            glRotatef(th1, 1, 0, 0)
            glRotatef(th2, 0, 1, 0)
            glRotatef(th3, 1, 0, 0)
        elif order =='212':
            glRotatef(th1, 0, 1, 0)
            glRotatef(th2, 1, 0, 0)
            glRotatef(th3, 0, 1, 0)
        elif order =='232':
            glRotatef(th1, 0, 1, 0)
            glRotatef(th2, 0, 0, 1)
            glRotatef(th3, 0, 1, 0)
        elif order =='323':
            glRotatef(th1, 0, 0, 1)
            glRotatef(th2, 0, 1, 0)
            glRotatef(th3, 0, 0, 1)
        elif order =='313':
            glRotatef(th1, 0, 0, 1)
            glRotatef(th2, 1, 0, 0)
            glRotatef(th3, 0, 0, 1)
        elif order =='123':
            glRotatef(th1, 1, 0, 0)
            glRotatef(th2, 0, 1, 0)
            glRotatef(th3, 0, 0, 1)
        elif order =='132':
            glRotatef(th1, 1, 0, 0)
            glRotatef(th2, 0, 0, 1)
            glRotatef(th3, 0, 1, 0)
        elif order =='213':
            glRotatef(th1, 0, 1, 0)
            glRotatef(th2, 1, 0, 0)
            glRotatef(th3, 0, 0, 1)
        elif order =='231':
            glRotatef(th1, 0, 1, 0)
            glRotatef(th2, 0, 0, 1)
            glRotatef(th3, 1, 0, 0)
        elif order =='321':
            glRotatef(th1, 0, 0, 1)
            glRotatef(th2, 0, 1, 0)
            glRotatef(th3, 1, 0, 0)
        elif order =='312':
            glRotatef(th1, 0, 0, 1)
            glRotatef(th2, 1, 0, 0)
            glRotatef(th3, 0, 1, 0)

    def unapply(self):
        glPopMatrix()

# scales an object by the value of a halpin
class HalScale(Collection):
    def __init__(self, parts, comp, x, y, z, var):
        self.parts = parts
        self.comp = comp
        self.var = var
        self.xyz = x, y, z

    def apply(self):
        x, y, z = self.xyz
        factor = self.comp[self.var]
        glPushMatrix()
        glScalef(x*factor,y*factor,z*factor)

    def unapply(self):
        glPopMatrix()

# shows an object if const=var and hides it otherwise, behavior can be changed
# using the optional arguments for scalefactors when true or false
class HalShow(Collection):
    def __init__(self, parts, comp, const, var, scaleby_true=1, scaleby_false=0):
        self.parts = parts
        self.comp = comp
        self.const = const
        self.var = var
        self.scaleby_true = scaleby_true
        self.scaleby_false = scaleby_false

    def apply(self):
        s_t = self.scaleby_true
        s_f = self.scaleby_false
        glPushMatrix()
        if self.const == self.comp[self.var]:
            glScalef(s_t,s_t,s_t)
        else:
            glScalef(s_f,s_f,s_f)

    def unapply(self):
        glPopMatrix()


# translates an object using a variable translation vector
# use scale=-1 to change direction
class HalVectorTranslate(Collection):
    def __init__(self, parts, comp, xvar, yvar, zvar, scale=1):
        self.parts = parts
        self.comp = comp
        self.xvar = xvar
        self.yvar = yvar
        self.zvar = zvar
        self.sc   = scale

    def apply(self):
        # check for zero vector components
        xvar = 0 if self.xvar == 0 else self.comp[self.xvar]
        yvar = 0 if self.yvar == 0 else self.comp[self.yvar]
        zvar = 0 if self.zvar == 0 else self.comp[self.zvar]
        sc = self.sc
        glPushMatrix()
        glTranslatef(sc*xvar, sc*yvar, sc*zvar)

    def unapply(self):
        glPopMatrix()

class HalVectorRotate(Collection):
    def __init__(self, parts, comp, var, th, x, y, z):
        self.parts = parts
        self.where = th, x, y, z
        self.comp = comp
        self.var = var

    def apply(self):
        th, x, y, z = self.get_values()
        glPushMatrix()
        glRotatef(th * self.comp[self.var], x, y, z)

    def unapply(self):
        glPopMatrix()

class CoordsBase(object):
    def __init__(self, *args):
        if args and isinstance(args[0], hal.component):
           self.comp = args[0]
           args = args[1:]
        else:
           self.comp = None
        self._coords = args
        self.q = gluNewQuadric()

    def coords(self):
        return list(map(self._coord, self._coords))

    def _coord(self, v):
        if isinstance(v, str): return self.comp[v]
        return v


# draw an open cylinder from point_1 to point_2, radius is optional (defaults to 5)
class HalLine():
    def __init__(self, comp, x1var, y1var, z1var, x2var, y2var, z2var, stretch, r=5):
        self.comp = comp
        self.x1var = x1var
        self.y1var = y1var
        self.z1var = z1var
        self.x2var = x2var
        self.y2var = y2var
        self.z2var = z2var
        self.stretch = stretch
        self.r = r
        self.q = gluNewQuadric()

    def cross(self, a, b):
        return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]

    # calculate polar coordinates in degrees
    # a rotates around the x-axis; b rotates around the y axis
    def polar(self, v):
        length = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]) * self.stretch
        axis = (1, 0, 0) if hypot(v[0], v[1]) < 0.001 else self.cross(v, (0, 0, 1))
        angle = -atan2(hypot(v[0], v[1]), v[2])*180/pi
        return (length, angle, axis)

    def draw(self):
        x1 = 0 if self.x1var == 0 else self.comp[self.x1var]
        y1 = 0 if self.y1var == 0 else self.comp[self.y1var]
        z1 = 0 if self.z1var == 0 else self.comp[self.z1var]
        x2 = 0 if self.x2var == 0 else self.comp[self.x2var]
        y2 = 0 if self.y2var == 0 else self.comp[self.y2var]
        z2 = 0 if self.z2var == 0 else self.comp[self.z2var]
        r = self.r
        v = [x2,y2,z2]
        length, angle, axis = self.polar(v)
        glPushMatrix()
        glTranslate(x1, y1, z1)
        glRotate(angle,*axis)
        gluCylinder(self.q, r, r, length, 32, 1)

    def unapply(self):
        glPopMatrix()

# draw a plane defined by it's normal vector(vx,vy,vz) origin at (x,y,z)
class HalPlaneFromNormal():
    def __init__(self, comp,  x, y, z, vx, vy, vz, s=500):
        self.comp = comp
        self.x = x
        self.y = y
        self.z = z
        self.vx = vx
        self.vy = vy
        self.vz = vz
        self.s = s
        self.q = gluNewQuadric()

    def cross(self, a, b):
        return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]

    # calculate polar coordinates in degrees
    # a rotates around the x-axis; b rotates around the y axis
    def polar(self, v):
        length = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2])
        axis = (1, 0, 0) if hypot(v[0], v[1]) < 0.001 else self.cross(v, (0, 0, 1))
        angle = -atan2(hypot(v[0], v[1]), v[2])*180/pi
        return (length, angle, axis)

    def square(self,n, s):
        glBegin(GL_QUADS)
        glNormal3f(n[0],n[1],n[2])
        glVertex3f( s,  s, 0)
        glVertex3f(-s,  s, 0)
        glVertex3f(-s, -s, 0)
        glVertex3f( s, -s, 0)
        glEnd()

    def draw(self):
        # check for zero values in the arguments
        x = 0 if self.x == 0 else self.comp[self.x]
        y = 0 if self.y == 0 else self.comp[self.y]
        z = 0 if self.z == 0 else self.comp[self.z]
        vx = 0 if self.vx == 0 else self.comp[self.vx]
        vy = 0 if self.vy == 0 else self.comp[self.vy]
        vz = 0 if self.vz == 0 else self.comp[self.vz]
        s = self.s
        v = [vx, vy, vz]
        length, angle, axis = self.polar(v)
        glPushMatrix()
        glTranslate(x,y,z)
        glRotate(angle,*axis)
        self.square(v,s)

    def unapply(self):
        glPopMatrix()


# draw a coordinate system defined by it's normal vector(zx,zy,zz) and x-direction vector(xx, xy, xz)
# optional r to define the thickness of the cylinders
class HalCoordsFromNormalAndDirection():
    def __init__(self, comp, ox, oy, oz, xx, xy, xz, zx, zy, zz, stretch, r=5):
        self.comp = comp
        self.ox = ox
        self.oy = oy
        self.oz = oz
        self.xx = xx
        self.xy = xy
        self.xz = xz
        self.zx = zx
        self.zy = zy
        self.zz = zz
        self.stretch = stretch
        self.r = r
        self.q = gluNewQuadric()


    def draw_vector(self, color):
        gluCylinder(self.q, self.r, self.r, 50*self.stretch, 32, 1)
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color)

    def draw(self):
        # check for zero values in the arguments
        ox = 0 if self.ox == 0 else self.comp[self.ox]
        oy = 0 if self.oy == 0 else self.comp[self.oy]
        oz = 0 if self.oz == 0 else self.comp[self.oz]
        xx = 0 if self.xx == 0 else self.comp[self.xx]
        xy = 0 if self.xy == 0 else self.comp[self.xy]
        xz = 0 if self.xz == 0 else self.comp[self.xz]
        zx = 0 if self.zx == 0 else self.comp[self.zx]
        zy = 0 if self.zy == 0 else self.comp[self.zy]
        zz = 0 if self.zz == 0 else self.comp[self.zz]
        r = self.r
        vo = [ox, oy, oz]
        vx = [xx, xy, xz]
        vz = [zx, zy, zz]
        # calculate the missing y vector
        vy = [yx, yy, yz] = np.cross(vz,vx)
        m=np.matrix([[ xx, yx, zx, ox],
                     [ xy, yy, zy, oy],
                     [ xz, yz, zz, oz],
                     [  0,  0,  0, 1]])

        glPushMatrix()
        # for some reason we need to rotate in the opposite (transpose) sense
        glMultMatrixf(np.transpose(m))
        self.draw_vector([1,0,0,1])
        glRotate(90,0,1,0)
        self.draw_vector([0,1,0,1])
        glRotate(-90,1,0,0)
        self.draw_vector([0,0,1,1])

    def unapply(self):
        glPopMatrix()


# draw a grid defined by it's normal vector(zx,zy,zz) and x-direction vector(xx, xy, xz)
# optional s to define the half-width from the origin (ox,oy,oz)
class HalGridFromNormalAndDirection():
    def __init__(self, comp, ox, oy, oz, xx, xy, xz, zx, zy, zz, s=500):
        self.comp = comp
        self.ox = ox
        self.oy = oy
        self.oz = oz
        self.xx = xx
        self.xy = xy
        self.xz = xz
        self.zx = zx
        self.zy = zy
        self.zz = zz
        self.s = s
        self.q = gluNewQuadric()

    def square(self, s):
        glBegin(GL_LINES);
        for i in range (-s,s+10,10):
            # line 1 in x direction
            glVertex3f(-s, i, 0)
            glVertex3f( s, i, 0)
            # line 1 in y direction
            glVertex3f( i,-s, 0)
            glVertex3f( i, s, 0)
        glEnd()

    def draw(self):
        # check for zero values in the arguments
        ox = 0 if self.ox == 0 else self.comp[self.ox]
        oy = 0 if self.oy == 0 else self.comp[self.oy]
        oz = 0 if self.oz == 0 else self.comp[self.oz]
        xx = 0 if self.xx == 0 else self.comp[self.xx]
        xy = 0 if self.xy == 0 else self.comp[self.xy]
        xz = 0 if self.xz == 0 else self.comp[self.xz]
        zx = 0 if self.zx == 0 else self.comp[self.zx]
        zy = 0 if self.zy == 0 else self.comp[self.zy]
        zz = 0 if self.zz == 0 else self.comp[self.zz]
        s = self.s
        vo = [ox, oy, oz]
        vx = [xx, xy, xz]
        vz = [zx, zy, zz]
        # calculate the missing y vector
        vy = [yx, yy, yz] = np.cross(vz,vx)
        m=np.matrix([[ xx, yx, zx, ox],
                     [ xy, yy, zy, oy],
                     [ xz, yz, zz, oz],
                     [  0,  0,  0, 1]])

        glPushMatrix()
        # for some reason we need to rotate in the opposite (transpose) sense
        glMultMatrixf(np.transpose(m))
        self.square(s)

    def unapply(self):
        glPopMatrix()

# draw a grid defined by it's normal vector(vx,vy,vz) origin at (x,y,z)
class HalGridFromNormal():
    def __init__(self, comp,  x, y, z, vx, vy, vz, s=500):
        self.comp = comp
        self.x = x
        self.y = y
        self.z = z
        self.vx = vx
        self.vy = vy
        self.vz = vz
        self.s = s
        self.q = gluNewQuadric()

    def cross(self, a, b):
        return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]

    # calculate polar coordinates in degrees
    # a rotates around the x-axis; b rotates around the y axis
    def polar(self, v):
        length = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2])
        axis = (1, 0, 0) if hypot(v[0], v[1]) < 0.001 else self.cross(v, (0, 0, 1))
        angle = -atan2(hypot(v[0], v[1]), v[2])*180/pi
        return (length, angle, axis)

    def square(self,n, s):
        glBegin(GL_LINES);
        for i in range (-s,s+10,10):
            # line 1 in x direction
            glVertex3f(-s, i, 0)
            glVertex3f( s, i, 0)
            # line 1 in y direction
            glVertex3f( i,-s, 0)
            glVertex3f( i, s, 0)
        glEnd()

    def draw(self):
        # check for zero values in the arguments
        x = 0 if self.x == 0 else self.comp[self.x]
        y = 0 if self.y == 0 else self.comp[self.y]
        z = 0 if self.z == 0 else self.comp[self.z]
        vx = 0 if self.vx == 0 else self.comp[self.vx]
        vy = 0 if self.vy == 0 else self.comp[self.vy]
        vz = 0 if self.vz == 0 else self.comp[self.vz]
        s = self.s
        v = [vx, vy, vz]
        length, angle, axis = self.polar(v)
        glPushMatrix()
        glTranslate(x,y,z)
        glRotate(angle,*axis)
        self.square(v,s)

    def unapply(self):
        glPopMatrix()

# draw a grid defined by it's normal vector(vx,vy,vz) origin at (x,y,z)
class HalGrid():
    def __init__(self, comp, s=500):
        self.comp = comp
        self.s = s
        self.q = gluNewQuadric()

    def grid(self, s):
        glBegin(GL_LINES);
        for i in range (-s,s+10,10):
            # line 1 in x direction
            glVertex3f(-s, i, 0)
            glVertex3f( s, i, 0)
            # line 1 in y direction
            glVertex3f( i,-s, 0)
            glVertex3f( i, s, 0)
        glEnd()

    def draw(self):
        s = self.s
        self.grid(s)

    def unapply(self):
        glPopMatrix()

# give endpoint X values and radii
# resulting cylinder is on the X axis
class CylinderX(CoordsBase):
    def draw(self):
        x1, r1, x2, r2 = self.coords()
        if x1 > x2:
            tmp = x1
            x1 = x2
            x2 = tmp
            tmp = r1
            r1 = r2
            r2 = tmp
        glPushMatrix()
        # GL creates cylinders along Z, so need to rotate
        z1 = x1
        z2 = x2
        glRotatef(90,0,1,0)
        # need to translate the whole thing to z1
        glTranslatef(0,0,z1)
        # the cylinder starts out at Z=0
        gluCylinder(self.q, r1, r2, z2-z1, 32, 1)
        # bottom cap
        glRotatef(180,1,0,0)
        gluDisk(self.q, 0, r1, 32, 1)
        glRotatef(180,1,0,0)
        # the top cap needs flipped and translated
        glPushMatrix()
        glTranslatef(0,0,z2-z1)
        gluDisk(self.q, 0, r2, 32, 1)
        glPopMatrix()
        glPopMatrix()


# give endpoint Y values and radii
# resulting cylinder is on the Y axis
class CylinderY(CoordsBase):
    def __init__(self, y1, r1, y2, r2):
        self._coords = y1, r1, y2, r2
        self.q = gluNewQuadric()

    def draw(self):
        y1, r1, y2, r2 = self.coords()
        if y1 > y2:
            tmp = y1
            y1 = y2
            y2 = tmp
            tmp = r1
            r1 = r2
            r2 = tmp
        glPushMatrix()
        # GL creates cylinders along Z, so need to rotate
        z1 = y1
        z2 = y2
        glRotatef(-90,1,0,0)
        # need to translate the whole thing to z1
        glTranslatef(0,0,z1)
        # the cylinder starts out at Z=0
        gluCylinder(self.q, r1, r2, z2-z1, 32, 1)
        # bottom cap
        glRotatef(180,1,0,0)
        gluDisk(self.q, 0, r1, 32, 1)
        glRotatef(180,1,0,0)
        # the top cap needs flipped and translated
        glPushMatrix()
        glTranslatef(0,0,z2-z1)
        gluDisk(self.q, 0, r2, 32, 1)
        glPopMatrix()
        glPopMatrix()


class CylinderZ(CoordsBase):
    def draw(self):
        z1, r1, z2, r2 = self.coords()
        if z1 > z2:
            tmp = z1
            z1 = z2
            z2 = tmp
            tmp = r1
            r1 = r2
            r2 = tmp
        # need to translate the whole thing to z1
        glPushMatrix()
        glTranslatef(0,0,z1)
        # the cylinder starts out at Z=0
        gluCylinder(self.q, r1, r2, z2-z1, 32, 1)
        # bottom cap
        glRotatef(180,1,0,0)
        gluDisk(self.q, 0, r1, 32, 1)
        glRotatef(180,1,0,0)
        # the top cap needs flipped and translated
        glPushMatrix()
        glTranslatef(0,0,z2-z1)
        gluDisk(self.q, 0, r2, 32, 1)
        glPopMatrix()
        glPopMatrix()


# give center and radius
class Sphere(CoordsBase):
    def draw(self):
        x, y, z, r = self.coords()
        # need to translate the whole thing to x,y,z
        glPushMatrix()
        glTranslatef(x,y,z)
        # the sphere starts out at the origin
        gluSphere(self.q, r, 32, 16)
        glPopMatrix()


# triangular plate in XY plane
# specify the corners Z values for each side
class TriangleXY(CoordsBase):
    def draw(self):
        x1, y1, x2, y2, x3, y3, z1, z2 = self.coords()
        x12 = x1-x2
        y12 = y1-y2
        x13 = x1-x3
        y13 = y1-y3
        cross = x12*y13 - x13*y12
        if cross < 0:
            tmp = x2
            x2 = x3
            x3 = tmp
            tmp = y2
            y2 = y3
            y3 = tmp
        if z1 > z2:
            tmp = z1
            z1 = z2
            z2 = tmp
        x12 = x1-x2
        y12 = y1-y2
        x23 = x2-x3
        y23 = y2-y3
        x31 = x3-x1
        y31 = y3-y1
        glBegin(GL_QUADS)
        # side 1-2
        h = hypot(x12,y12)
        glNormal3f(-y12/h,x12/h,0)
        glVertex3f(x1, y1, z1)
        glVertex3f(x2, y2, z1)
        glVertex3f(x2, y2, z2)
        glVertex3f(x1, y1, z2)
        # side 2-3
        h = hypot(x23,y23)
        glNormal3f(-y23/h,x23/h,0)
        glVertex3f(x2, y2, z1)
        glVertex3f(x3, y3, z1)
        glVertex3f(x3, y3, z2)
        glVertex3f(x2, y2, z2)
        # side 3-1
        h = hypot(x31,y31)
        glNormal3f(-y31/h,x31/h,0)
        glVertex3f(x3, y3, z1)
        glVertex3f(x1, y1, z1)
        glVertex3f(x1, y1, z2)
        glVertex3f(x3, y3, z2)
        glEnd()
        glBegin(GL_TRIANGLES)
        # upper face
        glNormal3f(0,0,1)
        glVertex3f(x1, y1, z2)
        glVertex3f(x2, y2, z2)
        glVertex3f(x3, y3, z2)
        # lower face
        glNormal3f(0,0,-1)
        glVertex3f(x1, y1, z1)
        glVertex3f(x3, y3, z1)
        glVertex3f(x2, y2, z1)
        glEnd()


# triangular plate in XZ plane
class TriangleXZ(TriangleXY):
    def coords(self):
        x1, z1, x2, z2, x3, z3, y1, y2 = TriangleXY.coords(self)
        return x1, z1, x2, z2, x3, z3, -y1, -y2

    def draw(self):
        glPushMatrix()
        glRotatef(90,1,0,0)
        # create the triangle in XY plane
        TriangleXY.draw(self)
        # bottom cap
        glPopMatrix()


# triangular plate in YZ plane
class TriangleYZ(TriangleXY):
    def coords(self):
        y1, z1, y2, z2, y3, z3, x1, x2 = TriangleXY.coords(self)
        return z1, y1, z2, y2, z3, y3, -x1, -x2

    def draw(self):
        glPushMatrix()
        glRotatef(90,0,-1,0)
        # create the triangle in XY plane
        TriangleXY.draw(self)
        # bottom cap
        glPopMatrix()


class ArcX(CoordsBase):
    def draw(self):
        x1, x2, r1, r2, a1, a2, steps = self.coords()
        if x1 > x2:
            tmp = x1
            x1 = x2
            x2 = tmp
        if r1 > r2:
            tmp = r1
            r1 = r2
            r2 = tmp
        while a1 > a2:
            a2 = a2 + 360
        astep = ((a2-a1)/steps)*(pi/180)
        a1rads = a1 * (pi/180)
        # positive X end face
        glBegin(GL_QUAD_STRIP)
        glNormal3f(1,0,0)
        n = 0
        while n <= steps:
            angle = a1rads+n*astep
            s = sin(angle)
            c = cos(angle)
            glVertex3f(x2, r1*s, r1*c)
            glVertex3f(x2, r2*s, r2*c)
            n = n + 1

        glEnd()
        # negative X end face
        glBegin(GL_QUAD_STRIP)
        glNormal3f(-1,0,0)
        n = 0
        while n <= steps:
            angle = a1rads+n*astep
            s = sin(angle)
            c = cos(angle)
            glVertex3f(x1, r1*s, r1*c)
            glVertex3f(x1, r2*s, r2*c)
            n = n + 1
        glEnd()
        # inner diameter
        glBegin(GL_QUAD_STRIP)
        n = 0
        while n <= steps:
            angle = a1rads+n*astep
            s = sin(angle)
            c = cos(angle)
            glNormal3f(0,-s, -c)
            glVertex3f(x1, r1*s, r1*c)
            glVertex3f(x2, r1*s, r1*c)
            n = n + 1
        glEnd()
        # outer diameter
        glBegin(GL_QUAD_STRIP)
        n = 0
        while n <= steps:
            angle = a1rads+n*astep
            s = sin(angle)
            c = cos(angle)
            glNormal3f(0, s, c)
            glVertex3f(x1, r2*s, r2*c)
            glVertex3f(x2, r2*s, r2*c)
            n = n + 1
        glEnd()
        # end plates
        glBegin(GL_QUADS)
        # first end plate
        angle = a1 * (pi/180)
        s = sin(angle)
        c = cos(angle)
        glNormal3f(0, -c, s)
        glVertex3f(x1, r2*s, r2*c)
        glVertex3f(x2, r2*s, r2*c)
        glVertex3f(x2, r1*s, r1*c)
        glVertex3f(x1, r1*s, r1*c)
        # other end
        angle = a2 * (pi/180)
        s = sin(angle)
        c = cos(angle)
        glNormal3f(0, c, -s)
        glVertex3f(x1, r2*s, r2*c)
        glVertex3f(x2, r2*s, r2*c)
        glVertex3f(x2, r1*s, r1*c)
        glVertex3f(x1, r1*s, r1*c)
        glEnd()


# six coordinate version - specify each side of the box
class Box(CoordsBase):
    def draw(self):
        x1, y1, z1, x2, y2, z2 = self.coords()
        if x1 > x2:
            tmp = x1
            x1 = x2
            x2 = tmp
        if y1 > y2:
            tmp = y1
            y1 = y2
            y2 = tmp
        if z1 > z2:
            tmp = z1
            z1 = z2
            z2 = tmp

        glBegin(GL_QUADS)
        # bottom face
        glNormal3f(0,0,-1)
        glVertex3f(x2, y1, z1)
        glVertex3f(x1, y1, z1)
        glVertex3f(x1, y2, z1)
        glVertex3f(x2, y2, z1)
        # positive X face
        glNormal3f(1,0,0)
        glVertex3f(x2, y1, z1)
        glVertex3f(x2, y2, z1)
        glVertex3f(x2, y2, z2)
        glVertex3f(x2, y1, z2)
        # positive Y face
        glNormal3f(0,1,0)
        glVertex3f(x1, y2, z1)
        glVertex3f(x1, y2, z2)
        glVertex3f(x2, y2, z2)
        glVertex3f(x2, y2, z1)
        # negative Y face
        glNormal3f(0,-1,0)
        glVertex3f(x2, y1, z2)
        glVertex3f(x1, y1, z2)
        glVertex3f(x1, y1, z1)
        glVertex3f(x2, y1, z1)
        # negative X face
        glNormal3f(-1,0,0)
        glVertex3f(x1, y1, z1)
        glVertex3f(x1, y1, z2)
        glVertex3f(x1, y2, z2)
        glVertex3f(x1, y2, z1)
        # top face
        glNormal3f(0,0,1)
        glVertex3f(x1, y2, z2)
        glVertex3f(x1, y1, z2)
        glVertex3f(x2, y1, z2)
        glVertex3f(x2, y2, z2)
        glEnd()


# specify the width in X and Y, and the height in Z
# the box is centered on the origin
class BoxCentered(Box):
    def __init__(self, xw, yw, zw):
        Box.__init__(self, -xw/2.0, -yw/2.0, -zw/2.0, xw/2.0, yw/2.0, zw/2.0)

# specify the width in X and Y, and the height in Z
# the box is centered in X and Y, and runs from Z=0 up
# (or down) to the specified Z value
class BoxCenteredXY(Box):
    def __init__(self, xw, yw, zw):
        Box.__init__(self, -xw/2.0, -yw/2.0, 0, xw/2.0, yw/2.0, zw)


# capture current transformation matrix
# note that this transforms from the current coordinate system
# to the viewport system, NOT to the world system
class Capture(object):
    def __init__(self):
        self.t = []

    def capture(self):
        self.t = glGetDoublev(GL_MODELVIEW_MATRIX)


class HalTestTranslate(Collection):
    def __init__(self, parts, comp, var, x, y, z):
        self.parts = parts
        self.where = x, y, z
        self.comp = comp
        self.var = var

    def apply(self):
        x, y, z = self.where
        v = hal.get_value(self.var)
        glPushMatrix()
        glTranslatef(x*v, y*v, z*v)

    def unapply(self):
        glPopMatrix()

# head up display - draws a semi-transparent text box.
class Hud(object):
    def __init__(self):
        self.app = []
        self.strs = []
        self.fontbase = []
        self.hud_lines = []
        self.show_tags = []
        self.hide_alls = []
        self.hud_rgba_r = 0
        self.hud_rgba_g = 0.2
        self.hud_rgba_b = 0
        self.hud_rgba_a = 0.5
        self.hud_text_rgb_r = 1
        self.hud_text_rgb_g = 0.8
        self.hud_text_rgb_b = 0.4

    # legacy vismach models used this
    def show(self, string):
        self.add_txt(string)

    # displays a string, optionally a tag or list of tags can be assigned
    def add_txt(self, string, tag=None):
        self.hud_lines += [[str(string), None, tag]]

    # displays a formatted pin value (can be embedded in a string)
    def add_pin(self, string, pin=None, tag=None):
        self.hud_lines += [[str(string), pin, tag]]

    # shows all lines with the specified tag if the pin value = val
    def show_tag_if_same(self, tag, pin, val=True):
        self.show_tags += [[tag, pin, val]]

    # shows all lines with a tag equal to the pin value + offset
    def show_tags_in_pin(self, pin, offs=0):
        self.show_tags += [[pin, None, offs]]

    # hides the complete hud if the pin value is equal to val
    def hide_all(self, pin, val=True):
        self.hide_alls += [[pin, val]]

    # changes the hud color and transparency
    def set_hud_rgba(self, r, g, b, a):
        self.hud_rgba_r = r
        self.hud_rgba_g = g
        self.hud_rgba_b = b
        self.hud_rgba_a = a

    # changes the hud text color
    def set_hud_text_rgb(self, r, g, b):
        self.hud_text_rgb_r = r
        self.hud_text_rgb_g = g
        self.hud_text_rgb_b = b

    # update the lines in the hud using the lists created above
    def draw(self):
        hide_hud = 0
        strs = []
        show_list = [None]
        # check if hud should be hidden
        for a in self.hide_alls:
            if hal.get_value(a[0]) == a[1]:
                hide_hud = 1
        if hide_hud == 0:
            # create list of all line tags to be shown
            for b in self.show_tags:
                if b[1] == None: # _show_tags_in_pin
                    tag = int(hal.get_value(b[0]) + b[2])
                else: # _show_tag_if_same
                    if  hal.get_value(b[1]) == b[2]:
                        tag = b[0]
                if not isinstance(tag, list):
                    tag = [tag]
                show_list = show_list + tag
            # build the
            for c in self.hud_lines:
                if not isinstance(c[2], list):
                    c[2] = [c[2]]
                if any(item in c[2] for item in show_list):
                    if c[1] == None: # _txt
                        strs += [c[0]]
                    else: # _pin
                        strs += [c[0].format(hal.get_value(c[1]))]
        drawtext = strs

        # draw head-up-display
        # see axis.py for more font/color configurability
        if len(drawtext) == 0:
                return

        glMatrixMode(GL_PROJECTION)
        glPushMatrix()
        glLoadIdentity()

        if not self.fontbase:
                self.fontbase = int(self.app.loadbitmapfont("9x15"))
        char_width, char_height = 9, 15
        xmargin,ymargin = 5,5
        ypos = float(self.app.winfo_height())

        glOrtho(0.0, self.app.winfo_width(), 0.0, ypos, -1.0, 1.0)
        glMatrixMode(GL_MODELVIEW)
        glPushMatrix()
        glLoadIdentity()

        # draw the text box
        maxlen = max([len(p) for p in drawtext])
        box_width = maxlen * char_width
        glDepthFunc(GL_ALWAYS)
        glDepthMask(GL_FALSE)
        glDisable(GL_LIGHTING)
        glEnable(GL_BLEND)
        glEnable(GL_NORMALIZE)
        glBlendFunc(GL_ONE, GL_CONSTANT_ALPHA)
        # sets the color of the hud overlay
        glColor3f(self.hud_rgba_r, self.hud_rgba_g, self.hud_rgba_b)
        # rgba, sets the transparency of the overlay using the 'a' value
        glBlendColor(0,0,0,self.hud_rgba_a)
        glBegin(GL_QUADS)
        # upper left
        glVertex3f(0, ypos, 1)
        # lower left
        glVertex3f(0, ypos - 2*ymargin - char_height*len(drawtext), 1)
        # lower right
        glVertex3f(box_width+2*xmargin, ypos - 2*ymargin - char_height*len(drawtext), 1)
        # upper right
        glVertex3f(box_width+2*xmargin,  ypos , 1)
        glEnd()
        glDisable(GL_BLEND)
        glEnable(GL_LIGHTING)

        # fill the box with text
        maxlen = 0
        ypos -= char_height+ymargin
        i = 0
        glDisable(GL_LIGHTING)
        # sets the color of the text in the hud
        glColor3f(self.hud_text_rgb_r, self.hud_text_rgb_g, self.hud_text_rgb_b)
        for string in drawtext:
                maxlen = max(maxlen, len(string))
                glRasterPos2i(xmargin, int(ypos))
                for char in string:
                        glCallList(self.fontbase + ord(char))
                ypos -= char_height
                i = i + 1
        glDepthFunc(GL_LESS)
        glDepthMask(GL_TRUE)
        glEnable(GL_LIGHTING)

        glPopMatrix()
        glMatrixMode(GL_PROJECTION)
        glPopMatrix()
        glMatrixMode(GL_MODELVIEW)


# function to invert a transform matrix
# based on http://steve.hollasch.net/cgindex/math/matrix/afforthinv.c
# with simplifications since we don't do scaling
# This function inverts a 4x4 matrix that is affine and orthogonal.  In
# other words, the perspective components are [0 0 0 1], and the basis
# vectors are orthogonal to each other.  In addition, the matrix must
# not do scaling
def invert(src):
        # make a copy
        inv=copy.deepcopy(src)
        # The inverse of the upper 3x3 is the transpose (since the basis
        # vectors are orthogonal to each other.
        inv[0][1],inv[1][0] = inv[1][0],inv[0][1]
        inv[0][2],inv[2][0] = inv[2][0],inv[0][2]
        inv[1][2],inv[2][1] = inv[2][1],inv[1][2]
        # The inverse of the translation component is just the negation
        # of the translation after dotting with the new upper3x3 rows. */
        inv[3][0] = -(src[3][0]*inv[0][0] + src[3][1]*inv[1][0] + src[3][2]*inv[2][0])
        inv[3][1] = -(src[3][0]*inv[0][1] + src[3][1]*inv[1][1] + src[3][2]*inv[2][1])
        inv[3][2] = -(src[3][0]*inv[0][2] + src[3][1]*inv[1][2] + src[3][2]*inv[2][2])
        return inv


class O(rs274.OpenGLTk.Opengl):
    def __init__(self, *args, **kw):
        rs274.OpenGLTk.Opengl.__init__(self, *args, **kw)
        # sets the background color of the vismach display
        (self.r_back, self.g_back, self.b_back) = (0,0,0)
        #self.q1 = gluNewQuadric()
        #self.q2 = gluNewQuadric()
        #self.q3 = gluNewQuadric()
        self.plotdata = []
        self.plotlen = 16000
        # does not show HUD by default
        self.hud = Hud()

    def basic_lighting(self):
        self.activate()
        glLightfv(GL_LIGHT0, GL_POSITION, (1, -1, .5, 0))
        glLightfv(GL_LIGHT0, GL_AMBIENT, (.2,.2,.2,0))
        glLightfv(GL_LIGHT0, GL_DIFFUSE, (.6,.6,.4,0))
        glLightfv(GL_LIGHT0+1, GL_POSITION, (-1, -1, .5, 0))
        glLightfv(GL_LIGHT0+1, GL_AMBIENT, (.0,.0,.0,0))
        glLightfv(GL_LIGHT0+1, GL_DIFFUSE, (.0,.0,.4,0))
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, (1,1,1,0))
        glDisable(GL_CULL_FACE)
        glEnable(GL_LIGHTING)
        glEnable(GL_LIGHT0)
        glEnable(GL_LIGHT0+1)
        glDepthFunc(GL_LESS)
        glEnable(GL_DEPTH_TEST)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()

    def redraw(self, *args):
        if self.winfo_width() == 1: return
        self.model.traverse()
        # current coords: world
        # the matrices tool2view, work2view, and world2view
        # transform from tool/work/world coords to viewport coords
        # if we want to draw in tool coords, we need to do
        # "tool -> view -> world" (since the current frame is world)
        # and if we want to draw in work coords, we need
        # "work -> view -> world".  For both, we need to invert
        # the world2view matrix to do the second step
        view2world = invert(self.world2view.t)
        # likewise, for backplot, we want to transform the tooltip
        # position from tool coords (where it is [0,0,0]) to work
        # coords, so we need tool -> view -> work
        # so lets also invert the work2view matrix
        view2work = invert(self.work2view.t)

        # since backplot lines only need vertices, not orientation,
        # and the tooltip is at the origin, getting the tool coords
        # is easy
        tx, ty, tz = self.tool2view.t[3][:3]
        # now we have to transform them to the work frame
        wx = tx*view2work[0][0]+ty*view2work[1][0]+tz*view2work[2][0]+view2work[3][0]
        wy = tx*view2work[0][1]+ty*view2work[1][1]+tz*view2work[2][1]+view2work[3][1]
        wz = tx*view2work[0][2]+ty*view2work[1][2]+tz*view2work[2][2]+view2work[3][2]
        # wx, wy, wz are the values to use for backplot
        # so we save them in a buffer
        if len(self.plotdata) == self.plotlen:
            del self.plotdata[:self.plotlen / 10]
        point = [ wx, wy, wz ]
        if not self.plotdata or point != self.plotdata[-1]:
            self.plotdata.append(point)

        # now lets draw something in the tool coordinate system
        #glPushMatrix()
        # matrixes take effect in reverse order, so the next
        # two lines do "tool -> view -> world"
        #glMultMatrixd(view2world)
        #glMultMatrixd(self.tool2view.t)

        # do drawing here
        # cylinder normally goes to +Z, we want it down
        #glTranslatef(0,0,-60)
        #gluCylinder(self.q1, 20, 20, 60, 32, 16)

        # back to world coords
        #glPopMatrix()

        # we can also draw in the work coord system
        glPushMatrix()
        # "work -> view -> world"
        glMultMatrixd(view2world)
        glMultMatrixd(self.work2view.t)
        # now we can draw in work coords, and whatever we draw
        # will move with the work, (if the work is attached to
        # a table or indexer or something that moves with
        # respect to the world

        # just a test object, sitting on the table
        #gluCylinder(self.q2, 40, 20, 60, 32, 16)

        # draw head up display
        if(hasattr(self.hud, "draw")):
                self.hud.draw()

        # draw backplot
        glDisable(GL_LIGHTING)
        glLineWidth(2)
        glColor3f(1.0,0.5,0.5)

        glBegin(GL_LINE_STRIP)
        for p in self.plotdata:
            glVertex3f(*p)
        glEnd()

        glEnable(GL_LIGHTING)
        glColor3f(1,1,1)
        glLineWidth(1)
        glDisable(GL_BLEND)
        glDepthFunc(GL_LESS)

        # back to world again
        glPopMatrix()

    def plotclear(self):
        del self.plotdata[:self.plotlen]

# added optional glow
class Color(Collection):
    def __init__(self, color, parts, glow=0):
        self.color = color
        self.glow = glow
        Collection.__init__(self, parts)

    def apply(self):
        glPushAttrib(GL_LIGHTING_BIT)
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, self.color)
        if self.glow==1:
            glMaterialfv(GL_FRONT, GL_EMISSION, [1,1,0,1])
        else:
            glMaterialfv(GL_FRONT, GL_EMISSION, [0,0,0,1])
    def unapply(self):
        glPopAttrib()


class AsciiSTL:
    def __init__(self, filename=None, data=None):
        if data is None:
            data = open(filename, "r")
        elif isinstance(data, str):
            data = data.split("\n")
        self.list = None
        t = []
        n = [0,0,0]
        self.d = d = []
        for line in data:
            if line.find("normal") != -1:
                line = line.split()
                x, y, z = list(map(float, line[-3:]))
                n = [x,y,z]
            elif line.find("vertex") != -1:
                line = line.split()
                x, y, z = list(map(float, line[-3:]))
                t.append([x,y,z])
                if len(t) == 3:
                    if n == [0,0,0]:
                        dx1 = t[1][0] - t[0][0]
                        dy1 = t[1][1] - t[0][1]
                        dz1 = t[1][2] - t[0][2]
                        dx2 = t[2][0] - t[0][0]
                        dy2 = t[2][1] - t[0][1]
                        dz2 = t[2][2] - t[0][2]
                        n = [dy1*dz2 - dy2*dz1, dz1*dx2 - dz2*dx1, dy1*dx2 - dy2*dx1]
                    d.append((n, t))
                    t = []
                    n = [0,0,0]

    def draw(self):
        if self.list is None:
            # OpenGL isn't ready yet in __init__ so the display list
            # is created during the first draw
            self.list = glGenLists(1)
            glNewList(self.list, GL_COMPILE)
            glBegin(GL_TRIANGLES)
            for n, t in self.d:
                glNormal3f(*n)
                glVertex3f(*t[0])
                glVertex3f(*t[1])
                glVertex3f(*t[2])
            glEnd()
            glEndList()
            del self.d
        glCallList(self.list)


class AsciiOBJ:
    def __init__(self, filename=None, data=None):
        if data is None:
            data = open(filename, "r")
        elif isinstance(data, str):
            data = data.split("\n")

        self.v = v = []
        self.vn = vn = []
        self.f = f = []
        for line in data:
            if line.startswith("#"): continue
            if line.startswith("vn"):
                vn.append([float(w) for w in line.split()[1:]])
            elif line.startswith("v"):
                v.append([float(w) for w in line.split()[1:]])
            elif line.startswith("f"):
                f.append(self.parse_face(line))
        self.list = None

    def parse_int(self, i):
        if i == '': return None
        return int(i)

    def parse_slash(self, word):
        return [self.parse_int(i) for i in word.split("/")]

    def parse_face(self, line):
        return [self.parse_slash(w) for w in line.split()[1:]]

    def draw(self):
        if self.list is None:
            # OpenGL isn't ready yet in __init__ so the display list
            # is created during the first draw
            self.list = glGenLists(1)
            glNewList(self.list, GL_COMPILE)
            glDisable(GL_CULL_FACE)
            glBegin(GL_TRIANGLES)
            #print "obj", len(self.f)
            for f in self.f:
                for v, t, n in f:
                    if n:
                        glNormal3f(*self.vn[n-1])
                    glVertex3f(*self.v[v-1])
            glEnd()
            glEndList()
            del self.v
            del self.vn
            del self.f
        glCallList(self.list)


old_plotclear = False

def main(model, tool, work, size=10, hud=0, rotation_vectors=None, lat=0, lon=0):
    app = tkinter.Tk()

    t = O(app, double=1, depth=1)
    # set which axes to rotate around
    if rotation_vectors: t.rotation_vectors = rotation_vectors
    # we want to be able to see the model from all angles
    t.set_latitudelimits(-180, 180)
    # set starting viewpoint if desired
    t.after(100, lambda: t.set_viewangle(lat, lon, forcerotate=1))

    vcomp = hal.component("vismach")
    vcomp.newpin("plotclear",hal.HAL_BIT,hal.HAL_IN)
    vcomp.ready()

    #there's probably a better way of doing this
    global HUD
    HUD = 0
    if(hud != 0 and hasattr(hud, "app")):
            HUD = hud
            #point our app at the global
            t.hud = HUD

    t.hud.app = t #HUD needs to know where to draw

    # need to capture the world coordinate system
    world = Capture()

    t.model = Collection([model, world])
    t.distance = size * 3
    t.near = size * 0.01
    t.far = size * 10.0
    t.tool2view = tool
    t.world2view = world
    t.work2view = work

    t.pack(fill="both", expand=1)

    def update():
        global old_plotclear
        t.tkRedraw()
        new_plotclear = vcomp["plotclear"]
        if new_plotclear and not old_plotclear:
            t.plotclear()
        old_plotclear=new_plotclear
        t.after(100, update)
    update()

    def quit(*args):
        raise SystemExit

    signal.signal(signal.SIGTERM, quit)
    signal.signal(signal.SIGINT, quit)

    app.mainloop()
