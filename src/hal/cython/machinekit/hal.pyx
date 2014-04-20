# vim: sts=4 sw=4 et

# Copyright Pavel Shramov, 4/2014
# see http://psha.org.ru/cgit/psha/emc2.git/commit/?h=wip-cython
# License: MIT

from .hal cimport *
from os import strerror

class hal_type(int):
    Bit = HAL_BIT
    Float = HAL_FLOAT
    S32 = HAL_S32
    U32 = HAL_U32

    names = \
        { HAL_TYPE_UNSPECIFIED: "HAL_TYPE_UNSPECIFIED"
        , Bit: "HAL_BIT"
        , Float: "HAL_FLOAT"
        , S32: "S32"
        , U32: "U32"
        }

    def __repr__(self):
        return "<hal_type_t %s>" % self.names[self]

class hal_pin_dir(int):
    In = HAL_IN
    Out = HAL_OUT
    IO = HAL_IO

    names = \
        { HAL_DIR_UNSPECIFIED: "HAL_DIR_UNSPECIFIED"
        , In: "HAL_IN"
        , Out: "HAL_OUT"
        , IO: "HAL_IO"
        }

    def __repr__(self):
        return "<hal_pin_dir_t %s>" % self.names[self]

cdef union _pin_storage:
    hal_bit_t * b
    hal_float_t * f
    hal_s32_t * s
    hal_u32_t * u

cdef class _Pin:
    cdef object _name
    cdef _pin_storage *_storage
    cdef hal_type_t _type
    cdef hal_pin_dir_t _dir

    def __cinit__(self, comp, name, t, dir):
        self._storage = NULL
        self._storage = <_pin_storage *>hal_malloc(sizeof(_pin_storage))
        if self._storage == NULL:
            raise RuntimeError("Fail to allocate HAL memory")
        name = "{}.{}".format(comp.name, name)
        self._name = name

        r = hal_pin_new(name, t, dir, <void **>(self._storage), (<Component>comp)._id)
        if r:
            raise RuntimeError("Fail to create pin %s: %s" % (name, strerror(-r)))

    def link(self, sig):
        if isinstance(sig, Signal):
            sig = sig._name
        r = hal_link(self._name, sig)
        print("link", self._name, sig, r)
        if r:
            raise RuntimeError("Fail to link pin %s to %s: %s" % (self._name, sig, strerror(-r)))
    def unlink(self):
        r = hal_unlink(self._name)
        if r:
            raise RuntimeError("Fail to unlink pin %s: %s" % (self._name, strerror(-r)))

    def _set_bit(self, v): self._storage.b[0] = bool(v)
    def _set_float(self, float v): self._storage.f[0] = v
    def _set_s32(self, int v): self._storage.s[0] = v
    def _set_u32(self, int v): self._storage.u[0] = v

    def _get_bit(self): return bool(self._storage.b[0])
    def _get_float(self): return self._storage.f[0]
    def _get_s32(self): return self._storage.s[0]
    def _get_u32(self): return self._storage.u[0]

class Pin(_Pin):
    def __init__(self, comp, name, t, dir):
        if dir != hal_pin_dir.In:
            if t == hal_type.Bit: self.set = self._set_bit
            elif t == hal_type.Float: self.set = self._set_float
            elif t == hal_type.S32: self.set = self._set_s32
            elif t == hal_type.U32: self.set = self._set_u32
        if dir != hal_pin_dir.Out:
            if t == hal_type.Bit: self.get = self._get_bit
            elif t == hal_type.Float: self.get = self._get_float
            elif t == hal_type.S32: self.get = self._get_s32
            elif t == hal_type.U32: self.get = self._get_u32
    def set(self, v): raise NotImplementedError("Pin is read-only")
    def get(self): raise NotImplementedError("Pin is write-only")

class Signal:
    def __init__(self, name, type):
        self._name, self._type = name, type
        r = hal_signal_new(name, type)
        if r:
            raise RuntimeError("Fail to create signal %s: %s" % (name, strerror(-r)))

    def link(self, *pins):
        for p in pins:
            if isinstance(p, Pin):
                p.link(self._name)
            else:
                r = hal_link(self._p, self._name)
                print("link", self._p, self._name, r)
                if r:
                    raise RuntimeError("Fail to link pin %s to %s: %s" % (p, self._name, strerror(-r)))

cdef class Component:
    cdef int _id
    cdef object _name
    def __cinit__(self, name):
        self._name = name
        self._id = -1
        self._id = hal_init(name)
        if self._id < 0:
            raise RuntimeError("Fail to create comp: %s" % strerror(-self._id))

    def __dealloc__(self):
        if self._id > 0:
            hal_exit(self._id)

    def ready(self):
        hal_ready(self._id)

    def pin(self, *a):
        return Pin(self, *a)

    def pin_bit(self, name, dir): return Pin(self, name, hal_type.Bit, dir)
    def pin_float(self, name, dir): return Pin(self, name, hal_type.Float, dir)
    def pin_s32(self, name, dir): return Pin(self, name, hal_type.S32, dir)
    def pin_u32(self, name, dir): return Pin(self, name, hal_type.U32, dir)

    property name:
        def __get__(self): return self._name
