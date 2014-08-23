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
import glnav

# Keith Junius <junius@chem.rug.nl> provided many changes to Togl
TOGL_NORMAL = 1
TOGL_OVERLAY = 2

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
        self.swapbuffers()


    def tkMap(self, *dummy):
        self.tkExpose()


    def tkExpose(self, *dummy):
        self.tkRedraw()




class Opengl(RawOpengl, glnav.GlNavBase):
    """\
Tkinter bindings for an Opengl widget.
Mike Hartshorn
Department of Chemistry
University of York, UK
http://www.yorvic.york.ac.uk/~mjh/
"""

    rotation_vectors = [(1.,0.,0.), (0., 0., 1.)]
    def __init__(self, master=None, cnf={}, **kw):
        """\
        Create an opengl widget.
        Arrange for redraws when the window is exposed or when
        it changes size."""

        #Widget.__init__(self, master, 'togl', cnf, kw)
        apply(RawOpengl.__init__, (self, master, cnf), kw)
        glnav.GlNavBase.__init__(self)

        # Basic bindings for the virtual trackball
        self.bind('<Map>', self.tkMap)
        self.bind('<Expose>', self.tkExpose)
        self.bind('<Configure>', self.tkExpose)

        self.bind('<Shift-Button-1>', self.tkHandlePick)
        self.bind('<Button-2>', self.tkStartRotate)
        self.bind("<B2-Motion>", self.tkRotateOrTranslate)
        self.bind('<ButtonRelease-2>', self.tkAutoSpin)
        self.bind('<Button-3>', self.tkRecordMouse)
        self.bind('<B3-Motion>', self.tkScale)
        self.bind('<Button-4>', self.tkZoomin)
        self.bind('<Button-5>', self.tkZoomout)
        self.bind('<MouseWheel>', self.zoomwheel)
        self.bind('<Button-1>', self.tkStartRotate)
        self.bind('<Button1-Motion>', self.tkTranslateOrRotate)
        self.bind("<Shift-Button-1>", self.tkStartRotate)
        self.bind("<Shift-B1-Motion>", self.tkRotateOrTranslate)

        self.bind("<Control-Button-1>", self.tkStartZoom)
        self.bind("<Control-B1-Motion>", self.tkContinueZoom)
        self.bind("<Button-3>", self.tkStartZoom)
        self.bind("<B3-Motion>", self.tkContinueZoom)

        self.lat = 0
        self.minlat = -90
        self.maxlat = 0
        self.lon = 0

        # Is the widget allowed to autospin?
        self.autospin_allowed = 0

        # Is the widget currently autospinning?
        self.autospin = 0

        self.initialised = 0
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


    def deactivate(self):
        pass

    def activate(self):
        """Cause this Opengl widget to be the current destination for drawing."""

        self.tk.call(self._w, 'makecurrent')


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
        self.recordMouse(event.x, event.y)


    def tkStartRotate(self, event):
        # Switch off any autospinning if it was happening
        self.autospin = 0
        self.tkRecordMouse(event)

    def tkScale(self, event):
        self.scale(event.x, event.y)

    def do_AutoSpin(self):
        s = 0.5
        self.activate()

        glnav.glRotateScene(self, 0.5, self.xcenter, self.ycenter, self.zcenter, self.yspin, self.xspin, 0, 0)
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
        self.rotate(event.x, event.y)


    def tkTranslate(self, event):
        self.translate(event.x, event.y)

    def tkTranslateOrRotate(self, event):
        self.translateOrRotate(event.x, event.y)

    def tkRotateOrTranslate(self, event):
        self.rotateOrTranslate(event.x, event.y)



    def _redraw(self):
        self.tkRedraw()

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
        self.redraw()
        glFlush()               # Tidy up
        glPopMatrix()           # Restore the matrix

        self.swapbuffers()


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

    def zoomwheel(self, event):
        if event.delta > 0: self.zoomin(event)
        else: self.zoomout(event)

    def tkStartZoom(self, event):
        self.startZoom(event.y)

    def tkContinueZoom(self, event):
        self.continueZoom(event.y)

    def tkZoomin(self, event):
        self.zoomin()

    def tkZoomout(self, event):
        self.zoomout()

# vim:ts=8:sts=4:et:
