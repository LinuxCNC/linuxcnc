from OpenGL.GL import *
from OpenGL.GLU import *
from OpenGL.GLUT import *
from OpenGL.GL import shaders
import numpy as np

from glutils import VBO, VAO

import glm

# responsible for rendering everything scaled in the scene,
# like bounding box, axis, objects, etc.
class ObjectRenderer():
    def __init__(self):
        self.VERTEX_SOURCE = '''
        #version 330

        #extension GL_ARB_explicit_uniform_location : require
        #extension GL_ARB_explicit_attrib_location : require

        layout(location=0) uniform mat4 proj; // view / projection matrix
        layout(location=1) uniform mat4 model; // model matrix

        layout(location=0) in vec4 position;
        layout(location=1) in vec3 color;
        out vec3 v_color;

        void main() {
          gl_Position = proj * model * position;
          v_color = color;
        }'''

        self.FRAGMENT_SOURCE ='''
        #version 330

        #extension GL_ARB_explicit_uniform_location : require
        #extension GL_ARB_explicit_attrib_location : require

        in vec3 v_color;
        out vec3 out_color;

        void main() {
          out_color = v_color;
        }'''

        # used for emulating deprecated glLineStipple
        self.LINE_VERTEX_SOURCE = '''
        #version 330

        #extension GL_ARB_explicit_uniform_location : require
        #extension GL_ARB_explicit_attrib_location : require

        layout(location=0) uniform mat4 proj; // view / projection matrix
        layout(location=1) uniform mat4 model; // model matrix

        layout (location=0) in vec4 position;
        layout (location=1) in vec3 color;

        out vec3 v_color;
        flat out vec4 v_start; // does not get interpolated, because of flat
        out vec4 v_pos;

        void main() {
          v_pos = proj * model * position;
          v_start = v_pos;
          gl_Position = v_pos;
          v_color = color;
        }'''

        self.LINE_FRAGMENT_SOURCE ='''
        #version 330

        #extension GL_ARB_explicit_uniform_location : require
        #extension GL_ARB_explicit_attrib_location : require

        flat in vec4 v_start;
        in vec4 v_pos;
        in vec3 v_color;
        out vec3 out_color;

        layout(location=2) uniform vec2 u_resolution;
        layout(location=3) uniform uint u_pattern;
        layout(location=4) uniform float u_factor;

        void main() {
          out_color = v_color;

          vec2 dir = (v_pos.xy-v_start.xy) * u_resolution/2.0;
          float dist = length(dir);

          uint bit = uint(round(dist / u_factor)) & 15U;
          if ((u_pattern & (1U<<bit)) == 0U)
            discard;
        }'''


    def init(self):
        glClearColor(0, 0, 0, 1)

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        
        # normal, colored shader
        VERTEX_SHADER_PROG = shaders.compileShader(self.VERTEX_SOURCE, GL_VERTEX_SHADER)
        FRAGMENT_SHADER_PROG = shaders.compileShader(self.FRAGMENT_SOURCE, GL_FRAGMENT_SHADER)
        self.shader_prog = shaders.compileProgram(VERTEX_SHADER_PROG, FRAGMENT_SHADER_PROG)
                
        # uniform locations
        self.projmat = 0
        self.modelmat = 1
        
        # line shader
        VERTEX_SHADER_PROG = shaders.compileShader(self.LINE_VERTEX_SOURCE, GL_VERTEX_SHADER)
        FRAGMENT_SHADER_PROG = shaders.compileShader(self.LINE_FRAGMENT_SOURCE, GL_FRAGMENT_SHADER)
        self.line_shader_prog = shaders.compileProgram(VERTEX_SHADER_PROG, FRAGMENT_SHADER_PROG)
        
        # uniform locations
        self.u_resolution = 2
        self.u_pattern = 3
        self.u_factor = 4

        # initial position / rotation / scale
        self.pos = glm.vec3(0,0,0)
        self.rotation = glm.mat4()
        self.scale = glm.vec3(1,1,1)
        # generate model matrix
        self._update_matrix()

        self.vbo = VBO()
        self.vao = VAO()

        self.vbo.gen()

        self.axes = np.array([
            # x axis
            1.0,0.0,0.0,
            0.2,1.0,0.2,
            0.0,0.0,0.0,
            0.2,1.0,0.2,
            # y axis
            0.0,1.0,0.0,
            1.0,0.2,0.2,
            0.0,0.0,0.0,
            1.0,0.2,0.2,
            # z axis
            0.0,0.0,1.0,
            0.2,0.2,1.0,
            0.0,0.0,0.0,
            0.2,0.2,1.0,
        ], dtype=np.float32)

        self.min_limit = glm.vec3(-1000)
        self.max_limit = glm.vec3(1000)

        self.box = self._get_bound_box(glm.vec3(-1000),glm.vec3(1000))

        data = np.concatenate([self.axes,self.box])

        self.vbo.fill(data, len(data)*4)
        self.vao.gen([self.vbo])

    def _get_bound_box(self, machine_limit_min, machine_limit_max):
        return np.array([
            machine_limit_min[0], machine_limit_min[1], machine_limit_max[2],
            1.0,0.0,0.0,
            machine_limit_min[0], machine_limit_min[1], machine_limit_min[2],
            1.0,0.0,0.0,

            machine_limit_min[0], machine_limit_min[1], machine_limit_min[2],
            1.0,0.0,0.0,
            machine_limit_min[0], machine_limit_max[1], machine_limit_min[2],
            1.0,0.0,0.0,

            machine_limit_min[0], machine_limit_max[1], machine_limit_min[2],
            1.0,0.0,0.0,
            machine_limit_min[0], machine_limit_max[1], machine_limit_max[2],
            1.0,0.0,0.0,

            machine_limit_min[0], machine_limit_max[1], machine_limit_max[2],
            1.0,0.0,0.0,
            machine_limit_min[0], machine_limit_min[1], machine_limit_max[2],
            1.0,0.0,0.0,


            machine_limit_max[0], machine_limit_min[1], machine_limit_max[2],
            1.0,0.0,0.0,
            machine_limit_max[0], machine_limit_min[1], machine_limit_min[2],
            1.0,0.0,0.0,

            machine_limit_max[0], machine_limit_min[1], machine_limit_min[2],
            1.0,0.0,0.0,
            machine_limit_max[0], machine_limit_max[1], machine_limit_min[2],
            1.0,0.0,0.0,

            machine_limit_max[0], machine_limit_max[1], machine_limit_min[2],
            1.0,0.0,0.0,
            machine_limit_max[0], machine_limit_max[1], machine_limit_max[2],
            1.0,0.0,0.0,

            machine_limit_max[0], machine_limit_max[1], machine_limit_max[2],
            1.0,0.0,0.0,
            machine_limit_max[0], machine_limit_min[1], machine_limit_max[2],
            1.0,0.0,0.0,


            machine_limit_min[0], machine_limit_min[1], machine_limit_min[2],
            1.0,0.0,0.0,
            machine_limit_max[0], machine_limit_min[1], machine_limit_min[2],
            1.0,0.0,0.0,

            machine_limit_min[0], machine_limit_max[1], machine_limit_min[2],
            1.0,0.0,0.0,
            machine_limit_max[0], machine_limit_max[1], machine_limit_min[2],
            1.0,0.0,0.0,

            machine_limit_min[0], machine_limit_max[1], machine_limit_max[2],
            1.0,0.0,0.0,
            machine_limit_max[0], machine_limit_max[1], machine_limit_max[2],
            1.0,0.0,0.0,

            machine_limit_min[0], machine_limit_min[1], machine_limit_max[2],
            1.0,0.0,0.0,
            machine_limit_max[0], machine_limit_min[1], machine_limit_max[2],
            1.0,0.0,0.0,
        ], dtype=np.float32)

    def change_box(self, machine_limit_min, machine_limit_max):
        if self.min_limit != machine_limit_min or self.max_limit != machine_limit_max:
            self.box = self._get_bound_box(machine_limit_min, machine_limit_max)
            self.vbo.update(len(self.axes)*4, self.box, len(self.box)*4)
            self.min_limit = machine_limit_min
            self.max_limit = machine_limit_max

    def _update_matrix(self):
        newmat = glm.translate(glm.mat4(), self.pos)
        newmat *= self.rotation
        self.model = glm.scale(newmat, self.scale)

    # sets position / center of the scene to this location
    def move(self, pos):
        self.pos = pos
        self._update_matrix()

    def translate(self, delta):
        self.pos += delta
        self._update_matrix()

    def rotate(self, angle, axis):
        self.rotation = glm.rotate(self.rotation, angle, axis)
        self._update_matrix()

    # updates scale of the scene
    def scale(self, scale):
        self.scale = scale
        self._update_matrix()

    def render(self,projmat, w, h):
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glViewport(0,0,w,h)
        
        self.vao.bind()
        glUseProgram(self.shader_prog)

        glUniformMatrix4fv(self.projmat, 1, GL_FALSE, glm.value_ptr(projmat))
        glUniformMatrix4fv(self.modelmat, 1, GL_FALSE, glm.value_ptr(self.model))

        glDrawArrays(GL_LINES, 0,6)            
        
        #self.vao.bind()
        glUseProgram(self.line_shader_prog)

        glUniformMatrix4fv(self.projmat, 1, GL_FALSE, glm.value_ptr(projmat))
        glUniformMatrix4fv(self.modelmat, 1, GL_FALSE, glm.value_ptr(self.model))
        glUniform2f(self.u_resolution, w, h)
        glUniform1ui(self.u_pattern, 0x1111)
        glUniform1f(self.u_factor, 1)
    
        glDrawArrays(GL_LINES, 6,24)

        self.vao.unbind()        
        glUseProgram(0)
