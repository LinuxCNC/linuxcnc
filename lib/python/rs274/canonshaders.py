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
from ctypes import c_uint

"""
Dev Notes:
- Chad Still does not understand why the stat mixin is needed? Seems it only populates a status object which we instatiate here?
- The QtShader class is a bit messy, it should be a separate file, but it needs to access CanonShaders methods.
- Should switch all matrixs to numpy arrays.
- Need to implement color buffers.
- Need to split this file up... but how, where, when...
"""

VERTEX_SHADER = """
#version 330
in vec4 color;
in vec3 position;
uniform mat4 MVP;
out vec4 f_color;
void main() {
    gl_Position = MVP * vec4(position, 1.0);
    f_color = color;
}

"""

FRAGMENT_SHADER = """
#version 330
varying vec4 f_color;
out vec4 fragColor;
void main() {
    fragColor = f_color;
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
            self.min_limits = [
                self.status.axis[0]["min_position_limit"],
                self.status.axis[1]["min_position_limit"],
                self.status.axis[2]["min_position_limit"],
            ]
            self.max_limits = [
                self.status.axis[0]["max_position_limit"],
                self.status.axis[1]["max_position_limit"],
                self.status.axis[2]["max_position_limit"],
            ]
        else:
            self.dro_in = "%.3f"
            self.dro_mm = "%.3f"
            self.max_limits = [200, 200, 200]
            self.min_limits = [-200, -200, -200]

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

        # Section 1: ============== GL PREAMBLE =========================
        glEnable(GL_DEPTH_TEST)

        # Section 2: ============== SHADER SETUP =========================
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

        # Section 3: ============== SHADER PROGRAM =========================
        self.shader_program = glCreateProgram()
        glAttachShader(self.shader_program, vertex_shader)
        glAttachShader(self.shader_program, fragment_shader)
        glLinkProgram(self.shader_program)

        if not glGetProgramiv(self.shader_program, GL_LINK_STATUS):
            print(glGetProgramInfoLog(self.shader_program))
            print(f"Shader program linking failed: {error}")
            return

        # Clean up shaders as they're linked into program now and no longer necessary
        glDetachShader(self.shader_program, vertex_shader)
        glDetachShader(self.shader_program, fragment_shader)

        glUseProgram(self.shader_program)

        # Section 4: ============== BUFFER SETUP =========================
        self.VAO_LIMITS = glGenVertexArrays(1)
        self.VAO_ORIGIN = glGenVertexArrays(1)
        self.VAO_BOUNDS = glGenVertexArrays(1)
        self.VAO_GCODE = glGenVertexArrays(1)

        # Translate to OpenGL-speak
        # TODO: Should we be using numpy arrays instead of ctypes?
        self.vertices = []

        # TODO Whats the best optimization, Multiple VBOs or a single VBO with offsets and indexes?
        self.VBO_FEED = glGenBuffers(1)
        self.VBO_ARC = glGenBuffers(1)
        self.VBO_ORIGIN = glGenBuffers(1)
        self.VBO_LIMITS = glGenBuffers(1)
        self.VBO_BOUND_BOX = glGenBuffers(1)

        self.VBO_FEED_COLORS = glGenBuffers(1)
        self.VBO_ARC_COLORS = glGenBuffers(1)
        self.VBO_ORIGIN_COLORS = glGenBuffers(1)
        self.VBO_LIMITS_COLORS = glGenBuffers(1)
        self.VBO_BOUND_BOX_COLORS = glGenBuffers(1)

    def origin_render(self):
        """
        Render the origin axis
        """

        # Draw the triangle
        glBindVertexArray(self.VAO_ORIGIN)

        # Origin lines
        origin_size = 1.0
        # fmt: off
        lines = [ 
            0.0, 0.0, 0.0, origin_size, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0, origin_size, 0.0,
            0.0, 0.0, 0.0, 0.0, 0.0, origin_size,
        ]
        # fmt: on

        # Prepare vertex data for origin axes
        glBindBuffer(GL_ARRAY_BUFFER, self.VBO_ORIGIN)
        self.vertices = (c_float * len(lines))(*lines)
        glBufferData(GL_ARRAY_BUFFER, len(self.vertices) * 4, self.vertices, GL_STATIC_DRAW)
        position_loc = glGetAttribLocation(self.shader_program, "position")
        glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE, 0, None)
        glEnableVertexAttribArray(position_loc)

        # Prepare color data for each axis (X: red, Y: green, Z: blue)
        # Each axis line has two vertices, so repeat color for both

        # fmt: off
        colors = [
            1.0, 0.0, 0.0, 1.0,  # X axis start (red)
            1.0, 0.0, 0.0, 1.0,  # X axis end   (red)
            0.0, 1.0, 0.0, 1.0,  # Y axis start (green)
            0.0, 1.0, 0.0, 1.0,  # Y axis end   (green)
            0.0, 0.0, 1.0, 1.0,  # Z axis start (blue)
            0.0, 0.0, 1.0, 1.0,  # Z axis end   (blue)
        ]
        # fmt: on
        glBindBuffer(GL_ARRAY_BUFFER, self.VBO_ORIGIN_COLORS)
        self.o_colors = (c_float * len(colors))(*colors)
        glBufferData(GL_ARRAY_BUFFER, len(self.o_colors) * 4, self.o_colors, GL_STATIC_DRAW)
        color_loc = glGetAttribLocation(self.shader_program, "color")
        glVertexAttribPointer(color_loc, 4, GL_FLOAT, GL_FALSE, 0, None)
        glEnableVertexAttribArray(color_loc)

        glDrawArrays(GL_LINES, 0, len(lines) // 3)

        # Optional: Unbind VAO and buffers for safety
        glBindBuffer(GL_ARRAY_BUFFER, 0)
        glBindVertexArray(0)

    def draw_cube_outline(self, min_extents, max_extents, vertex_buffer, color_buffer, color=(1.0, 1.0, 1.0, 1.0)):
        """
        Draw a cube using the provided minimum and maximum extents.
        """

        # TODO: You can draw a box with 6 points, can we do the same for lines?
        # TODO: Can we only draw this onces?
        # TODO: IBO here. 1,2,3,4,8,7,3,7,6,2,6,5,1
        # fmt: off
        # Define the 8 cube vertices
        lines = [
            min_extents[0], min_extents[1], min_extents[2], # 0
            max_extents[0], min_extents[1], min_extents[2], # 1
            max_extents[0], max_extents[1], min_extents[2], # 2
            min_extents[0], max_extents[1], min_extents[2], # 3
            min_extents[0], min_extents[1], max_extents[2], # 4
            max_extents[0], min_extents[1], max_extents[2], # 5
            max_extents[0], max_extents[1], max_extents[2], # 6
            min_extents[0], max_extents[1], max_extents[2], # 7
        ]
        # Specify the order to draw lines between vertices (0-based indices)

        # TODO: There is a line from 3-4 but why?
        cube_order = [0, 1, 2, 3, # Bottom Face
                      0, 4, 5, 1, 5, # Face 1
                      6, 2, 6, 7, 3, # Face 2
                      7, 4, 0  # Face 3
        ]
        # fmt: on

        # Upload vertex data
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer)
        self.vertices = (c_float * len(lines))(*lines)
        glBufferData(GL_ARRAY_BUFFER, len(self.vertices) * 4, self.vertices, GL_STATIC_DRAW)
        position_loc = glGetAttribLocation(self.shader_program, "position")
        glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE, 0, None)
        glEnableVertexAttribArray(position_loc)

        # Upload index data
        self.EBO_BOUND_BOX = glGenBuffers(1)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, self.EBO_BOUND_BOX)
        indices = (c_uint * len(cube_order))(*cube_order)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, len(indices) * 4, indices, GL_STATIC_DRAW)

        # Draw using element array buffer
        glDrawElements(GL_LINE_STRIP, len(cube_order), GL_UNSIGNED_INT, None)

        # Prepare color data for bounding box (white)
        colors = [color] * (12)
        colors = [item for sublist in colors for item in sublist]  # Flatten list
        glBindBuffer(GL_ARRAY_BUFFER, color_buffer)
        self.b_colors = (c_float * len(colors))(*colors)
        glBufferData(GL_ARRAY_BUFFER, len(self.b_colors) * 4, self.b_colors, GL_STATIC_DRAW)
        color_loc = glGetAttribLocation(self.shader_program, "color")
        glVertexAttribPointer(color_loc, 4, GL_FLOAT, GL_FALSE, 0, None)
        glEnableVertexAttribArray(color_loc)

        glDrawArrays(GL_LINE_LOOP, 0, len(indices) // 3)

    def draw_bounding_box(self):
        """
        Draw the bounding box around the G-code extents.
        """
        if self.canon is None:
            print("No canon set, cannot draw bounding box.")
            return

        # Draw the triangle
        glBindVertexArray(self.VAO_BOUNDS)

        (
            self.min_extents,
            self.max_extents,
            self.min_extents_notool,
            self.max_extents_notool,
        ) = gcode.calc_extents(self.canon.arcfeed, self.canon.feed, self.canon.traverse)

        # Set MVP matrix to fit min_extents and max_extents
        # Calculate the bounding box center and size
        center = [(self.min_extents[i] + self.max_extents[i]) / 2.0 for i in range(3)]
        size = [abs(self.max_extents[i] - self.min_extents[i]) for i in range(3)]
        max_size = max(size)

        self.draw_cube_outline(
            self.min_extents,
            self.max_extents,
            self.VBO_BOUND_BOX,
            self.VBO_BOUND_BOX_COLORS,
            color=(1.0, 1.0, 1.0, 1.0),
        )

    def draw_machine_limits(self):
        """
        Draw the machine limits as a cube.
        """
        if self.canon is None:
            print("No canon set, cannot draw machine limits.")
            return

        # Draw the triangle
        glBindVertexArray(self.VAO_LIMITS)

        self.draw_cube_outline(
            self.min_limits, self.max_limits, self.VBO_LIMITS, self.VBO_LIMITS_COLORS, color=(0.0, 0.0, 1.0, 1.0)
        )

    def gcode_render(self):
        """
        Render the OpenGL scene.
        """
        if self.canon is None:
            print("No canon set, cannot render.")
            return
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

        # ============= Active Vertex Array Object ==============
        glBindVertexArray(self.VAO_GCODE)

        # Draw Feed Lines
        glBindBuffer(GL_ARRAY_BUFFER, self.VBO_FEED)
        self.vertices = self.canon.draw_lines(self.canon.feed, for_selection=False)
        if self.vertices is None:
            print("No vertices to render.")
            return

        glBufferData(GL_ARRAY_BUFFER, len(self.vertices) * 4, self.vertices, GL_STATIC_DRAW)
        position_loc = glGetAttribLocation(self.shader_program, "position")
        glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE, 0, None)
        glEnableVertexAttribArray(position_loc)

        glBindBuffer(GL_ARRAY_BUFFER, self.VBO_FEED_COLORS)
        f_colors = []
        for line in self.canon.feed:
            f_colors.extend((1, 0.6, 0, 1.00))
            f_colors.extend((1, 0.6, 0, 1.00))
        f_colors = (c_float * len(f_colors))(*f_colors)

        glBufferData(GL_ARRAY_BUFFER, len(f_colors) * 4, f_colors, GL_STATIC_DRAW)
        color_loc = glGetAttribLocation(self.shader_program, "color")
        glVertexAttribPointer(color_loc, 4, GL_FLOAT, GL_FALSE, 0, None)
        glEnableVertexAttribArray(color_loc)

        glDrawArrays(GL_LINES, 0, len(self.vertices) // 3)

        # Draw Arcs
        glBindBuffer(GL_ARRAY_BUFFER, self.VBO_ARC)
        self.vertices = self.canon.draw_lines(self.canon.arcfeed, for_selection=False)
        if self.vertices is None:
            print("No arc vertices to render.")
            return
        glBufferData(GL_ARRAY_BUFFER, len(self.vertices) * 4, self.vertices, GL_STATIC_DRAW)
        position_loc = glGetAttribLocation(self.shader_program, "position")
        glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE, 0, None)
        glEnableVertexAttribArray(position_loc)

        glBindBuffer(GL_ARRAY_BUFFER, self.VBO_ARC_COLORS)
        f_colors = []
        for line in self.canon.arcfeed:
            f_colors.extend((1, 0.6, 0, 1.00))
            f_colors.extend((1, 0.6, 0, 1.00))
        f_colors = (c_float * len(f_colors))(*f_colors)

        glBufferData(GL_ARRAY_BUFFER, len(f_colors) * 4, f_colors, GL_STATIC_DRAW)
        color_loc = glGetAttribLocation(self.shader_program, "color")
        glVertexAttribPointer(color_loc, 4, GL_FLOAT, GL_FALSE, 0, None)
        glEnableVertexAttribArray(color_loc)

        glDrawArrays(GL_LINES, 0, len(self.vertices) // 3)

        # Optional: Unbind VAO and buffers for safety
        glBindBuffer(GL_ARRAY_BUFFER, 0)
        glBindVertexArray(0)

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
        CanonShaders.__init__(self, lp=None, canon=None)
        QtWidgets.QOpenGLWidget.__init__(self)
        self.status = linuxcnc.stat()
        self.colors = colors
        self.init_canon()

        self.filename = None

        # TODO: Can we use a singular matrix stack?
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
        self.current_scale = 1.0

    def init_canon(self):
        self.status.poll()
        inifile = linuxcnc.ini(status.ini_filename)
        parameter = inifile.find("RS274NGC", "PARAMETER_FILE")
        self.parrr = os.path.dirname(self.status.ini_filename) + "/" + parameter
        self.canon = CanonShade(
            colors=self.colors,
            geometry="XYZ",
            is_foam=False,
            lathe_view_option=False,
            stat=self.status,
            random=False,
        )
        self.canon.parameter_file = self.parrr

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
        mvp.scale(self.current_scale, self.current_scale, self.current_scale)
        mvp_data = mvp.data()

        # TODO: I would like to move all openGL calls to the CanonShader class
        MatrixID = glGetUniformLocation(self.shader_program, "MVP")
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, mvp_data)

        self.gcode_render()
        self.origin_render()
        self.draw_bounding_box()
        self.draw_machine_limits()

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

        # TODO: Validate current parameter file, proper offsets for G54-G59 and check G92
        #     Classic GlCanon seems to have some issues with G92 and G54-G59
        self.canon.parameter_file = self.parrr

        print(f"Loading {self.filename} with parameters from {self.canon.parameter_file}")

        # TODO: Unit Codes
        # TODO: This is a blocking function that blocks the UI.
        #       Would be nice to convert to a cyclic call with progress feedback.
        #       This is a problem for large files.
        #       When tried to debug gcodemodule.cc seems to block stderr and stdout.
        result, seq = gcode.parse(self.filename, self.canon, "G20")
        if result <= gcode.MIN_ERROR:
            pass
        else:
            print("Error parsing G-code file.")

    def mousePressEvent(self, e):

        if e.button() == QtCore.Qt.LeftButton:
            self.last_mouse_position = e.pos()
        elif e.button() == QtCore.Qt.RightButton:
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

    def mouseMoveEvent(self, e):
        if e.buttons() & QtCore.Qt.LeftButton:
            new_x = e.pos().x() - self.last_mouse_position.x()
            new_y = e.pos().y() - self.last_mouse_position.y()
            self.current_x += new_x * 0.005
            self.current_y -= new_y * 0.005
            self.last_mouse_position = e.pos()

        elif e.buttons() & QtCore.Qt.RightButton:
            fudge = 1.5
            new_x = (e.pos().x() - self.last_mouse_position.x()) * fudge
            new_y = (-1*(e.pos().y() - self.last_mouse_position.y())) * fudge

            dist = (new_x**2 + new_y**2) ** 0.5
            x_percent = new_x / dist if dist != 0 else 0
            y_percent = new_y / dist if dist != 0 else 0

            self.angularSpeed = dist * 0.1  # Adjust rotation speed based on

            # TODO: Fix center of rotation.
            self.rotationAxis = QtGui.QVector3D(-1 * y_percent, x_percent, 0)  # Update rotation axis
            self.rotation = QtGui.QQuaternion.fromAxisAndAngle(self.rotationAxis, self.angularSpeed) * self.rotation
            self.last_mouse_position = e.pos()

    def wheelEvent(self, e: QtGui.QWheelEvent):
        """
        Handle mouse scroll events to zoom in and out.
        """
        delta = e.angleDelta().y() / 120
        self.current_scale += delta * 0.1
        self.current_scale = max(0.1, min(self.current_scale, 10.0))
        self.update()

    def timerEvent(self, arg__0):
        update()


if __name__ == "__main__":
    colors = {
        "traverse": (0.30, 0.50, 0.50, 1.00),
        "traverse_alpha": 1 / 3.0,
        "traverse_xy": (0.30, 0.50, 0.50, 1.00),
        "traverse_alpha_xy": 1 / 3.0,
        "traverse_uv": (0.30, 0.50, 0.50, 1.00),
        "traverse_alpha_uv": 1 / 3.0,
        "backplotprobing_alpha": 0.75,
        "backplotprobing": (0.63, 0.13, 0.94, 1.00),
        "backplottraverse": (0.30, 0.50, 0.50, 1.00),
        "label_ok": (1.00, 0.51, 0.53, 1.00),
        "backplotjog_alpha": 0.75,
        "tool_diffuse": (0.60, 0.60, 0.60, 1.00),
        "backplotfeed": (0.75, 0.25, 0.25, 1.00),
        "back": (0.00, 0.00, 0.00, 1.00),
        "lathetool_alpha": 0.10,
        "axis_x": (0.20, 1.00, 0.20, 1.00),
        "cone": (1.00, 1.00, 1.00, 1.00),
        "cone_xy": (0.00, 1.00, 0.00, 1.00),
        "cone_uv": (0.00, 0.00, 1.00, 1.00),
        "axis_z": (0.20, 0.20, 1.00, 1.00),
        "label_limit": (1.00, 0.21, 0.23, 1.00),
        "backplotjog": (1.00, 1.00, 0.00, 1.00),
        "selected": (0.00, 1.00, 1.00, 1.00),
        "lathetool": (0.80, 0.80, 0.80, 1.00),
        "dwell": (1.00, 0.50, 0.50, 1.00),
        "overlay_foreground": (1.00, 1.00, 1.00, 1.00),
        "overlay_background": (0.00, 0.00, 0.00, 1.00),
        "straight_feed": (1.00, 1.00, 1.00, 1.00),
        "straight_feed_alpha": 1 / 3.0,
        "straight_feed_xy": (0.20, 1.00, 0.20, 1.00),
        "straight_feed_alpha_xy": 1 / 3.0,
        "straight_feed_uv": (0.20, 0.20, 1.00, 1.00),
        "straight_feed_alpha_uv": 1 / 3.0,
        "small_origin": (0.00, 1.00, 1.00, 1.00),
        "backplottoolchange_alpha": 0.25,
        "backplottraverse_alpha": 0.25,
        "overlay_alpha": 0.75,
        "tool_ambient": (0.40, 0.40, 0.40, 1.00),
        "tool_alpha": 0.20,
        "backplottoolchange": (1.00, 0.65, 0.00, 1.00),
        "backplotarc": (0.75, 0.25, 0.50, 1.00),
        "m1xx": (0.50, 0.50, 1.00, 1.00),
        "backplotfeed_alpha": 0.75,
        "backplotarc_alpha": 0.75,
        "arc_feed": (1.00, 1.00, 1.00, 1.00),
        "arc_feed_alpha": 0.5,
        "arc_feed_xy": (0.20, 1.00, 0.20, 1.00),
        "arc_feed_alpha_xy": 1 / 3.0,
        "arc_feed_uv": (0.20, 0.20, 1.00, 1.00),
        "arc_feed_alpha_uv": 1 / 3.0,
        "axis_y": (1.00, 0.20, 0.20, 1.00),
        "grid": (0.15, 0.15, 0.15, 1.00),
        "limits": (1.0, 0.0, 0.0, 1.0),
    }

    # FOR TESTING - Start linuxcnc with any UI. Then you can run this script to test this UI.
    # Start LCNC
    status = linuxcnc.stat()
    status.poll()
    # Start QT - GTK Guys y'all can go home.
    app = QtWidgets.QApplication([])
    Graphics = QtShader(colors, "XYZ", is_foam=False, lathe_view_option=False, stat=status, random=False)
    Graphics.show()
    Graphics.load_file(status.file)

    app.exec_()
