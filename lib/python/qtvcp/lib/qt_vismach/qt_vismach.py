#!/usr/bin/env python3

import sys

from PyQt5.QtCore import pyqtSignal, QPoint, QSize, Qt, QTimer
from PyQt5.QtGui import QColor
from PyQt5.QtWidgets import (QApplication, QHBoxLayout, QOpenGLWidget, QSlider,
                             QWidget)

import OpenGL.GL as GL
from OpenGL import GLU
from .primitives import *


class Window(QWidget):

    def __init__(self):
        super(Window, self).__init__()

        self.glWidget = GLWidget()

        self.xSlider = self.createSlider()
        self.ySlider = self.createSlider()
        self.zSlider = self.createSlider()
        self.zoomSlider = self.createSlider()
        self.zoomSlider.setRange(1000, 5600)
        self.zoomSlider.setPageStep(10)
        self.zoomSlider.setTickInterval(1)

        self.xSlider.valueChanged.connect(self.glWidget.setXRotation)
        self.glWidget.xRotationChanged.connect(self.xSlider.setValue)
        self.ySlider.valueChanged.connect(self.glWidget.setYRotation)
        self.glWidget.yRotationChanged.connect(self.ySlider.setValue)
        self.zSlider.valueChanged.connect(self.glWidget.setZRotation)
        self.glWidget.zRotationChanged.connect(self.zSlider.setValue)
        self.zoomSlider.valueChanged.connect(self.glWidget.setZoom)
        self.glWidget.zoomChanged.connect(self.zoomSlider.setValue)

        mainLayout = QHBoxLayout()
        mainLayout.addWidget(self.glWidget)
        mainLayout.addWidget(self.xSlider)
        mainLayout.addWidget(self.ySlider)
        mainLayout.addWidget(self.zSlider)
        mainLayout.addWidget(self.zoomSlider)
        self.setLayout(mainLayout)

        self.xSlider.setValue(112 * 16)
        self.ySlider.setValue(180 * 16)
        self.zSlider.setValue(0 * 16)
        self.zoomSlider.setValue(10)

        self.setWindowTitle("Qt Vismach")

    def createSlider(self):
        slider = QSlider(Qt.Vertical)

        slider.setRange(0, 360 * 16)
        slider.setSingleStep(16)
        slider.setPageStep(15 * 16)
        slider.setTickInterval(15 * 16)
        slider.setTickPosition(QSlider.TicksRight)

        return slider


class GLWidget(QOpenGLWidget):
    xRotationChanged = pyqtSignal(int)
    yRotationChanged = pyqtSignal(int)
    zRotationChanged = pyqtSignal(int)
    zoomChanged = pyqtSignal(int)

    def __init__(self, parent=None):
        super(GLWidget, self).__init__(parent)

        self.model = None

        self.xRot = 1700
        self.yRot = 2850
        self.zRot = 0

        self.lastPos = QPoint()

        self.r_back = self.g_back = self.b_back = 0

        self.plotdata = []
        self.plotlen = 16000

        # Where we are centering.
        self.xcenter = 0.0
        self.ycenter = 0.0
        self.zcenter = 0.0

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
        self.minlat = -90
        self.maxlat = 90

        # does not show HUD by default
        self.hud = Hud()
        # self.hud.show("stuff")
        # add a 100ms timer to poll linuxcnc stats
        self.timer = QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(100)

    def getOpenglInfo(self):
        info = """
            Vendor: {0}
            Renderer: {1}
            OpenGL Version: {2}
            Shader Version: {3}
        """.format(
            GL.glGetString(GL.GL_VENDOR),
            GL.glGetString(GL.GL_RENDERER),
            GL.glGetString(GL.GL_VERSION),
            GL.glGetString(GL.GL_SHADING_LANGUAGE_VERSION)
        )

        return info

    def minimumSizeHint(self):
        return QSize(50, 50)

    def sizeHint(self):
        return QSize(400, 400)

    def winfo_width(self):
        return self.geometry().width()

    def winfo_height(self):
        return self.geometry().height()

    def setXRotation(self, angle):
        angle = self.normalizeAngle(angle)
        if angle != self.xRot:
            self.xRot = angle
            self.xRotationChanged.emit(angle)
            self.update()

    def setYRotation(self, angle):
        angle = self.normalizeAngle(angle)
        if angle != self.yRot:
            self.yRot = angle
            self.yRotationChanged.emit(angle)
            self.update()

    def setZRotation(self, angle):
        angle = self.normalizeAngle(angle)
        if angle != self.zRot:
            self.zRot = angle
            self.zRotationChanged.emit(angle)
            self.update()

    def setZoom(self, depth):
        self.distance = depth
        self.update()

    def zoomin(self):
        self.distance = self.distance / 1.1
        if self.distance < 600:
            self.distance = 600
        self.update()

    def zoomout(self):
        self.distance = self.distance * 1.1
        if self.distance > 5600:
            self.distance = 5600
        self.update()

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

    def initializeGL(self):
        # basic_lighting
        GL.glLightfv(GL.GL_LIGHT0, GL.GL_POSITION, (1, -1, .5, 0))
        GL.glLightfv(GL.GL_LIGHT0, GL.GL_AMBIENT, (.2, .2, .2, 0))
        GL.glLightfv(GL.GL_LIGHT0, GL.GL_DIFFUSE, (.6, .6, .4, 0))
        GL.glLightfv(GL.GL_LIGHT0 + 1, GL.GL_POSITION, (-1, -1, .5, 0))
        GL.glLightfv(GL.GL_LIGHT0 + 1, GL.GL_AMBIENT, (.0, .0, .0, 0))
        GL.glLightfv(GL.GL_LIGHT0 + 1, GL.GL_DIFFUSE, (.0, .0, .4, 0))
        GL.glMaterialfv(GL.GL_FRONT_AND_BACK, GL.GL_AMBIENT_AND_DIFFUSE, (1, 1, 1, 0))
        GL.glDisable(GL.GL_CULL_FACE)
        GL.glEnable(GL.GL_LIGHTING)
        GL.glEnable(GL.GL_LIGHT0)
        GL.glEnable(GL.GL_LIGHT0 + 1)
        GL.glDepthFunc(GL.GL_LESS)
        GL.glEnable(GL.GL_DEPTH_TEST)
        GL.glMatrixMode(GL.GL_MODELVIEW)
        GL.glLoadIdentity()

    def paintGL(self):
        GL.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT)
        GL.glLoadIdentity()
        GL.glTranslated(0.0, 0.0, -10.0)
        GL.glRotated(self.xRot / 16.0, 1.0, 0.0, 0.0)
        GL.glRotated(self.yRot / 16.0, 0.0, 1.0, 0.0)
        GL.glRotated(self.zRot / 16.0, 0.0, 0.0, 1.0)

        # draw Objects
        GL.glPushMatrix()  # Protect our matrix
        w = self.winfo_width()
        h = self.winfo_height()
        GL.glViewport(0, 0, w, h)

        # Clear the background and depth buffer.
        GL.glClearColor(self.r_back, self.g_back, self.b_back, 0.)
        GL.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT)

        GL.glMatrixMode(GL.GL_PROJECTION)
        GL.glLoadIdentity()
        GLU.gluPerspective(self.fovy, float(w) / float(h), self.near, self.far)

        if 0:
            # Now translate the scene origin away from the world origin
            GL.glMatrixMode(GL.GL_MODELVIEW)
            mat = GL.glGetDoublev(GL.GL_MODELVIEW_MATRIX)
            GL.glLoadIdentity()
            GL.glTranslatef(-self.xcenter, -self.ycenter, -(self.zcenter + self.distance))
            GL.glMultMatrixd(mat)
        else:
            GLU.gluLookAt(self.xcenter, self.ycenter, self.zcenter + self.distance,
                          self.xcenter, self.ycenter, self.zcenter,
                          0., 1., 0.)
            GL.glMatrixMode(GL.GL_MODELVIEW)

        # Call objects redraw method.
        self.drawObjects()
        GL.glFlush()  # Tidy up
        GL.glPopMatrix()  # Restore the matrix

    def drawObjects(self, *args):
        if self.winfo_width() == 1: return
        if self.model is None: return

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
        wx = tx * view2work[0][0] + ty * view2work[1][0] + tz * view2work[2][0] + view2work[3][0]
        wy = tx * view2work[0][1] + ty * view2work[1][1] + tz * view2work[2][1] + view2work[3][1]
        wz = tx * view2work[0][2] + ty * view2work[1][2] + tz * view2work[2][2] + view2work[3][2]
        # wx, wy, wz are the values to use for backplot
        # so we save them in a buffer
        if len(self.plotdata) == self.plotlen:
            del self.plotdata[:self.plotlen / 10]
        point = [wx, wy, wz]
        if not self.plotdata or point != self.plotdata[-1]:
            self.plotdata.append(point)

        # now lets draw something in the tool coordinate system
        # GL.glPushMatrix()
        # matrixes take effect in reverse order, so the next
        # two lines do "tool -> view -> world"
        # GL.glMultMatrixd(view2world)
        # GL.glMultMatrixd(self.tool2view.t)

        # do drawing here
        # cylinder normally goes to +Z, we want it down
        # GL.glTranslatef(0,0,-60)
        # GLU.gluCylinder(self.q1, 20, 20, 60, 32, 16)

        # back to world coords
        # GL.glPopMatrix()

        # we can also draw in the work coord system
        GL.glPushMatrix()
        # "work -> view -> world"
        GL.glMultMatrixd(view2world)
        GL.glMultMatrixd(self.work2view.t)
        # now we can draw in work coords, and whatever we draw
        # will move with the work, (if the work is attached to
        # a table or indexer or something that moves with
        # respect to the world

        # just a test object, sitting on the table
        # GLU.gluCylinder(self.q2, 40, 20, 60, 32, 16)

        # draw head up display
        if (hasattr(self.hud, "draw")):
            self.hud.draw()

        # draw backplot
        GL.glDisable(GL.GL_LIGHTING)
        GL.glLineWidth(2)
        GL.glColor3f(1.0, 0.5, 0.5)

        GL.glBegin(GL.GL_LINE_STRIP)
        for p in self.plotdata:
            GL.glVertex3f(*p)
        GL.glEnd()

        GL.glEnable(GL.GL_LIGHTING)
        GL.glColor3f(1, 1, 1)
        GL.glLineWidth(1)
        GL.glDisable(GL.GL_BLEND)
        GL.glDepthFunc(GL.GL_LESS)

        # back to world again
        GL.glPopMatrix()

    def plotclear(self):
        del self.plotdata[:self.plotlen]

    def resizeGL(self, width, height):
        side = min(width, height)
        if side < 0:
            return

        GL.glViewport((width - side) // 2, (height - side) // 2, side,
                      side)

        GL.glMatrixMode(GL.GL_PROJECTION)
        GL.glLoadIdentity()
        GL.glOrtho(-0.5, +0.5, +0.5, -0.5, 4.0, 15.0)
        GL.glMatrixMode(GL.GL_MODELVIEW)

    def mousePressEvent(self, event):
        self.lastPos = event.pos()

    def mouseMoveEvent(self, event):
        dx = event.x() - self.lastPos.x()
        dy = event.y() - self.lastPos.y()

        if event.buttons() & Qt.LeftButton:
            self.setXRotation(self.xRot + 8 * dy)
            self.setYRotation(self.yRot + 8 * dx)
        elif event.buttons() & Qt.RightButton:
            self.setXRotation(self.xRot + 8 * dy)
            self.setZRotation(self.zRot + 8 * dx)

        self.lastPos = event.pos()

    def mouseDoubleClickEvent(self, event):
        if event.button() & Qt.RightButton:
            self.plotclear()

    def wheelEvent(self, event):
        # Use the mouse wheel to zoom in/out
        a = event.angleDelta().y() / 200
        if a < 0:
            self.zoomout()
        else:
            self.zoomin()
        event.accept()

    def normalizeAngle(self, angle):
        while angle < 0:
            angle += 360 * 16
        while angle > 360 * 16:
            angle -= 360 * 16
        return angle


def main(model, tool, work, size=10, hud=0, rotation_vectors=None, lat=0, lon=0):
    app = QApplication(sys.argv)

    window = Window()
    t = window.glWidget
    t.set_latitudelimits(-180, 180)

    world = Capture()

    t.model = Collection([model, world])
    t.distance = size * 3
    t.near = size * 0.01
    t.far = size * 10.0
    t.tool2view = tool
    t.world2view = world

    t.work2view = work

    window.show()
    sys.exit(app.exec_())


if __name__ == '__main__':
    print('This is a library file - it needs to be imported')
