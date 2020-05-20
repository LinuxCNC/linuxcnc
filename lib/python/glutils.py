from OpenGL.GL import *
from OpenGL.GLU import *
from OpenGL.GLUT import *
from OpenGL.GL import shaders

import glm


# this helper function calculates a pixel transformation
# to the fitting object transformation
# vec3 oldpos: the position of the object to be moved
# uint dx, dy: the pixel delta
# mat4 model, proj: model, view, and projection matrices
# vec4 view: the current viewport (set with glViewport)
def px_delta_to_obj(oldpos, dx, dy, model, proj, view):
    # first get the px position of the current object position
    oldpos_px = glm.project(oldpos, model, proj, view)
    # calculate the new position of the object in window/pixel coordinates
    newpos_px = glm.vec3(oldpos_px.x + dx, oldpos_px.y + dy, 0)
    # transform the new position into object space
    newpos = glm.unProject(newpos_px, model, proj, view)
    # calculate the delta of the two positions
    return newpos - oldpos

class GLObject:
    def __init__(self):
        self.initialized = 0

    def __del__(self):
        self.deinit()
        
    def init(self):
        self.initialized = 1

    def deinit(self):
        self.initialized = 0

"""
very simple vertex buffer object abstraction, no error checking performed currently (TODO)

the data_layout dictionary describes the layout of the data like this:

 'shader_name': {
    datatype: GL_TYPE_REPR,
    size: size of the data (bytes) / sizeof(typerepr)
    stride: stride of the data (bytes) between elements
    offset: offset of the data (bytes)
}
"""
class VBO(GLObject):
    def __init__(self):
        super().__init__()
        self.vboid = -1
        self.data_layout = {
            'position': {
                'location': 0,
                'datatype': GL_FLOAT,
                'size': 3,
                'stride': 3*4*2,
                'offset': 0,
            },
            "color": {
                'location': 1,
                'datatype': GL_FLOAT,
                'size': 3,
                'stride': 3*4*2,
                'offset': 3*4,
            }
        }

    def init(self):
        # Generate buffers to hold our vertices
        self.vboid = glGenBuffers(1)
        self.bind()

    def deinit(self):
        glDeleteBuffers(1,self.vboid)
        super().deinit()

    def bind(self):
        glBindBuffer(GL_ARRAY_BUFFER, self.vboid)

    def unbind(self):
        glBindBuffer(GL_ARRAY_BUFFER, 0)

    def bind_vao(self):
        for shadername, layout in self.data_layout.items():
            glEnableVertexAttribArray(layout['location'])
            # normalized currently unused & ignored
            glVertexAttribPointer(layout['location'], layout['size'], layout['datatype'], False, layout['stride'], ctypes.c_void_p(layout['offset']))

    def update(self, offset, data, size):
        self.bind()
        glBufferSubData(GL_ARRAY_BUFFER, offset, size, data)

    def arrayupdate(self, offset, arr):
        self.update(offset, arr.tobytes(), arr.buffer_info()[1] * arr.itemsize)

    def fill(self, data, size):
        self.bind()
        glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW)

    def arrayfill(self, arr):
        self.fill(arr.tobytes(), arr.buffer_info()[1] * arr.itemsize)

class VAO(GLObject):
    def __init__(self):
        self.vaoid=-1
        super().__init__()

    # this binds vbos with their respective bindings
    # to the used shader program
    def init(self, vbos):
        self.vaoid = glGenVertexArrays(1)
        self.bind()
        for vbo in vbos:
            vbo.bind_vao()

        super().init()

    def deinit(self):
        glDeleteVertexArrays(1,self.vaoid)
        super().deinit()
        
    def bind(self):
        glBindVertexArray(self.vaoid)

    def unbind(self):
        glBindVertexArray(0)

class Camera():
    def __init__(self):
        # camera position
        self.position = glm.vec3(0,0,-10)
        self.rot_x = 0
        self.rot_y = 0
        self.rot_z = 0
        self.rotate_pos = glm.vec3(0)
        self.lookpos = glm.vec3(0)

        # View settings
        self.zoomlevel = 10
        self.perspective = 0
        self.fovy = 30.0
        # Position of clipping planes.
        self.near = -1000.0
        self.far = 1000.0

        # projection matrix -> handles ortho / perspective
        self.proj = glm.mat4()
        # view matrix -> handles where the camera looks to/at
        self.view = glm.mat4()

        #self.lookat(self.lookpos)

    # returns the projection-view matrix for convenience
    def get(self):
        return self.proj * self.view

    def setpos(self, pos):
        self.position = pos
        self.update_view()

    def translate(self, delta):
        self.position += delta
        self.update_view()

    def set_rotate_pos(self, pos):
        self.rotate_pos = pos
        self.update_view()
        
    def rotate(self, x, y, z):
        self.rot_x = x
        self.rot_y = y
        self.rot_z = z
        self.update_view()
        
    def zoom(self, level):
        self.zoomlevel *= level
        self.update_view()

    def update_view(self):
        self.view = glm.mat4()
        self.view = glm.translate(self.view, self.rotate_pos)
        self.view = glm.rotate(self.view, self.rot_x, glm.vec3(1,0,0))
        self.view = glm.rotate(self.view, self.rot_y, glm.vec3(0,1,0))
        self.view = glm.rotate(self.view, self.rot_z, glm.vec3(0,0,1))
        self.view = glm.translate(self.view, self.position)
        self.view = glm.scale(self.view, glm.vec3(self.zoomlevel))

    """
    def lookat(self, center):
        self.lookpos = center
        # todo: check if the up vector is always (0,1,0)
        self.view = glm.lookAt(self.position, self.lookpos, glm.vec3(0.0,1.0,0))
    """

    # this method is called on resize and may not contain
    # any gl calls, because the context may not be bound
    # (generally, this class should only calc the matrix and
    # not change anything else)
    def update(self, w, h):
        self.w = w
        self.h = h

        if self.perspective:
            self.proj = glm.perspective(self.fovy,
                                       float(w)/float(h), # aspect ratio
                                       self.near, # clipping planes
                                       self.far)
        else:
            # left, right, bottom, top
            self.proj = glm.ortho(-w/2,w/2,-h/2,h/2,self.near,self.far)
