"""
This is an attempt at converting glcanon.py into a modern opengl processing stack.


Written by Chad A. Woitas for RMD Engineering. 2025

"""

import os
import time  # Debugging

import gcode
import linuxcnc
from rs274.glcanon import GLCanon
from rs274 import interpret

from OpenGL.GL import *
from OpenGL.GL.shaders import compileProgram, compileShader
from ctypes import c_float, c_void_p

from PyQt5 import QtCore, QtGui, QtWidgets

import traceback

"""
Dev Notes:
- Chad Still does not understand why the stat mixin is needed? Seems it only populates a status object which we instatiate here?
- Need to split this file up... but how, where, when...
"""

VERTEX_SHADER = """

// comment

#version 330
in vec3 vertices;
uniform mat4 MVP;
void main() {
    gl_Position = MVP * vec4(vertices, 1.0);
}

"""

FRAGMENT_SHADER = """

// comment

#version 330
out vec4 fragColor;
void main() {
    fragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red color
}

"""


class CanonShade(GLCanon, interpret.StatMixin):
    """
    Implementing GLcanon in a way to use shaders pipeline.
    Instantiating interpret stat mixin to be a self contained library without needing to reimplement it.
    """

    def __init__(
        self,
        colors,
        geometry,
        is_foam,
        lathe_view_option,
        stat,
        random,
        *args,
        **kwargs,
    ):
        GLCanon.__init__(self, colors, geometry, is_foam)
        
        interpret.StatMixin.__init__(self, stat, random)

        print("CanonShade initialized")
        self.shader_type = "CanonShader"

    def draw_lines(self, lines, for_selection, j=0, geometry=None):

        out_list = []
        if len(lines) < 0:
            print("No lines to draw, returning empty list.")
            return None
        for line in lines:
            out_list.append(line[1][0])
            out_list.append(line[1][1])
            out_list.append(line[1][2])
            out_list.append(line[2][0])
            out_list.append(line[2][1])
            out_list.append(line[2][2])

        return (c_float * len(out_list))(*out_list)

    def draw_dwells(self, dwells, alpha, for_selection, j0=0):
        # print(f"Drawing {len(dwells)} dwells with CanonShader")
        out_list = []
        if len(dwells) < 0:
            print("No dwells to draw, returning empty list.")
            return None
        for dwell in dwells:
            out_list.append(dwell[0][0])
            out_list.append(dwell[0][1])
            out_list.append(dwell[0][2])
            out_list.append(dwell[1][0])
            out_list.append(dwell[1][1])
            out_list.append(dwell[1][2])
        return (c_float * len(out_list))(*out_list)

    def highlight(self, lineno, geometry):
        # print("Highlighting lines with CanonShader")
        out_list = []
        if lineno < 0 or lineno >= len(self.feed):
            print(f"Invalid line number {lineno}, returning empty list.")
            return None
        line = self.feed[lineno]
        out_list.append(line[1][0])
        out_list.append(line[1][1])
        out_list.append(line[1][2])
        out_list.append(line[2][0])
        out_list.append(line[2][1])
        out_list.append(line[2][2])

        return (c_float * len(out_list))(*out_list)



class CanonShaders:

    def __init__(self, lp=None, canon=None, *args, **kwargs):

        # TODO:
        # self.lp = lp # Last Pose? Live Plot?

        # Load LinuxCNC information ----------------------------
        self.status = linuxcnc.stat()
        self.sim = False
        try:
            self.status.poll()
        except:
            self.sim = True

        self.inifile = None
        if not self.sim:
            self.load_ini_settings()

        # Load Drawing stuff ------------------------------------
        self.canon = canon

    def load_ini_settings(self):
        if not self.sim:
            self.status.poll()
            self.inifile = linuxcnc.ini(self.status.ini_filename)
        else:
            self.inifile = linuxcnc.ini("linuxcnc.ini")
        if self.inifile.find("DISPLAY", "DRO_FORMAT_IN"):
            temp = self.inifile.find("DISPLAY", "DRO_FORMAT_IN")
            try:
                test = temp % 1.234
            except:
                print("Error: invalid [DISPLAY] DRO_FORMAT_IN in INI file")
            else:
                self.dro_in = temp
        if self.inifile.find("DISPLAY", "DRO_FORMAT_MM"):
            temp = self.inifile.find("DISPLAY", "DRO_FORMAT_MM")
            try:
                test = temp % 1.234
            except:
                print("Error: invalid [DISPLAY] DRO_FORMAT_MM in INI file")
            else:
                self.dro_mm = temp
                self.dro_in = temp
        size = self.inifile.find("DISPLAY", "CONE_BASESIZE") or None
        # TODO: Draw tool tip
        # if size is not None:
        # self.set_cone_basesize(float(size))

    def init_gl(self):
        """
        Initialize OpenGL context and shaders.
        """
        print("Initializing OpenGL context and shaders...")
        glEnable(GL_DEPTH_TEST)
        vertex_shader = glCreateShader(GL_VERTEX_SHADER)
        glShaderSource(vertex_shader, VERTEX_SHADER)
        glCompileShader(vertex_shader)

        if not glGetShaderiv(vertex_shader, GL_COMPILE_STATUS):
            error = glGetShaderInfoLog(vertex_shader)
            print(f"Vertex shader compilation failed: {error}")
            return

        fragment_shader = glCreateShader(GL_FRAGMENT_SHADER)
        glShaderSource(fragment_shader, FRAGMENT_SHADER)
        glCompileShader(fragment_shader)

        if not glGetShaderiv(fragment_shader, GL_COMPILE_STATUS):
            error = glGetShaderInfoLog(fragment_shader)
            print(f"Fragment shader compilation failed: {error}")
            return

        # Create shader program
        self.shader_program = glCreateProgram()
        glAttachShader(self.shader_program, vertex_shader)
        glAttachShader(self.shader_program, fragment_shader)
        glLinkProgram(self.shader_program)

        if not glGetProgramiv(self.shader_program, GL_LINK_STATUS):
            error = glGetProgramInfoLog(self.shader_program)
            print(f"Shader program linking failed: {error}")
            return

        self.VAO = glGenVertexArrays(1)
        glBindVertexArray(self.VAO)

        # Triangle vertices
        vertices_list = [-0.5, -0.5, 0.0, 0.5, -0.5, 0.0, 0.0, 0.5, 0.0]
        print(f"Initial vertices: {vertices_list}")
        # Translate to OpenGL-speak
        self.vertices = (c_float * len(vertices_list))(*vertices_list)

        self.VBO_FEED = glGenBuffers(1)
        self.VBO_ARC = glGenBuffers(1)
        # glBindBuffer(GL_ARRAY_BUFFER, self.VBO_FEED)
        # glBufferData(GL_ARRAY_BUFFER, sizeof(self.vertices), self.vertices, GL_STATIC_DRAW)

        # glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, None)
        # glEnableVertexAttribArray(0)

        # Compile!
        self.shader = compileProgram(
            compileShader(VERTEX_SHADER, GL_VERTEX_SHADER),
            compileShader(FRAGMENT_SHADER, GL_FRAGMENT_SHADER),
        )

    def gcode_render(self):
        """
        Render the OpenGL scene.
        """
        if self.canon is None:
            print("No canon set, cannot render.")
            return
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glUseProgram(self.shader)

        # Draw the triangle
        glBindVertexArray(self.VAO)
        glBindBuffer(GL_ARRAY_BUFFER, self.VBO_FEED)
        self.vertices = self.canon.draw_lines(self.canon.feed, for_selection=False)
        if self.vertices is None:
            print("No vertices to render.")
            return
        # Upload vertex data to GPU
        glBufferData(GL_ARRAY_BUFFER, len(self.vertices) * 4, self.vertices, GL_STATIC_DRAW)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, None)
        glEnableVertexAttribArray(0)
        glDrawArrays(GL_LINES, 0, len(self.vertices))

        glBindBuffer(GL_ARRAY_BUFFER, self.VBO_ARC)
        self.vertices = self.canon.draw_lines(self.canon.arcfeed, for_selection=False)
        if self.vertices is None:
            print("No arc vertices to render.")
            return
        # Upload arc vertex data to GPU
        glBufferData(GL_ARRAY_BUFFER, len(self.vertices) * 4, self.vertices, GL_STATIC_DRAW)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, None)
        glEnableVertexAttribArray(0)
        glDrawArrays(GL_LINES, 0, len(self.vertices))
        
        # position = glGetAttribLocation(self.shader, 'vertices')
        # glBufferData(GL_ARRAY_BUFFER, sizeof(self.vertices), self.vertices, GL_STATIC_DRAW)
        # glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, 0, None)
        # glEnableVertexAttribArray(position)
        # glDrawArrays(GL_TRIANGLES, 0, len(self.vertices) // 3)
        # print(gcode.calc_extents(self.canon.arcfeed, self.canon.feed, self.canon.traverse))

    def set_canon(self, canon):
        self.canon = canon

    def load_preview(self, f, canon, *args):

        self.set_canon(canon)
        result, seq = gcode.parse(f, canon, *args)
        if result <= gcode.MIN_ERROR:
            canon.calc_extents()

        return result, seq


class QtShader(QtWidgets.QOpenGLWidget, CanonShaders):
    """
    A Qt widget that uses OpenGL to render using CanonShaders.
    """

    def __init__(self, colors, geometry, is_foam, lathe_view_option, stat, random, *args, **kwargs):
        QtWidgets.QOpenGLWidget.__init__(self)
        self.status = linuxcnc.stat()
        self.colors = colors
        CanonShaders.__init__(self, lp=None, canon=None)
        self.filename = None

        self.rotation = QtGui.QQuaternion()
        self.rotationAxis = QtGui.QVector3D(1, 1, 0)
        self.angularSpeed = 0
        self.lastTime = time.time()
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(16)  # Approximately 60 FPS
        self.setMinimumSize(800, 600)

        self.last_mouse_position = QtCore.QPoint()
        self.current_x = 0
        self.current_y = 0


    def init_canon(self):
        print("Initializing CanonShade...")
        self.status.poll()
        parrr = os.path.dirname(self.status.ini_filename) + "/" + parameter
        self.canon = CanonShade(colors=self.colors, geometry="XYZ", is_foam=False, lathe_view_option=False, stat=self.status, random=False)
        self.canon.parameter_file = parrr


    def initializeGL(self):
        self.init_gl()

    def paintGL(self):
        self.matrix = QtGui.QMatrix4x4()
        self.matrix.translate(self.current_x, self.current_y, -5.0)
        self.matrix.rotate(self.rotation)
        # Set modelview-projection matrix
        # Create a projection matrix (simple perspective)
        projection = QtGui.QMatrix4x4()
        projection.perspective(45.0, self.width() / self.height(), 0.1, 100.0)
        mvp = projection * self.matrix
        mvp_data = mvp.data()
        glUseProgram(self.shader)

        # TODO: I would like to move all openGL calls to the CanonShader class
        MatrixID = glGetUniformLocation(self.shader, "MVP")
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, mvp_data)

        self.gcode_render()

    def load_file(self, filename):
        """
        Load a G-code file and parse it.
        """
        if not os.path.exists(filename):
            print(f"File {filename} does not exist.")
            return

        self.status.poll()
        self.filename = self.status.file

        if self.canon is None:
            self.init_canon()

        parrr = os.path.dirname(self.status.ini_filename) + "/" + parameter
        self.canon.parameter_file = parrr

        print(f"Loading {self.filename} with parameters from {self.canon.parameter_file}")
        result, seq = gcode.parse(self.filename, self.canon, "G20")
        print(self.canon.colored_lines("straight_feed", self.canon.feed, for_selection=False))
        self.vertices = self.canon.draw_lines(self.canon.traverse, for_selection=False)
        print(self.vertices)
        if result <= gcode.MIN_ERROR:
            print(f"Extents: {self.canon.calc_extents()}")
        else:
            print("Error parsing G-code file.")

    def mousePressEvent(self, e):

        if(e.button() == QtCore.Qt.LeftButton):
            self.last_mouse_position = e.pos()
        elif(e.button() == QtCore.Qt.RightButton):
            self.last_mouse_position = e.pos()
            self.angularSpeed = 0.1


    def mouseReleaseEvent(self, e):
        if e.button() == QtCore.Qt.LeftButton:
            new_x = e.pos().x() - self.last_mouse_position.x()
            new_y = e.pos().y() - self.last_mouse_position.y()
            # TODO: Fix Scaling
            self.current_x += new_x * 0.005
            self.current_y -= new_y * 0.005
        else:
            return

        print(f"Mouse moved: {new_x}, {new_y}, total: {self.current_x}, {self.current_y}")

    def mouseMoveEvent(self, e):
        if e.buttons() & QtCore.Qt.LeftButton:
            new_x = e.pos().x() - self.last_mouse_position.x()
            new_y = e.pos().y() - self.last_mouse_position.y()
            self.current_x += new_x * 0.005
            self.current_y -= new_y * 0.005
            self.last_mouse_position = e.pos()

        elif e.buttons() & QtCore.Qt.RightButton:
            new_x = e.pos().x() - self.last_mouse_position.x()
            new_y = e.pos().y() - self.last_mouse_position.y()

            dist = (new_x**2 + new_y**2)**0.5
            x_percent = new_x / dist if dist != 0 else 0
            y_percent = new_y / dist if dist != 0 else 0

            self.angularSpeed = dist * 0.1  # Adjust rotation speed based on

            # TODO: Fix center of rotation.
            self.rotationAxis = QtGui.QVector3D(-1*y_percent, x_percent, 0)  # Update rotation axis
            self.rotation = QtGui.QQuaternion.fromAxisAndAngle(self.rotationAxis, self.angularSpeed) * self.rotation
            self.last_mouse_position = e.pos()



    def timerEvent(self, arg__0):
        update()




if __name__ == "__main__":
    colors = {
        "traverse": (0.30, 0.50, 0.50),
        "traverse_alpha": 1 / 3.0,
        "traverse_xy": (0.30, 0.50, 0.50),
        "traverse_alpha_xy": 1 / 3.0,
        "traverse_uv": (0.30, 0.50, 0.50),
        "traverse_alpha_uv": 1 / 3.0,
        "backplotprobing_alpha": 0.75,
        "backplotprobing": (0.63, 0.13, 0.94),
        "backplottraverse": (0.30, 0.50, 0.50),
        "label_ok": (1.00, 0.51, 0.53),
        "backplotjog_alpha": 0.75,
        "tool_diffuse": (0.60, 0.60, 0.60),
        "backplotfeed": (0.75, 0.25, 0.25),
        "back": (0.00, 0.00, 0.00),
        "lathetool_alpha": 0.10,
        "axis_x": (0.20, 1.00, 0.20),
        "cone": (1.00, 1.00, 1.00),
        "cone_xy": (0.00, 1.00, 0.00),
        "cone_uv": (0.00, 0.00, 1.00),
        "axis_z": (0.20, 0.20, 1.00),
        "label_limit": (1.00, 0.21, 0.23),
        "backplotjog": (1.00, 1.00, 0.00),
        "selected": (0.00, 1.00, 1.00),
        "lathetool": (0.80, 0.80, 0.80),
        "dwell": (1.00, 0.50, 0.50),
        "overlay_foreground": (1.00, 1.00, 1.00),
        "overlay_background": (0.00, 0.00, 0.00),
        "straight_feed": (1.00, 1.00, 1.00),
        "straight_feed_alpha": 1 / 3.0,
        "straight_feed_xy": (0.20, 1.00, 0.20),
        "straight_feed_alpha_xy": 1 / 3.0,
        "straight_feed_uv": (0.20, 0.20, 1.00),
        "straight_feed_alpha_uv": 1 / 3.0,
        "small_origin": (0.00, 1.00, 1.00),
        "backplottoolchange_alpha": 0.25,
        "backplottraverse_alpha": 0.25,
        "overlay_alpha": 0.75,
        "tool_ambient": (0.40, 0.40, 0.40),
        "tool_alpha": 0.20,
        "backplottoolchange": (1.00, 0.65, 0.00),
        "backplotarc": (0.75, 0.25, 0.50),
        "m1xx": (0.50, 0.50, 1.00),
        "backplotfeed_alpha": 0.75,
        "backplotarc_alpha": 0.75,
        "arc_feed": (1.00, 1.00, 1.00),
        "arc_feed_alpha": 0.5,
        "arc_feed_xy": (0.20, 1.00, 0.20),
        "arc_feed_alpha_xy": 1 / 3.0,
        "arc_feed_uv": (0.20, 0.20, 1.00),
        "arc_feed_alpha_uv": 1 / 3.0,
        "axis_y": (1.00, 0.20, 0.20),
        "grid": (0.15, 0.15, 0.15),
        "limits": (1.0, 0.0, 0.0),
    }

    # Start LCNC
    status = linuxcnc.stat()
    status.poll()
    print(f"Ini File: {status.ini_filename}")
    inifile = linuxcnc.ini(status.ini_filename)
    parameter = inifile.find("RS274NGC", "PARAMETER_FILE")

    # Start QT
    app = QtWidgets.QApplication([])
    Graphics = QtShader(colors, "XYZ", is_foam=False, lathe_view_option=False, stat=status, random=False)
    Graphics.show()
    Graphics.load_file(status.file)

    app.exec_()
