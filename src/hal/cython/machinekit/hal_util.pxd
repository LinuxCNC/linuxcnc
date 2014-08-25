# some handy inlines

from cpython.int   cimport PyInt_Check
from cpython.float cimport PyFloat_Check
from cpython.bool  cimport bool

from .hal_priv     cimport hal_shmem_base, hal_data_u, hal_pin_t, hal_sig_t, hal_data
from .hal_const    cimport HAL_BIT, HAL_FLOAT,HAL_S32,HAL_U32, HAL_TYPE_UNSPECIFIED
from .hal_const    cimport HAL_IN, HAL_OUT, HAL_IO
from .rtapi cimport rtapi_mutex_get,rtapi_mutex_give
import sys

cdef inline void *shmptr(int offset):
    return <void *>(hal_shmem_base + offset)

cdef inline int shmoff(char *ptr):
    return ptr - hal_shmem_base


cdef inline pin_value(hal_pin_t *pin):
    cdef hal_sig_t *sig
    cdef hal_data_u *dp
    if pin.signal != 0:
        sig = <hal_sig_t *>shmptr(pin.signal)
        dp = <hal_data_u *>shmptr(sig.data_ptr)
    else:
        dp = <hal_data_u *>shmptr(shmoff(<char *>&pin.dummysig))
    return hal2py(pin.type, dp)


cdef inline hal2py(int t, hal_data_u *dp):
    if t == HAL_BIT:  return dp.b
    elif t == HAL_FLOAT: return dp.f
    elif t == HAL_S32:   return dp.s
    elif t == HAL_U32:   return dp.u
    else:
        raise RuntimeError("hal2py: invalid type %d" % t)

cdef inline py2hal(int t, hal_data_u *dp, object v):
    cdef bint isint,isfloat

    isint = PyInt_Check(v)
    isfloat = PyFloat_Check(v)

    if not (isint or isfloat):
        raise RuntimeError("int or float expected, got %s" % (type(v)))

    if t == HAL_FLOAT:
        dp.f = v
        return dp.f

    elif isint:
        if t == HAL_BIT:
            dp.b = v
            return dp.b
        elif t ==  HAL_S32:
            dp.s = v
            return dp.s
        elif t ==  HAL_U32:
            dp.u = v
            return dp.u
        else:
            raise RuntimeError("invalid HAL object type %d" % (t))
    else:
        raise RuntimeError("py2hal: float value not valid for type: %d" % (t))

cdef inline bint pin_linked(hal_pin_t *p):
    return p.signal != 0

cdef inline hal_sig_t * linked_signal(hal_pin_t *pin):
    cdef hal_sig_t *sig
    if not pin_linked(pin): return NULL
    return <hal_sig_t *>shmptr(pin.signal)


cdef inline bint valid_type(int type):
    return type in [HAL_BIT, HAL_FLOAT,HAL_S32,HAL_U32]

cdef inline bint valid_dir(int type):
    return type in [HAL_IN, HAL_OUT, HAL_IO]
