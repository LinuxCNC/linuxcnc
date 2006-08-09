#!/usr/bin/env python

# This file is from PyOpenGL-2.0.1.07.  That distribution's license is
# """License :: OSI Approved :: BSD License""",

# A class that creates an opengl widget.
# Mike Hartshorn
# Department of Chemistry
# University of York, UK
# 

from minigl import *
from Tkinter import _default_root
from Tkinter import *
import math
import os,sys
import _togl

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

    lat = min(0, max(-90, w.lat + (y - mousey) * .5))
    lon = (w.lon + (x - mousex) * .5) % 360

    glMatrixMode(GL_MODELVIEW)

    glTranslatef(xcenter, ycenter, zcenter)
    mat = glGetDoublev(GL_MODELVIEW_MATRIX)

    glLoadIdentity()
    tx, ty, tz = mat[12:15]
    glTranslatef(tx, ty, tz)
    glRotatef(snap(lat), 1., 0., 0.)
    glRotatef(snap(lon), 0., 0., 1.)
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


# Keith Junius <junius@chem.rug.nl> provided many changes to Togl
TOGL_NORMAL = 1
TOGL_OVERLAY = 2

def v3distsq(a,b):
    d = ( a[0] - b[0], a[1] - b[1], a[2] - b[2] )
    return d[0]*d[0] + d[1]*d[1] + d[2]*d[2]

class Togl(Widget):
    """
    Togl Widget
    Keith Junius
    Department of Biophysical Chemistry
    University of Groningen, The Netherlands
    Very basic widget which provides access to Togl functions.
    N.B. this requires a modified version of Togl 1.5 to gain access to the
    extra functionality. This support should be included in Togl 1.6, I hope.
    """


    def __init__(self, master=None, cnf={}, **kw):
        _togl.install(master.tk)
        Widget.__init__(self, master, 'togl', cnf, kw)


    def render(self):
        return


    def swapbuffers(self):
        self.tk.call(self._w, 'swapbuffers')


    def makecurrent(self):
        self.tk.call(self._w, 'makecurrent')


    def alloccolor(self, red, green, blue):
        return self.tk.getint(self.tk.call(self._w, 'alloccolor', red, green, blue))


    def freecolor(self, index):
        self.tk.call(self._w, 'freecolor', index)


    def setcolor(self, index, red, green, blue):
        self.tk.call(self._w, 'setcolor', index, red, green, blue)


    def loadbitmapfont(self, fontname):
        return self.tk.getint(self.tk.call(self._w, 'loadbitmapfont', fontname))


    def unloadbitmapfont(self, fontbase):
        self.tk.call(self._w, 'unloadbitmapfont', fontbase)


    def uselayer(self, layer):
        self.tk.call(self._w, 'uselayer', layer)


    def showoverlay(self):
        self.tk.call(self._w, 'showoverlay')


    def hideoverlay(self):
        self.tk.call(self._w, 'hideoverlay')


    def existsoverlay(self):
        return self.tk.getboolean(self.tk.call(self._w, 'existsoverlay'))


    def getoverlaytransparentvalue(self):
        return self.tk.getint(self.tk.call(self._w, 'getoverlaytransparentvalue'))


    def ismappedoverlay(self):
        return self.tk.getboolean(self.tk.call(self._w, 'ismappedoverlay'))


    def alloccoloroverlay(self, red, green, blue):
        return self.tk.getint(self.tk.call(self._w, 'alloccoloroverlay', red, green, blue))


    def freecoloroverlay(self, index):
        self.tk.call(self._w, 'freecoloroverlay', index)



class RawOpengl(Togl, Misc):
    """Widget without any sophisticated bindings\
    by Tom Schwaller"""


    def __init__(self, master=None, cnf={}, **kw):
        Togl.__init__(self, master, cnf, **kw)
        self.bind('<Map>', self.tkMap)
        self.bind('<Expose>', self.tkExpose)
        self.bind('<Configure>', self.tkExpose)


    def tkRedraw(self, *dummy):
        # This must be outside of a pushmatrix, since a resize event
        # will call redraw recursively. 
        self.update_idletasks()
        self.tk.call(self._w, 'makecurrent')
        _mode = glGetDoublev(GL_MATRIX_MODE)
        try:
            glMatrixMode(GL_PROJECTION)
            glPushMatrix()
            try:
                self.redraw()
                glFlush()
            finally:
                glPopMatrix()
        finally:
            glMatrixMode(_mode)
        self.tk.call(self._w, 'swapbuffers')


    def tkMap(self, *dummy):
        self.tkExpose()


    def tkExpose(self, *dummy):
        self.tkRedraw()




class Opengl(RawOpengl):
    """\
Tkinter bindings for an Opengl widget.
Mike Hartshorn
Department of Chemistry
University of York, UK
http://www.yorvic.york.ac.uk/~mjh/
"""

    def __init__(self, master=None, cnf={}, **kw):
        """\
        Create an opengl widget.
        Arrange for redraws when the window is exposed or when
        it changes size."""

        #Widget.__init__(self, master, 'togl', cnf, kw)
        apply(RawOpengl.__init__, (self, master, cnf), kw)
        self.initialised = 0

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

        # Is the widget allowed to autospin?
        self.autospin_allowed = 0

        # Is the widget currently autospinning?
        self.autospin = 0

        # Basic bindings for the virtual trackball
        self.bind('<Map>', self.tkMap)
        self.bind('<Expose>', self.tkExpose)
        self.bind('<Configure>', self.tkExpose)
        self.bind('<Shift-Button-1>', self.tkHandlePick)
        #self.bind('<Button-1><ButtonRelease-1>', self.tkHandlePick)
        self.bind('<Button-1>', self.tkRecordMouse)
        self.bind('<B1-Motion>', self.tkTranslate)
        self.bind('<Button-2>', self.StartRotate)
        self.bind('<B2-Motion>', self.tkRotate)
        self.bind('<ButtonRelease-2>', self.tkAutoSpin)
        self.bind('<Button-3>', self.tkRecordMouse)
        self.bind('<B3-Motion>', self.tkScale)

        self.lat = 0
        self.lon = 0

    def help(self):
        """Help for the widget."""

        import Dialog
        d = Dialog.Dialog(None, {'title': 'Viewer help',
                     'text': 'Button-1: Translate\n'
                         'Button-2: Rotate\n'
                         'Button-3: Zoom\n'
                         'Reset: Resets transformation to identity\n',
                     'bitmap': 'questhead',
                     'default': 0,
                     'strings': ('Done', 'Ok')})


    def activate(self):
        """Cause this Opengl widget to be the current destination for drawing."""

        self.tk.call(self._w, 'makecurrent')


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

        self.tkRedraw()


    def set_centerpoint(self, x, y, z):
        """Set the new center point for the model.
        This is where we are looking."""

        self.xcenter = x
        self.ycenter = y
        self.zcenter = z

        self.tkRedraw()


    def set_eyepoint(self, distance):
        """Set how far the eye is from the position we are looking."""

        self.distance = distance
        self.tkRedraw()


    def reset(self):
        """Reset rotation matrix for this widget."""

        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()
        self.tkRedraw()


    def tkHandlePick(self, event):
        """Handle a pick on the scene."""

        if hasattr(self, 'pick'):
            # here we need to use glu.UnProject

            # Tk and X have their origin top left, 
            # while Opengl has its origin bottom left.
            # So we need to subtract y from the window height to get
            # the proper pick position for Opengl

            realy = self.winfo_height() - event.y

            p1 = gluUnProject(event.x, realy, 0.)
            p2 = gluUnProject(event.x, realy, 1.)

            if self.pick(self, p1, p2):
                """If the pick method returns true we redraw the scene."""

                self.tkRedraw()


    def tkRecordMouse(self, event):
        """Record the current mouse position."""

        self.xmouse = event.x
        self.ymouse = event.y


    def StartRotate(self, event):
        # Switch off any autospinning if it was happening

        self.autospin = 0
        self.tkRecordMouse(event)


    def tkScale(self, event):
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
        self.tkRedraw()
        self.tkRecordMouse(event)


    def do_AutoSpin(self):
        s = 0.5
        self.activate()

        glRotateScene(self, 0.5, self.xcenter, self.ycenter, self.zcenter, self.yspin, self.xspin, 0, 0)
        self.tkRedraw()

        if self.autospin:
            self.after(10, self.do_AutoSpin)


    def tkAutoSpin(self, event):
        """Perform autospin of scene."""

        self.after(4)
        self.update_idletasks()

        # This could be done with one call to pointerxy but I'm not sure
        # it would any quicker as we would have to split up the resulting
        # string and then conv

        x = self.tk.getint(self.tk.call('winfo', 'pointerx', self._w))
        y = self.tk.getint(self.tk.call('winfo', 'pointery', self._w))

        if self.autospin_allowed:
            if x != event.x_root and y != event.y_root:
                self.autospin = 1

        self.yspin = x - event.x_root
        self.xspin = y - event.y_root

        self.after(10, self.do_AutoSpin)


    def tkRotate(self, event):
        """Perform rotation of scene."""

        self.activate()
        glRotateScene(self, 0.5, self.xcenter, self.ycenter, self.zcenter, event.x, event.y, self.xmouse, self.ymouse)
        self.tkRedraw()
        self.tkRecordMouse(event)


    def tkTranslate(self, event):
        """Perform translation of scene."""

        self.activate()

        # Scale mouse translations to object viewplane so object tracks with mouse
        win_height = max( 1,self.winfo_height() )
        obj_c     = ( self.xcenter, self.ycenter, self.zcenter )
        win     = gluProject( obj_c[0], obj_c[1], obj_c[2])
        obj     = gluUnProject( win[0], win[1] + 0.5 * win_height, win[2])
        dist       = math.sqrt( v3distsq( obj, obj_c ) )
        scale     = abs( dist / ( 0.5 * win_height ) )

        glTranslateScene(self, scale, event.x, event.y, self.xmouse, self.ymouse)
        self.tkRedraw()
        self.tkRecordMouse(event)


    def tkRedraw(self, *dummy):
        """Cause the opengl widget to redraw itself."""

        if not self.initialised: return
        self.activate()

        glPushMatrix()          # Protect our matrix
        self.update_idletasks()
        self.activate()
        w = self.winfo_width()
        h = self.winfo_height()
        glViewport(0, 0, w, h)

        # Clear the background and depth buffer.
        glClearColor(self.r_back, self.g_back, self.b_back, 0.)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(self.fovy, float(w)/float(h), self.near, self.far)

        if 0:
            # Now translate the scene origin away from the world origin
            glMatrixMode(GL_MODELVIEW)
            mat = glGetDoublev(GL_MODELVIEW_MATRIX)
            glLoadIdentity()
            glTranslatef(-self.xcenter, -self.ycenter, -(self.zcenter+self.distance))
            glMultMatrixd(mat)
        else:
            gluLookAt(self.xcenter, self.ycenter, self.zcenter + self.distance,
                self.xcenter, self.ycenter, self.zcenter,
                0., 1., 0.)
            glMatrixMode(GL_MODELVIEW)
    
        # Call objects redraw method.
        self.redraw(self)
        glFlush()               # Tidy up
        glPopMatrix()           # Restore the matrix

        self.tk.call(self._w, 'swapbuffers')


    def tkMap(self, *dummy):
        """Cause the opengl widget to redraw itself."""

        self.tkExpose()


    def tkExpose(self, *dummy):
        """Redraw the widget.
        Make it active, update tk events, call redraw procedure and
        swap the buffers.  Note: swapbuffers is clever enough to
        only swap double buffered visuals."""

        self.activate()
        if not self.initialised:
            self.basic_lighting()
            self.initialised = 1
        self.tkRedraw()


    def tkPrint(self, file):
        """Turn the current scene into PostScript via the feedback buffer."""

        self.activate()

# vim:ts=8:sts=4:et:
