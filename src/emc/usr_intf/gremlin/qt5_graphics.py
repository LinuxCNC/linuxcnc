#!/usr/bin/env python

import sys
import math

import warnings

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

from PyQt5.QtCore import pyqtSignal, QPoint, QSize, Qt, QTimer
from PyQt5.QtGui import QColor
from PyQt5.QtWidgets import (QApplication, QHBoxLayout, QMessageBox, QSlider,
        QWidget)
try:
    from PyQt5.QtOpenGL import QGLWidget
except ImportError:
    log.critical("Qtvcp error with qt5_graphics - is package python-pyqt5.qtopengl installed?")

LIB_GOOD = True
try:
    from OpenGL import GL
    from OpenGL import GLU
except ImportError:
    log.error('Qtvcp Error with graphics - is python-openGL installed?')
    LIB_GOOD = False

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

###################################
# For stand alone window
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

#################
# Helper class
#################
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

###############################
# widget for graphics plotting
###############################
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
        # requires linuxcnc running before laoding this widget
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
        # start tracking linuxcnc position so we can plot it
        thread.start_new_thread(self.logger.start, (.01,))
        glcanon.GlCanonDraw.__init__(self, linuxcnc.stat(), self.logger)

        # set defaults
        self.current_view = 'p'
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
        self.gcode_properties = None
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
        self.enable_dro = False
        self.use_default_controls = True
        self.mouse_btn_mode = 0
        self.use_gradient_background = False

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
  
        # add a 100ms timer to poll linuxcnc stats
        self.timer = QTimer()
        self.timer.timeout.connect(self.poll)
        self.timer.start(100)

        self.Green = QColor.fromCmykF(0.40, 0.0, 1.0, 0.0)

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
        return True

    def load(self,filename = None):
        s = self.stat
        s.poll()
        if not filename and s.file:
            filename = s.file
        elif not filename and not s.file:
            return

        td = tempfile.mkdtemp()
        self._current_file = filename
        try:
            random = int(self.inifile.find("EMCIO", "RANDOM_TOOLCHANGER") or 0)
            canon = StatCanon(self.colors, self.get_geometry(),self.lathe_option, s, random)
            parameter = self.inifile.find("RS274NGC", "PARAMETER_FILE")
            temp_parameter = os.path.join(td, os.path.basename(parameter or "linuxcnc.var"))
            if parameter:
                shutil.copy(parameter, temp_parameter)
            canon.parameter_file = temp_parameter
            unitcode = "G%d" % (20 + (s.linear_units == 1))
            initcode = self.inifile.find("RS274NGC", "RS274NGC_STARTUP_CODE") or ""
            result, seq = self.load_preview(filename, canon, unitcode, initcode)
            if result > gcode.MIN_ERROR:
                self.report_gcode_error(result, seq, filename)
            self.calculate_gcode_properties(canon)
        except:
            self.gcode_properties = None
        finally:
            shutil.rmtree(td)


        self.set_current_view()

    def calculate_gcode_properties(self, canon):
        def dist((x,y,z),(p,q,r)):
            return ((x-p)**2 + (y-q)**2 + (z-r)**2) ** .5
        def from_internal_units(pos, unit=None):
            if unit is None:
                unit = self.stat.linear_units
            lu = (unit or 1) * 25.4

            lus = [lu, lu, lu, 1, 1, 1, lu, lu, lu]
            return [a*b for a, b in zip(pos, lus)]
        def from_internal_linear_unit(v, unit=None):
            if unit is None:
                unit = self.stat.linear_units
            lu = (unit or 1) * 25.4
            return v*lu

        props = {}
        loaded_file = self._current_file
        max_speed = float(
            self.inifile.find("DISPLAY","MAX_LINEAR_VELOCITY")
            or self.inifile.find("TRAJ","MAX_LINEAR_VELOCITY")
            or self.inifile.find("AXIS_X","MAX_VELOCITY")
            or 1)

        if not loaded_file:
            props['name'] = _("No file loaded")
        else:
            ext = os.path.splitext(loaded_file)[1]
            program_filter = None
            if ext:
                program_filter = self.inifile.find("FILTER", ext[1:])
            name = os.path.basename(loaded_file)
            if program_filter:
                props['name'] = _("generated from %s") % name
            else:
                props['name'] = name

            size = os.stat(loaded_file).st_size
            lines = sum(1 for line in open(loaded_file))
            props['size'] = _("%(size)s bytes\n%(lines)s gcode lines") % {'size': size, 'lines': lines}

            if self.metric_units:
                conv = 1
                units = _("mm")
                fmt = "%.3f"
            else:
                conv = 1/25.4
                units = _("in")
                fmt = "%.4f"

            mf = max_speed
            #print canon.traverse[0]

            g0 = sum(dist(l[1][:3], l[2][:3]) for l in canon.traverse)
            g1 = (sum(dist(l[1][:3], l[2][:3]) for l in canon.feed) +
                sum(dist(l[1][:3], l[2][:3]) for l in canon.arcfeed))
            gt = (sum(dist(l[1][:3], l[2][:3])/min(mf, l[3]) for l in canon.feed) +
                sum(dist(l[1][:3], l[2][:3])/min(mf, l[3])  for l in canon.arcfeed) +
                sum(dist(l[1][:3], l[2][:3])/mf  for l in canon.traverse) +
                canon.dwell_time
                )
 
            props['G0'] = "%f %s".replace("%f", fmt) % (from_internal_linear_unit(g0, conv), units)
            props['gG1'] = "%f %s".replace("%f", fmt) % (from_internal_linear_unit(g1, conv), units)
            if gt > 120:
                props['Run'] = _("%.1f Minutes") % (gt/60)
            else:
                props['Run'] = _("%d Ceconds") % (int(gt))

            min_extents = from_internal_units(canon.min_extents, conv)
            max_extents = from_internal_units(canon.max_extents, conv)
            for (i, c) in enumerate("XYZ"):
                a = min_extents[i]
                b = max_extents[i]
                if a != b:
                    props[c] = _("%(a)f to %(b)f = %(diff)f %(units)s").replace("%f", fmt) % {'a': a, 'b': b, 'diff': b-a, 'units': units}
        self.gcode_properties = props

    # setup details when window shows
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

    # gettter / setters
    def get_font_info(self):
        return self.font_charwidth, self.font_linespace, self.font_base
    def get_program_alpha(self): return self.program_alpha
    def get_joints_mode(self): return self.use_joints_mode
    def get_show_commanded(self): return self.use_commanded
    def get_show_extents(self): return self.show_extents_option
    def get_gcode_properties(self): return self.gcode_properties
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
    def get_geometry(self):
        temp = self.inifile.find("DISPLAY", "GEOMETRY")
        if temp:
            _geometry = re.split(" *(-?[XYZABCUVW])", temp.upper())
            self._geometry = "".join(reversed(_geometry))
        else:
            self._geometry = 'XYZ'
        return self._geometry
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
    def set_current_view(self):
        if self.current_view not in ['p', 'x', 'y', 'y2', 'z', 'z2']:
            return
        return getattr(self, 'set_view_%s' % self.current_view)()
    def clear_live_plotter(self):
        self.logger.clear()
        self.update()

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
    # gcannon assumes this function name
    def _redraw(self):
        self.updateGL()

    # This overrides glcannon.py method so we can not plot the DRO 
    def dro_format(self,s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset):
        if not self.enable_dro:
            return limit, homed, [''], ['']
        return parent_dro_format(self,s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset)

    # provide access to glcannon's default function
    def parent_dro_format(self,s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset):
        return glcanon.GlCanonDraw.dro_format(self,s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset)

    def minimumSizeHint(self):
        return QSize(50, 50)

    def sizeHint(self):
        return QSize(400, 400)

    def normalizeAngle(self, angle):
        while angle < 0:
            angle += 360 * 16
        while angle > 360 * 16:
            angle -= 360 * 16
        return angle

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
        self.updateGL()

    # called when widget is completely redrawn
    def initializeGL(self):
        self.object = self.makeObject()
        self.realize()
        GL.glEnable(GL.GL_CULL_FACE)
        return

    # redraws the screen aprox every 100ms
    def paintGL(self):
        #GL.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT)
        #GL.glLoadIdentity() # reset the model-view matrix
        #GL.glTranslated(0.0, 0.0, -10.0)
        #GL.glRotated(self.xRot / 16.0, 1.0, 0.0, 0.0) # rotate on x
        #GL.glRotated(self.yRot / 16.0, 0.0, 1.0, 0.0) # rotate on y
        #GL.glRotated(self.zRot / 16.0, 0.0, 0.0, 1.0) # rotate on z

        
        try:
            if self.perspective:
                self.redraw_perspective()
            else: self.redraw_ortho()

        except Exception as e:
            #print'error',e
            return
            #genList = GL.glGenLists(1)
            #self.draw_small_origin(genList)
            #GL.glCallList(genList)
            # display something - probably in QtDesigner
            GL.glCallList(self.object)

    #@with_context_swap
    def redraw_perspective(self):

        w = self.winfo_width()
        h = self.winfo_height()
        GL.glViewport(0, 0, w, h)
        if self.use_gradient_background:
                GL.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT)
                GL.glMatrixMode(GL.GL_PROJECTION)
                GL.glMatrixMode(GL.GL_PROJECTION)

                GL.glPushMatrix()
                GL.glLoadIdentity()
    
                GL.glMatrixMode(GL.GL_MODELVIEW)
                GL.glLoadIdentity()
                GL.glDisable(GL.GL_DEPTH_TEST)
                GL.glBegin(GL.GL_QUADS)
                #//blue color
                GL.glColor3f(0.0, 0.0, 1)
                GL.glVertex3f(-1.0, -1.0, -1.0)
                GL.glVertex3f(1.0, -1.0, -1.0)
                #//black color
                GL.glColor3f(0.0, 0.0, 0.0)
                GL.glVertex3f(1.0, 1.0, -1.0)
                GL.glVertex3f(-1.0, 1.0, -1.0)
               
                GL.glEnd()
                GL.glEnable(GL.GL_DEPTH_TEST)
                GL.glMatrixMode(GL.GL_PROJECTION)
                GL.glPopMatrix()
                GL.glMatrixMode(GL.GL_MODELVIEW)
                GL.glLoadIdentity()
        else:
            pass
            # Clear the background and depth buffer.
            GL.glClearColor(*(self.colors['back'] + (0,)))
            GL.glClear(GL.GL_COLOR_BUFFER_BIT | GL.GL_DEPTH_BUFFER_BIT)

        GL.glMatrixMode(GL.GL_PROJECTION)
        GL.glLoadIdentity()
        GLU.gluPerspective(self.fovy,               # The vertical Field of View, in radians: the amount of "zoom".
                                                    # Think "camera lens". Usually between 90 (extra wide) and 30 (quite zoomed in)
                        float(w)/float(h),          # Aspect Ratio. Notice that 4/3 == 800/600 screen resolution
                        self.near,                  # near clipping plane. Keep as big as possible, or you'll get precision issues.
                        self.far + self.distance)   # Far clipping plane. Keep as little as possible.

        GLU.gluLookAt(0, 0, self.distance,  # the position of your camera, in world space
            0, 0, 0,                        # where you want to look at, in world space
            0., 1., 0.)                     # probably glm::vec3(0,1,0), but (0,-1,0) would make you looking upside-down
        GL.glMatrixMode(GL.GL_MODELVIEW)
        GL.glPushMatrix()
        try:
            self.redraw()
        finally:
            GL.glFlush()                               # Tidy up
            GL.glPopMatrix()                   # Restore the matrix

    # resizes the view to fit the window
    def resizeGL(self, width, height):
        side = min(width, height)
        if side < 0:
            return
        GL.glViewport((width - side) // 2, (height - side) // 2, side, side)
        GL.glMatrixMode(GL.GL_PROJECTION) # To operate on projection-view matrix
        GL.glLoadIdentity() # reset the model-view matrix
        GL.glOrtho(-0.5, +0.5, +0.5, -0.5, 4.0, 15.0)
        GL.glMatrixMode(GL.GL_MODELVIEW) # To operate on model-view matrix

    ####################################
    # view controls
    ####################################
    def set_prime(self, x, y):
        if self.select_primed:
            primedx, primedy = self.select_primed
            distance = max(abs(x - primedx), abs(y - primedy))
            if distance > 8: self.select_cancel()

    def select_prime(self, x, y):
        self.select_primed = x, y

    # TODO This return statement breaks segment picking on the screen but
    # Also stop the display from pausing plotting update while searching
    # probably needs a thread - strange that Tkinter and GTK don't suffer...
    def select_fire(self):
        return
        if not self.select_primed: return
        x, y = self.select_primed
        self.select_primed = None
        self.select(x, y)

    def select_cancel(self, widget=None, event=None):
        self.select_primed = None

    def wheelEvent(self, _event):
        # Use the mouse wheel to zoom in/out
        a = _event.angleDelta().y()/200
        if a < 0:
            self.zoomout()
        else:
            self.zoomin()
        _event.accept()

    def mousePressEvent(self, event):
        if (event.buttons() & Qt.LeftButton):
            self.select_prime(event.pos().x(), event.pos().y())
            #print self.winfo_width()/2 - event.pos().x(), self.winfo_height()/2 - event.pos().y()
        self.recordMouse(event.pos().x(), event.pos().y())
        self.startZoom(event.pos().y())

    # event.buttons = current button state
    # event_button  = event causing button
    def mouseReleaseEvent(self, event):
        if event.button() & Qt.LeftButton:
            self.select_fire()

    def mouseDoubleClickEvent(self, event):
        if event.button() & Qt.RightButton:
            self.clear_live_plotter()

    def mouseMoveEvent(self, event):
        # move
        if event.buttons() & Qt.LeftButton:
            self.translateOrRotate(event.pos().x(), event.pos().y())
        # rotate
        elif event.buttons() & Qt.RightButton:
            self.set_prime(event.pos().x(), event.pos().y())
            self.rotateOrTranslate(event.pos().x(), event.pos().y())
        # zoom
        elif event.buttons() & Qt.MiddleButton:
            self.continueZoom(event.pos().y())

    def user_plot(self):
        pass
        #GL.glCallList(self.object)

    ############################################################
    # display for when linuxcnc isn't runnimg - forQTDesigner
    ############################################################
    def makeObject(self):
        genList = GL.glGenLists(1)
        GL.glNewList(genList, GL.GL_COMPILE)
  
        GL.glBegin(GL.GL_QUADS)

        # Make a tee section
        x1 = +0.06
        y1 = -0.14
        x2 = +0.14
        y2 = -0.06
        x3 = +0.08
        y3 = +0.00
        x4 = +0.30
        y4 = +0.22

        # cross
        self.quad(x1, y1, x2, y2, y2, x2, y1, x1)
        # vertical line
        self.quad(x3, y3, x4, y4, y4, x4, y3, x3)

        # cross depth
        self.extrude(x1, y1, x2, y2)
        self.extrude(x2, y2, y2, x2)
        self.extrude(y2, x2, y1, x1)
        self.extrude(y1, x1, x1, y1)

        # vertical depth
        self.extrude(x3, y3, x4, y4)
        self.extrude(x4, y4, y4, x4)
        self.extrude(y4, x4, y3, x3)
  
        NumSectors = 200
  
        # Make a circle
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
        self.qglColor(self.Green)
  
        GL.glVertex3d(x1, y1, -0.05)
        GL.glVertex3d(x2, y2, -0.05)
        GL.glVertex3d(x3, y3, -0.05)
        GL.glVertex3d(x4, y4, -0.05)
  
        GL.glVertex3d(x4, y4, +0.05)
        GL.glVertex3d(x3, y3, +0.05)
        GL.glVertex3d(x2, y2, +0.05)
        GL.glVertex3d(x1, y1, +0.05)
  
    def extrude(self, x1, y1, x2, y2):
        self.qglColor(self.Green.darker(250 + int(100 * x1)))
  
        GL.glVertex3d(x1, y1, +0.05)
        GL.glVertex3d(x2, y2, +0.05)
        GL.glVertex3d(x2, y2, -0.05)
        GL.glVertex3d(x1, y1, -0.05)

###########
# Testing
###########
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
  

