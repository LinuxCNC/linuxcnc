# rtapi_compat.c  python bindings

from .compat cimport *
from os import strerror


cdef Flavor_Init(flavor_t *f):
      result = Flavor()
      result._f = f
      return result

cdef class Flavor:
    cdef flavor_t *_f

    property name:
        def __get__(self): return self._f.name

    property mod_ext:
        def __get__(self): return self._f.mod_ext

    property so_ext:
        def __get__(self): return self._f.so_ext

    property build_sys:
        def __get__(self): return self._f.build_sys

    property id:
        def __get__(self): return self._f.id

    property flags:
        def __get__(self): return self._f.flags

def is_module_loaded(m):
    return c_is_module_loaded(m)

def kernel_is_xenomai():
    return bool(c_kernel_is_xenomai())

def kernel_is_rtai():
    return bool(c_kernel_is_rtai())

def kernel_is_rtpreempt():
    return bool(c_kernel_is_rtpreempt())

def xenomai_gid():
    return c_xenomai_gid()

def user_in_xenomai_group():
    return bool(c_user_in_xenomai_group())

def kernel_instance_id():
    return c_kernel_instance_id()

def flavor_byname(name):
    cdef flavor_t *f = c_flavor_byname(name)
    if f == NULL:
        raise RuntimeError("flavor_byname: no such flavor: %s" % name)
    return Flavor_Init(f)

def flavor_byid(id):
    cdef flavor_t *f = c_flavor_byid(id)
    if f == NULL:
        raise RuntimeError("flavor_byid: no such flavor: %d" % id)
    return Flavor_Init(f)


def default_flavor():
    cdef flavor_t *f = c_default_flavor()
    if f == NULL:
        raise RuntimeError("BUG: flavor() failed")
    return Flavor_Init(f)

def run_module_helper(name):
    rc = c_run_module_helper(name)
    if rc:
        raise RuntimeError("module_helper(%s) failed: %d %s " % (name, rc, strerror(rc)))

def module_path(basename):
    cdef char result[1024]
    rc = c_module_path(result, basename)
    if rc:
        raise RuntimeError("modpath(%s) failed: %d %s " % (basename, rc, strerror(-rc)))
    return str(<char *>result)

def get_rtapi_config(param):
    cdef char result[1024]
    rc = c_get_rtapi_config(result, param, 1024)
    if rc:
        raise RuntimeError("get_rtapiconfig(%s) failed: %d %s " % (param, rc, strerror(-rc)))
    return str(<char *>result)
