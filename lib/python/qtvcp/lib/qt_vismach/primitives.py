import copy
import OpenGL.GL as GL
from OpenGL import GLU
import hal


#################################################

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

    def volume(self):
        if hasattr(self, "vol") and self.vol != 0:
            vol = self.vol
        else:
            vol = sum(part.volume() for part in self.parts)
        # print "Collection.volume", vol
        return vol

    # a collection consisting of overlapping parts will have an incorrect
    # volume, because overlapping volumes will be counted twice.  If the
    # correct volume is known, it can be set using this method
    def set_volume(self, vol):
        self.vol = vol;


class Translate(Collection):
    def __init__(self, parts, x, y, z):
        self.parts = parts
        self.where = x, y, z

    def apply(self):
        GL.glPushMatrix()
        GL.glTranslatef(*self.where)

    def unapply(self):
        GL.glPopMatrix()


class Scale(Collection):
    def __init__(self, parts, x, y, z):
        self.parts = parts
        self.scaleby = x, y, z

    def apply(self):
        GL.glPushMatrix()
        GL.glScalef(*self.scaleby)

    def unapply(self):
        GL.glPopMatrix()


class HalTranslate(Collection):
    def __init__(self, parts, comp, var, x, y, z, direct=None):
        self.parts = parts
        self.where = x, y, z
        self.comp = comp
        self.var = var
        self.direct = direct

    def apply(self):
        x, y, z = self.where
        if self.direct is None:
            v = self.comp[self.var]
        else:
            try:
                v = hal.get_value(self.var)
            except:
                v = 0
        GL.glPushMatrix()
        GL.glTranslatef(x * v, y * v, z * v)

    def unapply(self):
        GL.glPopMatrix()


class HalRotate(Collection):
    def __init__(self, parts, comp, var, th, x, y, z, direct=None):
        self.parts = parts
        self.where = th, x, y, z
        self.comp = comp
        self.var = var
        self.direct = direct

    def apply(self):
        th, x, y, z = self.where
        GL.glPushMatrix()
        if self.direct is None:
            v = self.comp[self.var]
        else:
            try:
                v = hal.get_value(self.var)
            except:
                v = 0
        GL.glRotatef(th * v, x, y, z)

    def unapply(self):
        GL.glPopMatrix()


class Rotate(Collection):
    def __init__(self, parts, th, x, y, z):
        self.parts = parts
        self.where = th, x, y, z

    def apply(self):
        th, x, y, z = self.where
        GL.glPushMatrix()
        GL.glRotatef(th, x, y, z)

    def unapply(self):
        GL.glPopMatrix()


class Track(Collection):
    '''move and rotate an object to point from one capture()'d 
        coordinate system to another.
        we need "world" to convert coordinates from GL_MODELVIEW coordinates
        to our coordinate system'''

    def __init__(self, parts, position, target, world):
        self.parts = parts
        self.target = target
        self.position = position
        self.world2view = world

    def angle_to(self, x, y, z):
        '''returns polar coordinates in degrees to a point from the origin
        a rotates around the x-axis; b rotates around the y axis; r is the distance'''
        azimuth = atan2(y, x) * 180 / pi  # longitude
        elevation = atan2(z, sqrt(x ** 2 + y ** 2)) * 180 / pi
        radius = sqrt(x ** 2 + y ** 2 + z ** 2)
        return ((azimuth, elevation, radius))

    def map_coords(self, tx, ty, tz, transform):
        # now we have to transform them to the world frame
        wx = tx * transform[0] + ty * transform[4] + tz * transform[8] + transform[12]
        wy = tx * transform[1] + ty * transform[5] + tz * transform[9] + transform[13]
        wz = tx * transform[2] + ty * transform[6] + tz * transform[10] + transform[14]
        return ([wx, wy, wz])

    def apply(self):
        # make sure we have something to work with first
        if (self.world2view.t == []):
            # something's borkled - give up
            print("vismach.py: Track: why am i here? world is not in the scene yet")
            GL.glPushMatrix()
            return

        view2world = invert(self.world2view.t)

        px, py, pz = self.position.t[3][:3]
        px, py, pz = self.map_coords(px, py, pz, view2world)
        tx, ty, tz = self.target.t[3][:3]
        tx, ty, tz = self.map_coords(tx, ty, tz, view2world)
        dx = tx - px;
        dy = ty - py;
        dz = tz - pz;
        (az, el, r) = self.angle_to(dx, dy, dz)
        if (hasattr(HUD, "debug_track") and HUD.debug_track == 1):
            HUD.strs = []
            HUD.strs += ["current coords: %3.4f %3.4f %3.4f " % (px, py, pz)]
            HUD.strs += ["target coords: %3.4f %3.4f %3.4f" % (tx, ty, tz)]
            HUD.strs += ["az,el,r: %3.4f %3.4f %3.4f" % (az, el, r)]
        GL.glPushMatrix()
        GL.glTranslatef(px, py, pz)
        GL.glRotatef(az - 90, 0, 0, 1)
        GL.glRotatef(el - 90, 1, 0, 0)

    def unapply(self):
        GL.glPopMatrix()


class CoordsBase(object):
    def __init__(self, *args):
        if args and isinstance(args[0], hal.component):
            self.comp = args[0]
            args = args[1:]
        else:
            self.comp = None
        self._coords = args
        self.q = GLU.gluNewQuadric()

    def coords(self):
        return list(map(self._coord, self._coords))

    def _coord(self, v):
        if isinstance(v, str): return self.comp[v]
        return v


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
        GL.glPushMatrix()
        # GL creates cylinders along Z, so need to rotate
        z1 = x1
        z2 = x2
        GL.glRotatef(90, 0, 1, 0)
        # need to translate the whole thing to z1
        GL.glTranslatef(0, 0, z1)
        # the cylinder starts out at Z=0
        GLU.gluCylinder(self.q, r1, r2, z2 - z1, 32, 1)
        # bottom cap
        GL.glRotatef(180, 1, 0, 0)
        GLU.gluDisk(self.q, 0, r1, 32, 1)
        GL.glRotatef(180, 1, 0, 0)
        # the top cap needs flipped and translated
        GL.glPushMatrix()
        GL.glTranslatef(0, 0, z2 - z1)
        GLU.gluDisk(self.q, 0, r2, 32, 1)
        GL.glPopMatrix()
        GL.glPopMatrix()

    def volume(self):
        x1, r1, x2, r2 = self.coords()
        # actually a frustum of a cone
        vol = 3.1415927 / 3.0 * abs(x1 - x2) * (r1 * r1 + r1 * r2 + r2 * r2)
        # print "CylinderX.volume", vol
        return vol


# give endpoint Y values and radii
# resulting cylinder is on the Y axis
class CylinderY(CoordsBase):
    def __init__(self, y1, r1, y2, r2):
        self._coords = y1, r1, y2, r2
        self.q = GLU.gluNewQuadric()

    def draw(self):
        y1, r1, y2, r2 = self.coords()
        if y1 > y2:
            tmp = y1
            y1 = y2
            y2 = tmp
            tmp = r1
            r1 = r2
            r2 = tmp
        GL.glPushMatrix()
        # GL creates cylinders along Z, so need to rotate
        z1 = y1
        z2 = y2
        GL.glRotatef(-90, 1, 0, 0)
        # need to translate the whole thing to z1
        GL.glTranslatef(0, 0, z1)
        # the cylinder starts out at Z=0
        GLU.gluCylinder(self.q, r1, r2, z2 - z1, 32, 1)
        # bottom cap
        GL.glRotatef(180, 1, 0, 0)
        GLU.gluDisk(self.q, 0, r1, 32, 1)
        GL.glRotatef(180, 1, 0, 0)
        # the top cap needs flipped and translated
        GL.glPushMatrix()
        GL.glTranslatef(0, 0, z2 - z1)
        GLU.gluDisk(self.q, 0, r2, 32, 1)
        GL.glPopMatrix()
        GL.glPopMatrix()

    def volume(self):
        y1, r1, y2, r2 = self.coords()
        # actually a frustum of a cone
        vol = 3.1415927 / 3.0 * abs(y1 - y2) * (r1 * r1 + r1 * r2 + r2 * r2)
        # print "CylinderY.volume", vol
        return vol


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
        GL.glPushMatrix()
        GL.glTranslatef(0, 0, z1)
        # the cylinder starts out at Z=0
        GLU.gluCylinder(self.q, r1, r2, z2 - z1, 32, 1)
        # bottom cap
        GL.glRotatef(180, 1, 0, 0)
        GLU.gluDisk(self.q, 0, r1, 32, 1)
        GL.glRotatef(180, 1, 0, 0)
        # the top cap needs flipped and translated
        GL.glPushMatrix()
        GL.glTranslatef(0, 0, z2 - z1)
        GLU.gluDisk(self.q, 0, r2, 32, 1)
        GL.glPopMatrix()
        GL.glPopMatrix()

    def volume(self):
        z1, r1, z2, r2 = self.coords()
        # actually a frustum of a cone
        vol = 3.1415927 / 3.0 * abs(z1 - z2) * (r1 * r1 + r1 * r2 + r2 * r2)
        # print "CylinderZ.volume", vol
        return vol


# give center and radius
class Sphere(CoordsBase):
    def draw(self):
        x, y, z, r = self.coords()
        # need to translate the whole thing to x,y,z
        GL.glPushMatrix()
        GL.glTranslatef(x, y, z)
        # the sphere starts out at the origin
        GLU.gluSphere(self.q, r, 32, 16)
        GL.glPopMatrix()

    def volume(self):
        x, y, z, r = self.coords()
        vol = 1.3333333 * 3.1415927 * r * r * r
        # print "Sphere.volume", vol
        return vol


# triangular plate in XY plane
# specify the corners Z values for each side
class TriangleXY(CoordsBase):
    def draw(self):
        x1, y1, x2, y2, x3, y3, z1, z2 = self.coords()
        x12 = x1 - x2
        y12 = y1 - y2
        x13 = x1 - x3
        y13 = y1 - y3
        cross = x12 * y13 - x13 * y12
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
        x12 = x1 - x2
        y12 = y1 - y2
        x23 = x2 - x3
        y23 = y2 - y3
        x31 = x3 - x1
        y31 = y3 - y1
        GL.glBegin(GL.GL_QUADS)
        # side 1-2
        h = hypot(x12, y12)
        GL.glNormal3f(-y12 / h, x12 / h, 0)
        GL.glVertex3f(x1, y1, z1)
        GL.glVertex3f(x2, y2, z1)
        GL.glVertex3f(x2, y2, z2)
        GL.glVertex3f(x1, y1, z2)
        # side 2-3
        h = hypot(x23, y23)
        GL.glNormal3f(-y23 / h, x23 / h, 0)
        GL.glVertex3f(x2, y2, z1)
        GL.glVertex3f(x3, y3, z1)
        GL.glVertex3f(x3, y3, z2)
        GL.glVertex3f(x2, y2, z2)
        # side 3-1
        h = hypot(x31, y31)
        GL.glNormal3f(-y31 / h, x31 / h, 0)
        GL.glVertex3f(x3, y3, z1)
        GL.glVertex3f(x1, y1, z1)
        GL.glVertex3f(x1, y1, z2)
        GL.glVertex3f(x3, y3, z2)
        GL.glEnd()
        GL.glBegin(GL.GL_TRIANGLES)
        # upper face
        GL.glNormal3f(0, 0, 1)
        GL.glVertex3f(x1, y1, z2)
        GL.glVertex3f(x2, y2, z2)
        GL.glVertex3f(x3, y3, z2)
        # lower face
        GL.glNormal3f(0, 0, -1)
        GL.glVertex3f(x1, y1, z1)
        GL.glVertex3f(x3, y3, z1)
        GL.glVertex3f(x2, y2, z1)
        GL.glEnd()

    def volume(self):
        x1, y1, x2, y2, x3, y3, z1, z2 = self.coords()
        # compute pts 2 and 3 relative to 1 (puts pt1 at origin)
        x2 = x2 - x1
        x3 = x3 - x1
        y2 = y2 - y1
        y3 = y3 - y1
        # compute area of triangle
        area = 0.5 * abs(x2 * y3 - x3 * y2)
        thk = abs(z1 - z2)
        vol = area * thk
        # print "TriangleXY.volume = area * thickness)",vol, area, thk
        return vol


# triangular plate in XZ plane
class TriangleXZ(TriangleXY):
    def coords(self):
        x1, z1, x2, z2, x3, z3, y1, y2 = TriangleXY.coords(self)
        return x1, z1, x2, z2, x3, z3, -y1, -y2

    def draw(self):
        GL.glPushMatrix()
        GL.glRotatef(90, 1, 0, 0)
        # create the triangle in XY plane
        TriangleXY.draw(self)
        # bottom cap
        GL.glPopMatrix()

    def volume(self):
        vol = TriangleXY.volume(self)
        # print " TriangleXZ.volume",vol
        return vol


# triangular plate in YZ plane
class TriangleYZ(TriangleXY):
    def coords(self):
        y1, z1, y2, z2, y3, z3, x1, x2 = TriangleXY.coords(self)
        return z1, y1, z2, y2, z3, y3, -x1, -x2

    def draw(self):
        GL.glPushMatrix()
        GL.glRotatef(90, 0, -1, 0)
        # create the triangle in XY plane
        TriangleXY.draw(self)
        # bottom cap
        GL.glPopMatrix()

    def volume(self):
        vol = TriangleXY.volume(self)
        # print " TriangleYZ.volume",vol
        return vol


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
        astep = ((a2 - a1) / steps) * (pi / 180)
        a1rads = a1 * (pi / 180)
        # positive X end face
        GL.glBegin(GL.GL_QUAD_STRIP)
        GL.glNormal3f(1, 0, 0)
        n = 0
        while n <= steps:
            angle = a1rads + n * astep
            s = sin(angle)
            c = cos(angle)
            GL.glVertex3f(x2, r1 * s, r1 * c)
            GL.glVertex3f(x2, r2 * s, r2 * c)
            n = n + 1

        GL.glEnd()
        # negative X end face
        GL.glBegin(GL.GL_QUAD_STRIP)
        GL.glNormal3f(-1, 0, 0)
        n = 0
        while n <= steps:
            angle = a1rads + n * astep
            s = sin(angle)
            c = cos(angle)
            GL.glVertex3f(x1, r1 * s, r1 * c)
            GL.glVertex3f(x1, r2 * s, r2 * c)
            n = n + 1
        GL.glEnd()
        # inner diameter
        GL.glBegin(GL.GL_QUAD_STRIP)
        n = 0
        while n <= steps:
            angle = a1rads + n * astep
            s = sin(angle)
            c = cos(angle)
            GL.glNormal3f(0, -s, -c)
            GL.glVertex3f(x1, r1 * s, r1 * c)
            GL.glVertex3f(x2, r1 * s, r1 * c)
            n = n + 1
        GL.glEnd()
        # outer diameter
        GL.glBegin(GL.GL_QUAD_STRIP)
        n = 0
        while n <= steps:
            angle = a1rads + n * astep
            s = sin(angle)
            c = cos(angle)
            GL.glNormal3f(0, s, c)
            GL.glVertex3f(x1, r2 * s, r2 * c)
            GL.glVertex3f(x2, r2 * s, r2 * c)
            n = n + 1
        GL.glEnd()
        # end plates
        GL.glBegin(GL.GL_QUADS)
        # first end plate
        angle = a1 * (pi / 180)
        s = sin(angle)
        c = cos(angle)
        GL.glNormal3f(0, -c, s)
        GL.glVertex3f(x1, r2 * s, r2 * c)
        GL.glVertex3f(x2, r2 * s, r2 * c)
        GL.glVertex3f(x2, r1 * s, r1 * c)
        GL.glVertex3f(x1, r1 * s, r1 * c)
        # other end
        angle = a2 * (pi / 180)
        s = sin(angle)
        c = cos(angle)
        GL.glNormal3f(0, c, -s)
        GL.glVertex3f(x1, r2 * s, r2 * c)
        GL.glVertex3f(x2, r2 * s, r2 * c)
        GL.glVertex3f(x2, r1 * s, r1 * c)
        GL.glVertex3f(x1, r1 * s, r1 * c)
        GL.glEnd()

    def volume(self):
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
        height = x2 - x1
        angle = a2 - a1
        area = (angle / 360.0) * pi * (r2 * r2 - r1 * r1)
        vol = area * height
        # print "Arc.volume = angle * area * height",vol, angle, area, height
        return vol


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

        GL.glBegin(GL.GL_QUADS)
        # bottom face
        GL.glNormal3f(0, 0, -1)
        GL.glVertex3f(x2, y1, z1)
        GL.glVertex3f(x1, y1, z1)
        GL.glVertex3f(x1, y2, z1)
        GL.glVertex3f(x2, y2, z1)
        # positive X face
        GL.glNormal3f(1, 0, 0)
        GL.glVertex3f(x2, y1, z1)
        GL.glVertex3f(x2, y2, z1)
        GL.glVertex3f(x2, y2, z2)
        GL.glVertex3f(x2, y1, z2)
        # positive Y face
        GL.glNormal3f(0, 1, 0)
        GL.glVertex3f(x1, y2, z1)
        GL.glVertex3f(x1, y2, z2)
        GL.glVertex3f(x2, y2, z2)
        GL.glVertex3f(x2, y2, z1)
        # negative Y face
        GL.glNormal3f(0, -1, 0)
        GL.glVertex3f(x2, y1, z2)
        GL.glVertex3f(x1, y1, z2)
        GL.glVertex3f(x1, y1, z1)
        GL.glVertex3f(x2, y1, z1)
        # negative X face
        GL.glNormal3f(-1, 0, 0)
        GL.glVertex3f(x1, y1, z1)
        GL.glVertex3f(x1, y1, z2)
        GL.glVertex3f(x1, y2, z2)
        GL.glVertex3f(x1, y2, z1)
        # top face
        GL.glNormal3f(0, 0, 1)
        GL.glVertex3f(x1, y2, z2)
        GL.glVertex3f(x1, y1, z2)
        GL.glVertex3f(x2, y1, z2)
        GL.glVertex3f(x2, y2, z2)
        GL.glEnd()

    def volume(self):
        x1, y1, z1, x2, y2, z2 = self.coords()
        vol = abs((x1 - x2) * (y1 - y2) * (z1 - z2))
        # print "Box.volume", vol
        return vol


# specify the width in X and Y, and the height in Z
# the box is centered on the origin
class BoxCentered(Box):
    def __init__(self, xw, yw, zw):
        Box.__init__(self, -xw / 2.0, -yw / 2.0, -zw / 2.0, xw / 2.0, yw / 2.0, zw / 2.0)


# specify the width in X and Y, and the height in Z
# the box is centered in X and Y, and runs from Z=0 up
# (or down) to the specified Z value
class BoxCenteredXY(Box):
    def __init__(self, xw, yw, zw):
        Box.__init__(self, -xw / 2.0, -yw / 2.0, 0, xw / 2.0, yw / 2.0, zw)


# capture current transformation matrix
# note that this transforms from the current coordinate system
# to the viewport system, NOT to the world system
class Capture(object):
    def __init__(self):
        self.t = []

    def capture(self):
        self.t = GL.glGetDoublev(GL.GL_MODELVIEW_MATRIX)

    def volume(self):
        return 0.0


# function to invert a transform matrix
# based on http://steve.hollasch.net/cgindex/math/matrix/afforthinv.c
# with simplifications since we don't do scaling

# This function inverts a 4x4 matrix that is affine and orthogonal.  In
# other words, the perspective components are [0 0 0 1], and the basis
# vectors are orthogonal to each other.  In addition, the matrix must
# not do scaling

def invert(src):
    # make a copy
    inv = copy.deepcopy(src[:])
    # The inverse of the upper 3x3 is the transpose (since the basis
    # vectors are orthogonal to each other.
    inv[0][1], inv[1][0] = inv[1][0], inv[0][1]
    inv[0][2], inv[2][0] = inv[2][0], inv[0][2]
    inv[1][2], inv[2][1] = inv[2][1], inv[1][2]
    # The inverse of the translation component is just the negation
    # of the translation after dotting with the new upper3x3 rows. */
    inv[3][0] = -(src[3][0] * inv[0][0] + src[3][1] * inv[1][0] + src[3][2] * inv[2][0])
    inv[3][1] = -(src[3][0] * inv[0][1] + src[3][1] * inv[1][1] + src[3][2] * inv[2][1])
    inv[3][2] = -(src[3][0] * inv[0][2] + src[3][1] * inv[1][2] + src[3][2] * inv[2][2])
    return inv


class Hud(object):
    '''head up display - draws a semi-transparent text box.
        use HUD.strs for things that must be updated constantly,
        and HUD.show("stuff") for one-shot things like error messages'''

    def __init__(self, showme=1):
        self.app = []
        self.strs = []
        self.messages = []
        self.showme = 0
        self.fontbase = []

    def show(self, string="xyzzy"):
        self.showme = 1
        if string != "xyzzy":
            self.messages += [str(string)]

    def hide(self):
        self.showme = 0

    def clear(self):
        self.messages = []

    def draw(self):
        drawtext = self.strs + self.messages
        self.lines = len(drawtext)
        # draw head-up-display
        # see axis.py for more font/color configurability
        if ((self.showme == 0) or (self.lines == 0)):
            return

        GL.glMatrixMode(GL.GL_PROJECTION)
        GL.glPushMatrix()
        GL.glLoadIdentity()

        if not self.fontbase:
            self.fontbase = int(self.app.loadbitmapfont("9x15"))
        char_width, char_height = 9, 15
        xmargin, ymargin = 5, 5
        ypos = float(self.app.winfo_height())

        GL.glOrtho(0.0, self.app.winfo_width(), 0.0, ypos, -1.0, 1.0)
        GL.glMatrixMode(GL.GL_MODELVIEW)
        GL.glPushMatrix()
        GL.glLoadIdentity()

        # draw the text box
        maxlen = max([len(p) for p in drawtext])
        box_width = maxlen * char_width
        GL.glDepthFunc(GL.GL_ALWAYS)
        GL.glDepthMask(GL.GL_FALSE)
        GL.glDisable(GL.GL_LIGHTING)
        GL.glEnable(GL.GL_BLEND)
        GL.glEnable(GL.GL_NORMALIZE)
        GL.glBlendFunc(GL.GL_ONE, GL.GL_CONSTANT_ALPHA)
        GL.glColor3f(0.2, 0, 0)
        GL.glBlendColor(0, 0, 0, 0.5)  # rgba
        GL.glBegin(GL.GL_QUADS)
        GL.glVertex3f(0, ypos, 1)  # upper left
        GL.glVertex3f(0, ypos - 2 * ymargin - char_height * len(drawtext), 1)  # lower left
        GL.glVertex3f(box_width + 2 * xmargin, ypos - 2 * ymargin - char_height * len(drawtext), 1)  # lower right
        GL.glVertex3f(box_width + 2 * xmargin, ypos, 1)  # upper right
        GL.glEnd()
        GL.glDisable(GL.GL_BLEND)
        GL.glEnable(GL.GL_LIGHTING)

        # fill the box with text
        maxlen = 0
        ypos -= char_height + ymargin
        i = 0
        GL.glDisable(GL.GL_LIGHTING)
        GL.glColor3f(0.9, 0.9, 0.9)
        for string in drawtext:
            maxlen = max(maxlen, len(string))
            #        if i < len(homed) and homed[i]:
            #                GL.glRasterPos2i(6, ypos)
            #                GL.glBitmap(13, 16, 0, 3, 17, 0, homeicon)
            GL.glRasterPos2i(xmargin, int(ypos))
            for char in string:
                GL.glCallList(self.fontbase + ord(char))
            #        if i < len(homed) and limit[i]:
            #                GL.glBitmap(13, 16, -5, 3, 17, 0, limiticon)
            ypos -= char_height
            i = i + 1
        GL.glDepthFunc(GL.GL_LESS)
        GL.glDepthMask(GL.GL_TRUE)
        GL.glEnable(GL.GL_LIGHTING)

        GL.glPopMatrix()
        GL.glMatrixMode(GL.GL_PROJECTION)
        GL.glPopMatrix()
        GL.glMatrixMode(GL.GL_MODELVIEW)


class Color(Collection):
    def __init__(self, color, parts):
        self.color = color
        Collection.__init__(self, parts)

    def apply(self):
        GL.glPushAttrib(GL.GL_LIGHTING_BIT)
        GL.glMaterialfv(GL.GL_FRONT_AND_BACK, GL.GL_AMBIENT_AND_DIFFUSE, self.color)

    def unapply(self):
        GL.glPopAttrib()


class AsciiSTL:
    def __init__(self, filename=None, data=None):
        if data is None:
            data = open(filename, "r")
        elif isinstance(data, str):
            data = data.split("\n")
        self.list = None
        t = []
        n = [0, 0, 0]
        self.d = d = []
        for line in data:
            if line.find("normal") != -1:
                line = line.split()
                x, y, z = list(map(float, line[-3:]))
                n = [x, y, z]
            elif line.find("vertex") != -1:
                line = line.split()
                x, y, z = list(map(float, line[-3:]))
                t.append([x, y, z])
                if len(t) == 3:
                    if n == [0, 0, 0]:
                        dx1 = t[1][0] - t[0][0]
                        dy1 = t[1][1] - t[0][1]
                        dz1 = t[1][2] - t[0][2]
                        dx2 = t[2][0] - t[0][0]
                        dy2 = t[2][1] - t[0][1]
                        dz2 = t[2][2] - t[0][2]
                        n = [dy1 * dz2 - dy2 * dz1, dz1 * dx2 - dz2 * dx1, dy1 * dx2 - dy2 * dx1]
                    d.append((n, t))
                    t = []
                    n = [0, 0, 0]

    def draw(self):
        if self.list is None:
            # OpenGL isn't ready yet in __init__ so the display list
            # is created during the first draw
            self.list = GL.glGenLists(1)
            GL.glNewList(self.list, GL.GL_COMPILE)
            GL.glBegin(GL.GL_TRIANGLES)
            for n, t in self.d:
                GL.glNormal3f(*n)
                GL.glVertex3f(*t[0])
                GL.glVertex3f(*t[1])
                GL.glVertex3f(*t[2])
            GL.glEnd()
            GL.glEndList()
            del self.d
        GL.glCallList(self.list)


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

        #        print v[:5]
        #        print vn[:5]
        #        print f[:5]

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
            self.list = GL.glGenLists(1)
            GL.glNewList(self.list, GL.GL_COMPILE)
            GL.glDisable(GL.GL_CULL_FACE)
            GL.glBegin(GL.GL_TRIANGLES)
            # print "obj", len(self.f)
            for f in self.f:
                for v, t, n in f:
                    if n:
                        GL.glNormal3f(*self.vn[n - 1])
                    GL.glVertex3f(*self.v[v - 1])
            GL.glEnd()
            GL.glEndList()
            del self.v
            del self.vn
            del self.f
        GL.glCallList(self.list)
