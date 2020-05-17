from OpenGL.GL import *
from OpenGL.GLU import *
from OpenGL.GLUT import *
from OpenGL.GL import shaders

import array

from glutils import GLObject, VBO, VAO

import glm

# responsible for rendering everything scaled in the scene,
# like bounding box, axis, objects, etc.
class ObjectRenderer(GLObject):
    def __init__(self):
        # initial position / rotation / scale
        self.pos = glm.vec3(0)
        self.rot_x = 0
        self.rot_y = 0
        self.rot_z = 0
        self.rotate_pos = glm.vec3(0)
        self.scale = glm.vec3(1)
        # generate model matrix
        self._update_matrix()

        self.min_extent = glm.vec3(0)
        self.max_extent = glm.vec3(2)

        self.vbo = VBO()
        self.vao = VAO()

        self.feed_data = []
        self.feed_vbo = VBO()
        self.feed_vao = VAO()

        self.rapids_data = []
        self.rapids_vbo = VBO()
        self.rapids_vao = VAO()
        
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
        super().__init__()


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

        self.vbo.init()

        self.axes = array.array('f', [
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
        ])

        self.min_limit = glm.vec3(-1000)
        self.max_limit = glm.vec3(1000)

        self.box = self._get_bound_box(glm.vec3(-1000),glm.vec3(1000))
        self.extent_data = self._get_extents([glm.vec3(0), glm.vec3(2)])
        
        data = array.array('f')
        data.extend(self.axes)
        data.extend(self.box)
        data.extend(self.extent_data)
        self.axes_size = int(self.axes.buffer_info()[1]/6)
        self.box_size = int(self.box.buffer_info()[1]/6)
        self.extent_size = int(self.extent_data.buffer_info()[1]/6)
        
        self.vbo.arrayfill(data)
        self.vao.init([self.vbo])

        self.feed_vbo.init()
        self.feed_vao.init([self.vbo])
        
        self.rapids_vbo.init()
        self.rapids_vao.init([self.vbo])

        super().init()

        if len(self.feed_data):
            self.set_feed(self.feed_data)
            
        if len(self.rapids_data):
            self.set_rapids(self.rapids_data)
        

    def _get_bound_box(self, machine_limit_min, machine_limit_max):
        return array.array('f', [
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
            1.0,0.0,0.0,]
        )

    def _get_extents(self, extents):
        x,y,z,p = 0,1,2,3

        min_extents = extents[0]
        max_extents = extents[1]
        
        pullback = max(max_extents[x] - min_extents[x],
                       max_extents[y] - min_extents[y],
                       max_extents[z] - min_extents[z],
                       2 ) * .1
            
        dashwidth = pullback/4
        zdashwidth = dashwidth
        charsize = dashwidth * 1.5
        halfchar = charsize * .5
        
        """
            if view == z or view == p:
                z_pos = min_extents[z]
                zdashwidth = 0
            else:
                z_pos = min_extents[z] - pullback
                zdashwidth = dashwidth
            """
        x_pos = min_extents[x] - pullback
        y_pos = min_extents[y] - pullback
        z_pos = min_extents[z] - pullback
        
        return array.array('f', [
            #if view != x and max_extents[x] > min_extents[x]:
            
            min_extents[x], y_pos, z_pos,
            1.0, 0.51, 0.53,
            max_extents[x], y_pos, z_pos,
            1.0, 0.51, 0.53,
            
            min_extents[x], y_pos - dashwidth, z_pos - zdashwidth,
            1.0, 0.51, 0.53,
            min_extents[x], y_pos + dashwidth, z_pos + zdashwidth,
            1.0, 0.51, 0.53,
        
            max_extents[x], y_pos - dashwidth, z_pos - zdashwidth,
            1.0, 0.51, 0.53,
            max_extents[x], y_pos + dashwidth, z_pos + zdashwidth,
            1.0, 0.51, 0.53,

            # y dimension
            #if view != y and max_extents[y] > min_extents[y]:
            
            x_pos, min_extents[y], z_pos,
            1.0, 0.51, 0.53,
            x_pos, max_extents[y], z_pos,
            1.0, 0.51, 0.53,
            
            x_pos - dashwidth, min_extents[y], z_pos - zdashwidth,
            1.0, 0.51, 0.53,
            x_pos + dashwidth, min_extents[y], z_pos + zdashwidth,
            1.0, 0.51, 0.53,

            x_pos - dashwidth, max_extents[y], z_pos - zdashwidth,
            1.0, 0.51, 0.53,
            x_pos + dashwidth, max_extents[y], z_pos + zdashwidth,
            1.0, 0.51, 0.53,

            # z dimension
            #if view != z and max_extents[z] > min_extents[z]:
            x_pos, y_pos, min_extents[z],
            1.0, 0.51, 0.53,
            x_pos, y_pos, max_extents[z],
            1.0, 0.51, 0.53,
            
            x_pos - dashwidth, y_pos - zdashwidth, min_extents[z],
            1.0, 0.51, 0.53,
            x_pos + dashwidth, y_pos + zdashwidth, min_extents[z],
            1.0, 0.51, 0.53,
            
            x_pos - dashwidth, y_pos - zdashwidth, max_extents[z],
            1.0, 0.51, 0.53,
            x_pos + dashwidth, y_pos + zdashwidth, max_extents[z],
            1.0, 0.51, 0.53,
        ])  

    def change_box(self, machine_limit_min, machine_limit_max):
        if self.min_limit != machine_limit_min or self.max_limit != machine_limit_max:
            self.box = self._get_bound_box(machine_limit_min, machine_limit_max)
            self.vbo.arrayupdate(self.axes_size*6*4,
                                 self.box)
            self.min_limit = machine_limit_min
            self.max_limit = machine_limit_max

    def change_extents(self, min_extent, max_extent):
        if self.min_extent != min_extent or self.max_extent != max_extent:
            self.extent_data = self._get_extents((min_extent, max_extent))
            self.vbo.arrayupdate((self.axes_size+self.box_size)*6*4,
                            self.extent_data)
            
            self.min_extent = min_extent
            self.max_extent = max_extent
            
    def _update_matrix(self):
        self.model = glm.mat4()
        self.model = glm.translate(self.model, self.pos)
        self.model = glm.translate(self.model, self.rotate_pos)
        self.model = glm.rotate(self.model, self.rot_x, glm.vec3(1,0,0))
        self.model = glm.rotate(self.model, self.rot_y, glm.vec3(0,1,0))
        self.model = glm.rotate(self.model, self.rot_z, glm.vec3(0,0,1))
        self.model = glm.translate(self.model, -self.rotate_pos)
        self.model = glm.scale(self.model, glm.vec3(self.scale))

    # sets position / center of the scene to this location
    def move(self, pos):
        self.pos = pos
        self._update_matrix()

    def translate(self, delta):
        self.pos += delta
        self._update_matrix()

    def set_rotate_pos(self, pos):
        self.rotate_pos = pos
        self._update_matrix()
        
    def rotate(self, x, y, z):
        self.rot_x = x
        self.rot_y = y
        self.rot_z = z
        self._update_matrix()

    # updates scale of the scene
    def scale(self, scale):
        self.scale = scale
        self._update_matrix()

    def set_feed(self, data):
        self.feed_data = data
        if self.initialized:
            self.feed_vbo.arrayfill(data)
        
    def set_rapids(self, data):
        self.rapids_data = data
        if self.initialized:
            self.rapids_vbo.arrayfill(data)
        
    def render(self,projmat, w, h):
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glViewport(0,0,w,h)

        # render everything with normal lines
        glUseProgram(self.shader_prog)
        
        glUniformMatrix4fv(self.projmat, 1, GL_FALSE, glm.value_ptr(projmat))
        glUniformMatrix4fv(self.modelmat, 1, GL_FALSE, glm.value_ptr(self.model))

        self.vao.bind()
        glDrawArrays(GL_LINES, 0,self.axes_size)
        glDrawArrays(GL_LINES, self.axes_size + self.box_size, self.extent_size)

        self.feed_vao.bind()
        glDrawArrays(GL_LINES, 0, int(len(self.feed_data)/2))

        # render everything with stippled lines
        glUseProgram(self.line_shader_prog)

        glUniformMatrix4fv(self.projmat, 1, GL_FALSE, glm.value_ptr(projmat))
        glUniformMatrix4fv(self.modelmat, 1, GL_FALSE, glm.value_ptr(self.model))
        glUniform2f(self.u_resolution, w, h)
        glUniform1ui(self.u_pattern, 0x1111)
        glUniform1f(self.u_factor, 1)

        self.vao.bind()    
        glDrawArrays(GL_LINES, self.axes_size, self.box_size)

        self.rapids_vao.bind()        
        glDrawArrays(GL_LINES, 0, int(len(self.rapids_data)/2))

        self.vao.unbind()        
        glUseProgram(0)
