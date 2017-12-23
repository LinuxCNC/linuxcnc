#!/usr/bin/env python

import sys
import math

from PyQt5.QtCore import pyqtSignal, QPoint, QSize, Qt, QTimer
from PyQt5.QtGui import QColor
from PyQt5.QtWidgets import (QApplication, QHBoxLayout, QMessageBox, QSlider,
        QWidget)
from PyQt5.QtOpenGL import QGLWidget

LIB_GOOD = True
try:
    from OpenGL import GL
except ImportError:
    log.error('Qtvcp Error with graphics - is python-openGL installed?')
    LIB_GOOD = False


################################
import pango
import glnav
from rs274 import glcanon
from rs274 import interpret
import linuxcnc
import gcode

import time
import re
import tempfile
import shutil
import os

import thread

class DummyProgress:
    def nextphase(self, unused): pass
    def progress(self): pass

class StatCanon(glcanon.GLCanon, interpret.StatMixin):
    def __init__(self, colors, geometry, lathe_view_option, stat, random):
        glcanon.GLCanon.__init__(self, colors, geometry)
        interpret.StatMixin.__init__(self, stat, random)
        self.progress = DummyProgress()
        self.lathe_view_option = lathe_view_option

    def is_lathe(self): return self.lathe_view_option

    def change_tool(self, pocket):
        glcanon.GLCanon.change_tool(self,pocket)
        interpret.StatMixin.change_tool(self,pocket)

###################################
class Window(QWidget):
    def __init__(self, inifile):
        super(Window, self).__init__()
        self.glWidget = Lcnc_3dGraphics()
  
        self.xSlider = self.createSlider()
        self.ySlider = self.createSlider()
        self.zSlider = self.createSlider()
        self.zoomSlider = self.createZoomSlider()

        self.xSlider.valueChanged.connect(self.glWidget.setXRotation)
        self.glWidget.xRotationChanged.connect(self.xSlider.setValue)
        self.ySlider.valueChanged.connect(self.glWidget.setYRotation)
        self.glWidget.yRotationChanged.connect(self.ySlider.setValue)
        self.zSlider.valueChanged.connect(self.glWidget.setZRotation)
        self.glWidget.zRotationChanged.connect(self.zSlider.setValue)
        self.zoomSlider.valueChanged.connect(self.glWidget.setZoom)

        mainLayout = QHBoxLayout()
        mainLayout.addWidget(self.glWidget)
        mainLayout.addWidget(self.xSlider)
        mainLayout.addWidget(self.ySlider)
        mainLayout.addWidget(self.zSlider)
        mainLayout.addWidget(self.zoomSlider)
        self.setLayout(mainLayout)
  
        self.xSlider.setValue(15 * 16)
        self.ySlider.setValue(345 * 16)
        self.zSlider.setValue(0 * 16)
        self.zSlider.setValue(10)

        self.setWindowTitle("Hello GL")
  
    def createSlider(self):
        slider = QSlider(Qt.Vertical)
  
        slider.setRange(0, 360 * 16)
        slider.setSingleStep(16)
        slider.setPageStep(15 * 16)
        slider.setTickInterval(15 * 16)
        slider.setTickPosition(QSlider.TicksRight)
  
        return slider

    def createZoomSlider(self):
        slider = QSlider(Qt.Vertical)
  
        slider.setRange(1, 1000000)
        slider.setSingleStep(1)
        slider.setPageStep(10)
        slider.setTickInterval(10)
        slider.setTickPosition(QSlider.TicksRight)
  
        return slider


class Lcnc_3dGraphics(QGLWidget,  glcanon.GlCanonDraw, glnav.GlNavBase):
    xRotationChanged = pyqtSignal(int)
    yRotationChanged = pyqtSignal(int)
    zRotationChanged = pyqtSignal(int)
    rotation_vectors = [(1.,0.,0.), (0., 0., 1.)]

    def __init__(self, parent=None):
        super(Lcnc_3dGraphics, self).__init__(parent)
        glnav.GlNavBase.__init__(self)

        def C(s):
            a = self.colors[s + "_alpha"]
            s = self.colors[s]
            return [int(x * 255) for x in s + (a,)]
        inifile = os.environ.get('INI_FILE_NAME', '/dev/null')
        self.inifile = linuxcnc.ini(inifile)
        self.logger = linuxcnc.positionlogger(linuxcnc.stat(),
            C('backplotjog'),
            C('backplottraverse'),
            C('backplotfeed'),
            C('backplotarc'),
            C('backplottoolchange'),
            C('backplotprobing'),
            self.get_geometry()
        )
        thread.start_new_thread(self.logger.start, (.01,))
        glcanon.GlCanonDraw.__init__(self, linuxcnc.stat(), self.logger)
        self.current_view = 'z'
        self.fingerprint = ()
        self.select_primed = None
        self.lat = 0
        self.minlat = -90
        self.maxlat = 90

        self._current_file = None
        self.highlight_line = None
        self.program_alpha = False
        self.use_joints_mode = False
        self.use_commanded = True
        self.show_limits = True
        self.show_extents_option = True
        self.show_live_plot = True
        self.show_velocity = True
        self.metric_units = True
        self.show_program = True
        self.show_rapids = True
        self.use_relative = True
        self.show_tool = True
        self.show_dtg = True
        self.grid_size = 0.0
        temp = self.inifile.find("DISPLAY", "LATHE")
        self.lathe_option = bool(temp == "1" or temp == "True" or temp == "true" )
        self.foam_option = bool(self.inifile.find("DISPLAY", "FOAM"))
        self.show_offsets = False
        self.show_overlay = False
        self.use_default_controls = True
        self.mouse_btn_mode = 0

        self.a_axis_wrapped = self.inifile.find("AXIS_A", "WRAPPED_ROTARY")
        self.b_axis_wrapped = self.inifile.find("AXIS_B", "WRAPPED_ROTARY")
        self.c_axis_wrapped = self.inifile.find("AXIS_C", "WRAPPED_ROTARY")

        live_axis_count = 0
        for i,j in enumerate("XYZABCUVW"):
            if self.stat.axis_mask & (1<<i) == 0: continue
            live_axis_count += 1
        self.num_joints = int(self.inifile.find("KINS", "JOINTS") or live_axis_count)

        self.object = 0
        self.xRot = 0
        self.yRot = 0
        self.zRot = 0
  
        self.lastPos = QPoint()
  
        self.trolltechGreen = QColor.fromCmykF(0.40, 0.0, 1.0, 0.0)
        self.trolltechPurple = QColor.fromCmykF(0.39, 0.39, 0.0, 0.0)
        #self.realize()
        #self.load()
        self.timer = QTimer()
        self.timer.timeout.connect(self.poll)
        self.timer.start(100)
####
    def get_geometry(self):
        temp = self.inifile.find("DISPLAY", "GEOMETRY")
        if temp:
            _geometry = re.split(" *(-?[XYZABCUVW])", temp.upper())
            self._geometry = "".join(reversed(_geometry))
        else:
            self._geometry = 'XYZ'
        return self._geometry

    def set_current_view(self):
        if self.current_view not in ['p', 'x', 'y', 'y2', 'z', 'z2']:
            return
        return getattr(self, 'set_view_%s' % self.current_view)()

    def poll(self):
        s = self.stat
        try:
            s.poll()
        except:
            return
        fingerprint = (self.logger.npts, self.soft_limits(),
            s.actual_position, s.joint_actual_position,
            s.homed, s.g5x_offset, s.g92_offset, s.limit, s.tool_in_spindle,
            s.motion_mode, s.current_vel)

        if fingerprint != self.fingerprint:
            self.fingerprint = fingerprint
            self.update()

        # return self.visible
        return True

    def load(self,filename = None):
        s = self.stat
        s.poll()
        print 'filename',filename,s.file
        if not filename and s.file:
            filename = s.file
        elif not filename and not s.file:
            return

        td = tempfile.mkdtemp()
        self._current_file = filename
        print 'try loading'
        try:
            random = int(self.inifile.find("EMCIO", "RANDOM_TOOLCHANGER") or 0)
            canon = StatCanon(self.colors, self.get_geometry(),self.lathe_option, s, random)
            parameter = self.inifile.find("RS274NGC", "PARAMETER_FILE")
            temp_parameter = os.path.join(td, os.path.basename(parameter or "linuxcnc.var"))
            if parameter:
                shutil.copy(parameter, temp_parameter)
            canon.parameter_file = temp_parameter

            print 'get result'
            unitcode = "G%d" % (20 + (s.linear_units == 1))
            initcode = self.inifile.find("RS274NGC", "RS274NGC_STARTUP_CODE") or ""
            result, seq = self.load_preview(filename, canon, unitcode, initcode)
            if result > gcode.MIN_ERROR:
                self.report_gcode_error(result, seq, filename)

        finally:
            shutil.rmtree(td)
        print 'set_view'
        self.set_current_view()


    def realize(self):
        self.set_current_view()
        s = self.stat
        try:
            s.poll()
        except:
            return
        self._current_file = None

        self.font_base, width, linespace = \
		glnav.use_pango_font('courier bold 16', 0, 128)
        self.font_linespace = linespace
        self.font_charwidth = width
        glcanon.GlCanonDraw.realize(self)

    def get_font_info(self):
        return self.font_charwidth, self.font_linespace, self.font_base
    def get_program_alpha(self): return self.program_alpha
    def get_joints_mode(self): return self.use_joints_mode
    def get_show_commanded(self): return self.use_commanded
    def get_show_extents(self): return self.show_extents_option
    def get_show_limits(self): return self.show_limits
    def get_show_live_plot(self): return self.show_live_plot
    def get_show_machine_speed(self): return self.show_velocity
    def get_show_metric(self): return self.metric_units
    def get_show_program(self): return self.show_program
    def get_show_rapids(self): return self.show_rapids
    def get_show_relative(self): return self.use_relative
    def get_show_tool(self): return self.show_tool
    def get_show_distance_to_go(self): return self.show_dtg
    def get_grid_size(self): return self.grid_size
    def get_show_offsets(self): return self.show_offsets
    def get_view(self):
        view_dict = {'x':0, 'y':1, 'y2':1, 'z':2, 'z2':2, 'p':3}
        return view_dict.get(self.current_view, 3)

    def is_lathe(self): return self.lathe_option
    def is_foam(self): return self.foam_option
    def get_current_tool(self):
        for i in self.stat.tool_table:
            if i[0] == self.stat.tool_in_spindle:
                return i
    def get_highlight_line(self): return self.highlight_line

    def get_a_axis_wrapped(self): return self.a_axis_wrapped
    def get_b_axis_wrapped(self): return self.b_axis_wrapped
    def get_c_axis_wrapped(self): return self.c_axis_wrapped

    def winfo_width(self):
        return self.geometry().width()

    def winfo_height(self):
        return self.geometry().height()

    # trick - we are not gtk
    def activate(self):
        return
    def deactivate(self):
        return
    def swapbuffers(self):
        return
    # redirect for conversion from pygtk to pyqt
    def _redraw(self):
        self.updateGL()
    def select_prime(self, x, y):
        self.select_primed = x, y

    # This overrides glcannon.py method so we can not plot the DRO 
    def dro_format(self,s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset):
        if not self.show_overlay:
            return limit, homed, [''], ['']
        return glcanon.GlCanonDraw.dro_format(self,s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset)


####


    def minimumSizeHint(self):
        return QSize(50, 50)
  
    def sizeHint(self):
        return QSize(400, 400)
  
    def setXRotation(self, angle):
        angle = self.normalizeAngle(angle)
        if angle != self.xRot:
            self.xRot = angle
            self.xRotationChanged.emit(angle)
            self.updateGL()
  
    def setYRotation(self, angle):
        angle = self.normalizeAngle(angle)
        if angle != self.yRot:
            self.yRot = angle
            self.yRotationChanged.emit(angle)
            self.updateGL()
  
    def setZRotation(self, angle):
        angle = self.normalizeAngle(angle)
        if angle != self.zRot:
            self.zRot = angle
            self.zRotationChanged.emit(angle)
            self.updateGL()
  
    def setZoom(self, zoom):
        self.distance = zoom/100.0
        #print self.distance
        self.updateGL()

    def initializeGL(self):
        self.realize()
        GL.glEnable(GL.GL_CULL_FACE)
        return
        self.qglClearColor(self.trolltechPurple.darker())
        self.object = self.makeObject()
        GL.glShadeModel(GL.GL_FLAT)
        GL.glEnable(GL.GL_DEPTH_TEST)
        GL.glEnable(GL.GL_CULL_FACE)

    def paintGL(self):
        GL.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT)
        GL.glLoadIdentity()
        GL.glTranslated(0.0, 0.0, -10.0)
        GL.glRotated(self.xRot / 16.0, 1.0, 0.0, 0.0)
        GL.glRotated(self.yRot / 16.0, 0.0, 1.0, 0.0)
        GL.glRotated(self.zRot / 16.0, 0.0, 0.0, 1.0)
        #if self._current_file == None:
        #    GL.glCallList(self.object)
        #    return
        if self.perspective: self.redraw_perspective()
        else: self.redraw_ortho()
        #self.redraw()
        #GL.glCallList(self.object)
  
    def resizeGL(self, width, height):
        side = min(width, height)
        if side < 0:
            return
  
        GL.glViewport((width - side) // 2, (height - side) // 2, side, side)
  
        GL.glMatrixMode(GL.GL_PROJECTION)
        GL.glLoadIdentity()
        GL.glOrtho(-0.5, +0.5, +0.5, -0.5, 4.0, 15.0)
        GL.glMatrixMode(GL.GL_MODELVIEW)

    def wheelEvent(self, _event):
        # Use the mouse wheel to zoom in/out
        a = _event.angleDelta().y()/200
        if a < 0:
            self.zoomin()
        else:
            self.zoomout()

        d = - float(_event.angleDelta().y()) / 200.0 #* self.radius_
        print a,d,self.distance 
        #self.translate([0.0, 0.0, d])
        self.updateGL()
        _event.accept()

    def mousePressEvent(self, event):
        self.lastPos = event.pos()
        if (event.buttons() & Qt.LeftButton):
            print event.pos(), event.pos().x()
            self.select_prime(event.pos().x(), event.pos().y()) # select G-Code element

    def mouseMoveEvent(self, event):
        dx = event.x() - self.lastPos.x()
        dy = event.y() - self.lastPos.y()
  
        if event.buttons() & Qt.LeftButton:
            self.setXRotation(self.xRot + 8 * dy)
            self.setYRotation(self.yRot + 8 * dx)
        elif event.buttons() & Qt.RightButton:
            self.setXRotation(self.xRot + 8 * dy)
            self.setZRotation(self.zRot + 8 * dx)
        elif event.buttons() & Qt.MiddleButton:
            print 'mid'
            self.translateOrRotate(event.x(), event.y())
        self.lastPos = event.pos()
  
    def makeObject(self):
        genList = GL.glGenLists(1)
        GL.glNewList(genList, GL.GL_COMPILE)
  
        GL.glBegin(GL.GL_QUADS)
  
        x1 = +0.06
        y1 = -0.14
        x2 = +0.14
        y2 = -0.06
        x3 = +0.08
        y3 = +0.00
        x4 = +0.30
        y4 = +0.22
  
        self.quad(x1, y1, x2, y2, y2, x2, y1, x1)
        self.quad(x3, y3, x4, y4, y4, x4, y3, x3)
  
        self.extrude(x1, y1, x2, y2)
        self.extrude(x2, y2, y2, x2)
        self.extrude(y2, x2, y1, x1)
        self.extrude(y1, x1, x1, y1)
        self.extrude(x3, y3, x4, y4)
        self.extrude(x4, y4, y4, x4)
        self.extrude(y4, x4, y3, x3)
  
        NumSectors = 200
  
        for i in range(NumSectors):
            angle1 = (i * 2 * math.pi) / NumSectors
            x5 = 0.30 * math.sin(angle1)
            y5 = 0.30 * math.cos(angle1)
            x6 = 0.20 * math.sin(angle1)
            y6 = 0.20 * math.cos(angle1)
  
            angle2 = ((i + 1) * 2 * math.pi) / NumSectors
            x7 = 0.20 * math.sin(angle2)
            y7 = 0.20 * math.cos(angle2)
            x8 = 0.30 * math.sin(angle2)
            y8 = 0.30 * math.cos(angle2)
  
            self.quad(x5, y5, x6, y6, x7, y7, x8, y8)
  
            self.extrude(x6, y6, x7, y7)
            self.extrude(x8, y8, x5, y5)
  
        GL.glEnd()
        GL.glEndList()
  
        return genList
  
    def quad(self, x1, y1, x2, y2, x3, y3, x4, y4):
        self.qglColor(self.trolltechGreen)
  
        GL.glVertex3d(x1, y1, -0.05)
        GL.glVertex3d(x2, y2, -0.05)
        GL.glVertex3d(x3, y3, -0.05)
        GL.glVertex3d(x4, y4, -0.05)
  
        GL.glVertex3d(x4, y4, +0.05)
        GL.glVertex3d(x3, y3, +0.05)
        GL.glVertex3d(x2, y2, +0.05)
        GL.glVertex3d(x1, y1, +0.05)
  
    def extrude(self, x1, y1, x2, y2):
        self.qglColor(self.trolltechGreen.darker(250 + int(100 * x1)))
  
        GL.glVertex3d(x1, y1, +0.05)
        GL.glVertex3d(x2, y2, +0.05)
        GL.glVertex3d(x2, y2, -0.05)
        GL.glVertex3d(x1, y1, -0.05)
  
    def normalizeAngle(self, angle):
        while angle < 0:
            angle += 360 * 16
        while angle > 360 * 16:
            angle -= 360 * 16
        return angle
  
  
if __name__ == '__main__':
  
    app = QApplication(sys.argv)
    if len(sys.argv) == 1:
        inifilename = None
    elif len(sys.argv) == 2:
        inifilename = sys.argv[1]
    else:
        usage()
    window = Window(inifilename)
    window.show()
    sys.exit(app.exec_())
  

