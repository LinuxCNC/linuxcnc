cimport chalpy

class HalException(Exception):
    pass

BIT = 1
FLOAT = 2
S32 = 3
U32 = 4
IN = 16
OUT = 32
IO = IN | OUT

cdef class Pin(object):
    cdef char * full_name
    cdef public chalpy.hal_type_t type
    cdef public chalpy.hal_pin_dir_t dir
    cdef public int id
    cdef chalpy.pinunion *ptr

    def __cinit__(self, full_name, type, dir, id):
        self.type = type
        self.dir = dir
        self.id = id
        self.ptr = <chalpy.pinunion *> chalpy.hal_malloc(sizeof(chalpy.pinunion))
        result = chalpy.hal_pin_new(full_name, type, dir, <void **>self.ptr, id)
        if result < 0:
            raise HalException('failed to create pin "{0}" with code {1}'.format(full_name, result))

    def get(self):
        if self.type is FLOAT:
            return self.ptr.f[0]
        elif self.type is U32:
            return self.ptr.u32[0]
        else:
            return self.ptr.s32[0]

    def set(self, val):
        if self.type is FLOAT:
            self.ptr.f[0] = val
        elif self.type is U32:
            self.ptr.u32[0] = val
        else:
            self.ptr.s32[0] = val

# Boiler code
    def __getitem__(self, item):
        return getattr(self, item)

cdef class component(object):
    cdef int id
    cdef public dict pins

    def __cinit__(self, pname):
        global COMP_NAME
        self.id = chalpy.hal_init(pname.encode())
        self.pins = {}
        COMP_NAME = pname.encode()
        if self.id < 0:
            raise HalException('failed to initialized component "{0}" return # {1}'.format(pname,self.id))

    def set_prefix(self, prename):
        global COMP_NAME
        COMP_NAME = prename.encode()

    def get_prefix(self):
        return COMP_NAME.decode()

    def ready(self):
        result = chalpy.hal_ready(self.id)
        if result < 0:
            raise HalException('hal_ready failed with code {0}'.format(result))

    def exit(self):
        result = chalpy.hal_exit(self.id)
        if result < 0:
            raise HalException('hal_exit failed with code {0}'.format(result))

    def new_signal(self, sname, type):
        rtn = chalpy.hal_signal_new(sname.encode(), type)

    def signal_delete(self, sname):
        rtn = chalpy.hal_signal_delete(sname.encode())

    def connect(self, pname, sname):
        result = chalpy.hal_link(pname.encode(), sname.encode())
        if result < 0:
            raise HalException('failed to link sig "{0}" to pin "{1}" with result {2}'.format(sname, pname, result))

    def unlink(self, pname):
        rtn = chalpy.hal_unlink(pname.encode())

    def new_pin(self, pname, type, dir):
        if not type in (BIT,FLOAT,S32,U32):
            raise HalException('failed to create pin "{0}". Unknown type {1}'.format(pname, type))
        if not dir in (IN,OUT,IO):
            raise HalException('failed to create pin "{0}". Unknown dir {1}'.format(pname, dir))
        full_name = COMP_NAME+ b'.' + pname.encode()
        self.pins[pname] = Pin(full_name, type, dir, self.id)

    def __getitem__(self, item):
        try:
            return getattr(self, item)
        except:
            return self.pins[item]

    def __getattr__(self, attr):
        try:
            return self.pins[attr]
        except:
            return getattr(self, attr)
