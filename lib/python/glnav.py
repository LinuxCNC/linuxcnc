from minigl import *
import math
import array, itertools

def use_pango_font(font, start, count):
    import pango, cairo, pangocairo
    fontDesc = pango.FontDescription(font)
    a = array.array('b', itertools.repeat(0, 256*256))
    surface = cairo.ImageSurface.create_for_data(a, cairo.FORMAT_A8, 256, 256)
    context = pangocairo.CairoContext(cairo.Context(surface))
    layout = context.create_layout()
    fontmap = pangocairo.cairo_font_map_get_default()
    font = fontmap.load_font(fontmap.create_context(), fontDesc)
    layout.set_font_description(fontDesc)
    metrics = font.get_metrics()
    descent = metrics.get_descent()
    d = pango.PIXELS(descent)
    linespace = metrics.get_ascent() + metrics.get_descent()
    width = metrics.get_approximate_char_width()

    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT)
    glPixelStorei(GL_UNPACK_SWAP_BYTES, 0)
    glPixelStorei(GL_UNPACK_LSB_FIRST, 1)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 256)
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 256)
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0)
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0)
    glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1)
    glPixelZoom(1, -1)

    base = glGenLists(count)
    for i in range(count):
        ch = unichr(start+i)
        layout.set_text(ch)
        w, h = layout.get_size()
        context.save()
        context.new_path()
        context.rectangle(0, 0, 256, 256)
        context.set_source_rgba(0., 0., 0., 0.)
        context.set_operator (cairo.OPERATOR_SOURCE);
        context.paint()
        context.restore()

        context.save()
        context.set_source_rgba(1., 1., 1., 1.)
        context.set_operator (cairo.OPERATOR_SOURCE);
        context.move_to(0, 0)
        context.update_layout(layout)
        context.show_layout(layout)
        context.restore()

        w, h = pango.PIXELS(w), pango.PIXELS(h)
        glNewList(base+i, GL_COMPILE)
        glBitmap(0, 0, 0, 0, 0, h-d, '');
        if w and h: glDrawPixels(w, h, GL_LUMINANCE, GL_UNSIGNED_BYTE, a)
        glBitmap(0, 0, 0, 0, w, -h+d, '');
        glEndList()

    glPopClientAttrib()
    return base, pango.PIXELS(width), pango.PIXELS(linespace)

def glTranslateScene(w, s, x, y, mousex, mousey):
    glMatrixMode(GL_MODELVIEW)
    mat = glGetDoublev(GL_MODELVIEW_MATRIX)
    glLoadIdentity()
    glTranslatef(s * (x - mousex), s * (mousey - y), 0.0)
    glMultMatrixd(mat)


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

    glMatrixMode(GL_MODELVIEW)

    glTranslatef(xcenter, ycenter, zcenter)
    mat = glGetDoublev(GL_MODELVIEW_MATRIX)

    glLoadIdentity()
    tx, ty, tz = mat[12:15]
    glTranslatef(tx, ty, tz)
    glRotatef(snap(lat), *w.rotation_vectors[0])
    glRotatef(snap(lon), *w.rotation_vectors[1])
    glTranslatef(-xcenter, -ycenter, -zcenter)
    w.lat = lat
    w.lon = lon

def sub(x, y):
    return map(lambda a, b: a-b, x, y)

def dot(x, y):
    t = 0
    for i in range(len(x)):
        t = t + x[i]*y[i]
    return t

def glDistFromLine(x, p1, p2):
    f = map(lambda x, y: x-y, p2, p1)
    g = map(lambda x, y: x-y, x, p1)
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

        self.lat = 0
        self.minlat = -90
        self.maxlat = 90

    # This should almost certainly be part of some derived class.
    # But I have put it here for convenience.
    def basic_lighting(self):
        """\
        Set up some basic lighting (single infinite light source).

        Also switch on the depth buffer."""

        self.activate()
        glLightfv(GL_LIGHT0, GL_POSITION, (1, -1, 1, 0))
        glLightfv(GL_LIGHT0, GL_AMBIENT, (.4, .4, .4, 1))
        glLightfv(GL_LIGHT0, GL_DIFFUSE, (.6, .6, .6, 1))
        glEnable(GL_LIGHTING)
        glEnable(GL_LIGHT0)
        glDepthFunc(GL_LESS)
        glEnable(GL_DEPTH_TEST)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()


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


    def reset(self):
        """Reset rotation matrix for this widget."""

        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()
        self._redraw()


    def recordMouse(self, x, y):
        self.xmouse = x
        self.ymouse = y

    def startRotate(self, x, y):
        self.recordMouse(x, y)

    def scale(self, x, y):
        """Scale the scene.  Achieved by moving the eye position.

        Dragging up zooms in, while dragging down zooms out
        """
        scale = 1 - 0.01 * (event.y - self.ymouse)
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

        self.activate()
        glRotateScene(self, 0.5, self.xcenter, self.ycenter, self.zcenter, x, y, self.xmouse, self.ymouse)
        self._redraw()
        self.recordMouse(x, y)

    def translate(self, x, y):
        """Perform translation of scene."""

        self.activate()

        # Scale mouse translations to object viewplane so object tracks with mouse
        win_height = max( 1,self.winfo_height() )
        obj_c     = ( self.xcenter, self.ycenter, self.zcenter )
        win     = gluProject( obj_c[0], obj_c[1], obj_c[2])
        obj     = gluUnProject( win[0], win[1] + 0.5 * win_height, win[2])
        dist       = math.sqrt( v3distsq( obj, obj_c ) )
        scale     = abs( dist / ( 0.5 * win_height ) )

        glTranslateScene(self, scale, x, y, self.xmouse, self.ymouse)
        self._redraw()
        self.recordMouse(x, y)


    def set_viewangle(self, lat, lon):
        self.lat = lat
        self.lon = lon
        glRotateScene(self, 0.5, self.xcenter, self.ycenter, self.zcenter, 0, 0, 0, 0)
        self.tkRedraw()

    def zoomin(self):
        self.distance = self.distance / 1.1
        self._redraw()

    def zoomout(self):
        self.distance = self.distance * 1.1
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


