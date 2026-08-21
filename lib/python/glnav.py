import math

import numpy as np

def use_pango_font(font, start, count, will_call_prepost=False):
    """Build a glyph-atlas font for the OpenGL 3.3 core overlay renderer.

    Signature preserved for callers (axis.py, gremlin.py, qt5_graphics.py):
    returns ``(handle, char_width, line_space)``. The handle is now an opaque
    ``rs274.glcanon_gl.GlyphAtlas`` (a texture atlas + overlay shader) instead
    of a per-glyph display-list base; consumers pass it through get_font_info()
    to the shared overlay pass, which draws text as textured quads.
    """
    from rs274 import glcanon_gl
    atlas = glcanon_gl.build_atlas(font, start, count)
    return atlas, atlas.char_width, atlas.line_space


def identity_matrix():
    return np.identity(4, dtype=np.float64)


def multiply(*matrices):
    """Multiply 4x4 matrices using OpenGL's column-vector convention."""
    result = identity_matrix()
    for matrix in matrices:
        result = result @ np.asarray(matrix, dtype=np.float64)
    return result


def translation_matrix(x, y, z):
    result = identity_matrix()
    result[:3, 3] = (x, y, z)
    return result


def rotation_matrix(angle, x, y, z):
    axis = np.asarray((x, y, z), dtype=np.float64)
    length = np.linalg.norm(axis)
    if not length:
        return identity_matrix()
    x, y, z = axis / length
    c = math.cos(math.radians(angle))
    s = math.sin(math.radians(angle))
    d = 1.0 - c
    return np.array(((x*x*d+c,   x*y*d-z*s, x*z*d+y*s, 0.0),
                     (y*x*d+z*s, y*y*d+c,   y*z*d-x*s, 0.0),
                     (z*x*d-y*s, z*y*d+x*s, z*z*d+c,   0.0),
                     (0.0,       0.0,       0.0,       1.0)), dtype=np.float64)


def perspective_matrix(fovy, aspect, near, far):
    f = 1.0 / math.tan(math.radians(fovy) / 2.0)
    return np.array(((f / aspect, 0.0, 0.0,                         0.0),
                     (0.0,        f,   0.0,                         0.0),
                     (0.0,        0.0, (far + near) / (near - far), 2*far*near / (near - far)),
                     (0.0,        0.0, -1.0,                        0.0)), dtype=np.float64)


def ortho_matrix(left, right, bottom, top, near, far):
    return np.array(((2.0 / (right - left), 0.0, 0.0, -(right + left) / (right - left)),
                     (0.0, 2.0 / (top - bottom), 0.0, -(top + bottom) / (top - bottom)),
                     (0.0, 0.0, -2.0 / (far - near), -(far + near) / (far - near)),
                     (0.0, 0.0, 0.0, 1.0)), dtype=np.float64)


def project(point, modelview, projection, viewport):
    clip = np.asarray(projection) @ np.asarray(modelview) @ np.append(point, 1.0)
    ndc = clip[:3] / clip[3]
    x, y, width, height = viewport
    return np.array((x + (ndc[0] + 1.0) * width / 2.0,
                     y + (ndc[1] + 1.0) * height / 2.0,
                     (ndc[2] + 1.0) / 2.0))


def unproject(point, modelview, projection, viewport):
    x, y, width, height = viewport
    ndc = np.array(((point[0] - x) * 2.0 / width - 1.0,
                    (point[1] - y) * 2.0 / height - 1.0,
                    point[2] * 2.0 - 1.0, 1.0))
    obj = np.linalg.inv(np.asarray(projection) @ np.asarray(modelview)) @ ndc
    return obj[:3] / obj[3]


def glTranslateScene(w, s, x, y, mousex, mousey):
    zoom_boost = max(1.0, (5.0 / w.distance) ** 0.6)
    s *= zoom_boost
    s = max(0.005, s)
    w.modelview = multiply(translation_matrix(s * (x - mousex),
                                              s * (mousey - y), 0.0),
                           w.modelview)

def glRotateScene(w, s, xcenter, ycenter, zcenter, x, y, mousex, mousey):
    def snap(a):
        m = a%90
        if m < 3:
            return a-m
        elif m > 87:
            return a-m+90
        else:
            return a

    lat = min(w.maxlat, max(w.minlat, w.lat + (y - mousey) * .5))
    lon = (w.lon + (x - mousex) * .5) % 360

    # The legacy sequence preserved the translated origin while replacing the
    # rotational part of the modelview matrix.  Express that sequence directly.
    translated = multiply(w.modelview, translation_matrix(xcenter, ycenter, zcenter))
    tx, ty, tz = translated[:3, 3]
    w.modelview = multiply(translation_matrix(tx, ty, tz),
                           rotation_matrix(snap(lat), *w.rotation_vectors[0]),
                           rotation_matrix(snap(lon), *w.rotation_vectors[1]),
                           translation_matrix(-xcenter, -ycenter, -zcenter))
    w.lat = lat
    w.lon = lon

def sub(x, y):
    return list(map(lambda a, b: a-b, x, y))

def dot(x, y):
    t = 0
    for i in range(len(x)):
        t = t + x[i]*y[i]
    return t

def glDistFromLine(x, p1, p2):
    f = list(map(lambda x, y: x-y, p2, p1))
    g = list(map(lambda x, y: x-y, x, p1))
    return dot(g, g) - dot(f, g)**2/dot(f, f)

def v3distsq(a,b):
    d = ( a[0] - b[0], a[1] - b[1], a[2] - b[2] )
    return d[0]*d[0] + d[1]*d[1] + d[2]*d[2]

class GlNavBase:
    rotation_vectors = [(1.,0.,0.), (0., 0., 1.)]

    def __init__(self):
        # Current coordinates of the mouse.
        self.xmouse = 0
        self.ymouse = 0

        # Where we are centering.
        self.xcenter = 0.0
        self.ycenter = 0.0
        self.zcenter = 0.0

        # The _back color
        self.r_back = 1.
        self.g_back = 0.
        self.b_back = 1.

        # Where the eye is
        self.distance = 10.0

        # Field of view in y direction
        self.fovy = 30.0

        # Position of clipping planes.
        self.near = 0.1
        self.far = 1000.0

        # View settings
        self.perspective = 0
        self.lat = 0
        self.lon = 0
        self.minlat = -90
        self.maxlat = 90

        # keep track of total translations
        # since last view reset
        self._totalx = 0.0
        self._totaly = 0.0
        self.modelview = identity_matrix()

    def is_lathe(self):
        # preview widgets that need it (gremlin, axis, qt5_graphics) override this
        return False

    def basic_lighting(self):
        """\
        Reset the camera for the first expose.

        Widgets that own a fixed-function context (rs274.OpenGLTk.Opengl,
        vismach) override this to set their lighting and depth state up first;
        the camera itself has no GL state to configure."""

        self.modelview = identity_matrix()


    def set_background(self, r, g, b):
        """Change the background colour of the widget."""

        self.r_back = r
        self.g_back = g
        self.b_back = b

        self._redraw()


    def set_centerpoint(self, x, y, z):
        """Set the new center point for the model.
        This is where we are looking."""

        self.xcenter = x
        self.ycenter = y
        self.zcenter = z

        self._redraw()


    def set_latitudelimits(self, minlat, maxlat):
        """Set the new "latitude" limits for rotations."""

        if maxlat > 180:
            return
        if minlat < -180:
            return
        if maxlat <= minlat:
            return
        self.maxlat = maxlat
        self.minlat = minlat

        self._redraw()


    def set_eyepoint(self, distance):
        """Set how far the eye is from the position we are looking."""

        self.distance = distance
        self._redraw()

    def set_eyepoint_from_extents(self, e1, e2):
        """Set how far the eye is from the position we are looking
        based on the screen width and height of a subject."""
        w = self.winfo_width()
        h = self.winfo_height()

        ztran = max(2.0, e1, e2 * w/h) ** 2
        self.set_eyepoint(ztran - self.zcenter)

    def reset(self):
        """Reset rotation matrix for this widget."""

        self.modelview = identity_matrix()
        self._redraw()
        # zero the translations - we will be recentering
        self._totalx = 0.0
        self._totaly = 0.0

    def recordMouse(self, x, y):
        self.xmouse = x
        self.ymouse = y

    def startRotate(self, x, y):
        self.recordMouse(x, y)

    def scale(self, x, y):
        """Scale the scene.  Achieved by moving the eye position.

        Dragging up zooms in, while dragging down zooms out
        """
        scale = 1 - 0.01 * (y - self.ymouse)
        # do some sanity checks, scale no more than
        # 1:1000 on any given click+drag
        if scale < 0.001:
            scale = 0.001
        elif scale > 1000:
            scale = 1000
        newdistance = self.distance * scale
        if newdistance < 1e-30 or newdistance > 1e30:
            return
        self.distance = newdistance
        self._redraw()
        self.recordMouse(x, y)

    def rotate(self, x, y):
        """Perform rotation of scene."""

        self.perspective = True
        glRotateScene(self, 0.5, self.xcenter, self.ycenter, self.zcenter, x, y, self.xmouse, self.ymouse)
        if self.is_lathe():
            self.modelview = multiply(self.modelview,
                                      rotation_matrix(90, 1, 0, 0),
                                      rotation_matrix(90, 0, 1, 0))
        self._redraw()
        self.recordMouse(x, y)

    def translate(self, x, y):
        """Perform translation of scene."""

        # Scale mouse translations to object viewplane so object tracks with mouse
        win_width = max(1, self.winfo_width())
        win_height = max(1, self.winfo_height())
        obj_c     = ( self.xcenter, self.ycenter, self.zcenter )
        projection = self.get_projection_matrix(win_width, win_height)
        win     = project(obj_c, self.modelview, projection, (0, 0, win_width, win_height))
        obj     = unproject((win[0], win[1] + 0.5 * win_height, win[2]),
                            self.modelview, projection, (0, 0, win_width, win_height))
        dist       = math.sqrt( v3distsq( obj, obj_c ) )
        scale     = abs( dist / ( 0.5 * win_height ) )

        glTranslateScene(self, scale, x, y, self.xmouse, self.ymouse)
        # keep track of all translations since view reset
        self._totalx = self._totalx + (x - self.xmouse)
        self._totaly = self._totaly - (self.ymouse - y)

        self._redraw()
        self.recordMouse(x, y)


    def set_viewangle(self, lat, lon, forcerotate=0):
        self.lat = lat
        self.lon = lon
        if forcerotate or self.perspective:
            glRotateScene(self, 0.5, self.xcenter, self.ycenter, self.zcenter, 0, 0, 0, 0)
        self._redraw()

    def get_viewangle(self):
        return self.lat, self.lon

    def get_zoom_distance(self):
        data = self.distance
        return data

    def set_zoom_distance(self,data):
        self.distance = data
        self._redraw()

    def zoomin(self):
        self.distance = self.distance / 1.1 - 0.2
        if self.distance < 0.1:
            self.distance = 0.1
        self._redraw()

    def zoomout(self):
        self.distance = self.distance * 1.1 + 0.2
        if self.distance > 6000:
            self.distance = 6000
        self._redraw()

    def startZoom(self, y):
        self.y0 = y
        self.original_zoom = self.distance

    def continueZoom(self, y):
        dy = y - self.y0
        self.distance = self.original_zoom * pow(1.25, dy / 16.)
        self._redraw()

    def getRotateMode(self): return False

    def translateOrRotate(self, x, y):
        if self.getRotateMode():
            self.rotate(x, y)
        else:
            self.translate(x, y)

    def rotateOrTranslate(self, x, y):
        if not self.getRotateMode():
            self.rotate(x, y)
        else:
            self.translate(x, y)

    # can be used to get current view position
    def get_total_translation(self):
        return self._totalx, self._totaly

    def get_projection_matrix(self, width, height):
        """Return the complete legacy projection, including its eye translation."""
        width = max(1, width)
        height = max(1, height)
        if self.perspective:
            return multiply(perspective_matrix(self.fovy, float(width) / height,
                                               self.near, self.far + self.distance),
                            translation_matrix(0.0, 0.0, -self.distance))
        k = abs(self.distance or 1.0) ** .55555
        return multiply(ortho_matrix(-k, k, -k * height / width, k * height / width,
                                     -1000.0, 1000.0),
                        translation_matrix(0.0, 0.0, -1.0))

    def get_modelview_matrix(self):
        return self.modelview.copy()

    def translate_modelview(self, x, y, z):
        """Compose a translation onto the camera's modelview matrix.

        This is the supported replacement for the legacy ``glTranslatef()``
        against the fixed-function matrix stack: both camera consumers reload
        the GL modelview from this matrix every frame, so a translation issued
        outside the camera is discarded.  The translation is post-multiplied,
        matching what ``glTranslatef()`` did to the current matrix, so call
        sites port with no numeric change.

        Deliberately does not redraw.  This is a composition primitive rather
        than a settled camera state, and callers follow it with
        ``set_eyepoint*()`` or an explicit refresh, which redraws anyway.
        """
        self.modelview = multiply(self.modelview, translation_matrix(x, y, z))

    def set_view_x(self):
        self.reset()
        self.modelview = multiply(self.modelview, rotation_matrix(-90, 0, 1, 0), rotation_matrix(-90, 1, 0, 0))
        mid, size = self.extents_info()
        self.modelview = multiply(self.modelview, translation_matrix(-mid[0], -mid[1], -mid[2]))
        self.set_eyepoint_from_extents(size[1], size[2])
        self.perspective = False
        self.lat = -90
        self.lon = 270
        self._redraw()

    def set_view_y(self):
        self.reset()
        self.modelview = multiply(self.modelview, rotation_matrix(-90, 1, 0, 0))
        if self.is_lathe():
            self.modelview = multiply(self.modelview, rotation_matrix(90, 0, 1, 0))
        mid, size = self.extents_info()
        self.modelview = multiply(self.modelview, translation_matrix(-mid[0], -mid[1], -mid[2]))
        self.set_eyepoint_from_extents(size[0], size[2])
        self.perspective = False
        self.lat = -90
        self.lon = 0
        self._redraw()

        # lathe backtool display
    def set_view_y2(self):
        self.reset()
        self.modelview = multiply(self.modelview, rotation_matrix(90, 1, 0, 0), rotation_matrix(90, 0, 1, 0))
        mid, size = self.extents_info()
        self.modelview = multiply(self.modelview, translation_matrix(-mid[0], -mid[1], -mid[2]))
        self.set_eyepoint_from_extents(size[0], size[2])
        self.perspective = False
        self.lat = -90
        self.lon = 0
        self._redraw()

    def set_view_z(self):
        self.reset()
        mid, size = self.extents_info()
        self.modelview = multiply(self.modelview, translation_matrix(-mid[0], -mid[1], -mid[2]))
        self.set_eyepoint_from_extents(size[0], size[1])
        self.perspective = False
        self.lat = self.lon = 0
        self._redraw()

    def set_view_z2(self):
        self.reset()
        self.modelview = multiply(self.modelview, rotation_matrix(-90, 0, 0, 1))
        mid, size = self.extents_info()
        self.modelview = multiply(self.modelview, translation_matrix(-mid[0], -mid[1], -mid[2]))
        self.set_eyepoint_from_extents(size[1], size[0])
        self.perspective = False
        self.lat = 0
        self.lon = 270
        self._redraw()

    def set_view_p(self):
        self.reset()
        self.perspective = True
        mid, size = self.extents_info()
        self.modelview = multiply(self.modelview, translation_matrix(-mid[0], -mid[1], -mid[2]))
        size = (size[0] ** 2 + size[1] ** 2 + size[2] ** 2) ** .5
        if size > 1e99: size = 5. # in case there are no moves in the preview
        w = self.winfo_width()
        h = self.winfo_height()
        fovx = self.fovy if h == 0 else self.fovy * w / h
        fov = min(fovx, self.fovy)
        self.set_eyepoint((size * 1.1 + 1.0) / 2 / math.sin ( fov * math.pi / 180 / 2))
        self.lat = -60
        self.lon = 335
        glRotateScene(self, 1.0, mid[0], mid[1], mid[2], 0, 0, 0, 0)
        if self.is_lathe():
            self.modelview = multiply(self.modelview,
                                      rotation_matrix(90, 1, 0, 0),
                                      rotation_matrix(90, 0, 1, 0))
        self._redraw()

# vim:ts=8:sts=4:sw=4:et:
