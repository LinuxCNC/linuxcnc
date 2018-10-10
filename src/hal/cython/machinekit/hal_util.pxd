# some handy inlines

from cpython.int   cimport PyInt_Check
from cpython.float cimport PyFloat_Check
from cpython.bool  cimport bool

from .hal_priv     cimport hal_shmem_base, hal_data_u, hal_pin_t, hal_sig_t, hal_data
from .hal_priv     cimport pin_type, pin_value, pin_is_linked

from .hal_priv     cimport set_bit_value, set_s32_value, set_u32_value, set_float_value
from .hal_priv     cimport set_s64_value, set_u64_value
from .hal_priv     cimport get_bit_value, get_s32_value, get_u32_value, get_float_value
from .hal_priv     cimport get_s64_value, get_u64_value

from .hal_const    cimport HAL_BIT, HAL_FLOAT,HAL_S32,HAL_U32, HAL_S64,HAL_U64,HAL_TYPE_UNSPECIFIED
from .hal_const    cimport HAL_IN, HAL_OUT, HAL_IO
from .rtapi cimport rtapi_mutex_get,rtapi_mutex_give
import sys

cdef inline void *shmptr(int offset):
    return <void *>(hal_shmem_base + offset)

cdef inline int shmoff(char *ptr):
    return ptr - hal_shmem_base


cdef inline pypin_value(hal_pin_t *pin):
    return hal2py(pin_type(pin), pin_value(pin))


cdef inline hal2py(int t, hal_data_u *dp):
    if t == HAL_BIT:  return get_bit_value(dp)
    elif t == HAL_FLOAT: return get_float_value(dp)
    elif t == HAL_S32:   return get_s32_value(dp)
    elif t == HAL_U32:   return get_u32_value(dp)
    elif t == HAL_S64:   return get_s64_value(dp)
    elif t == HAL_U64:   return get_u64_value(dp)
    else:
        raise RuntimeError("hal2py: invalid type %d" % t)

cdef inline py2hal(int t, hal_data_u *dp, object v):
    cdef bint isint,isfloat

    isint = PyInt_Check(v)
    isfloat = PyFloat_Check(v)

    if not (isint or isfloat):
        raise RuntimeError("int or float expected, got %s" % (type(v)))

    if t == HAL_FLOAT:
        return set_float_value(dp, v)

    elif isint:
        if t == HAL_BIT:
            return set_bit_value(dp, v)
        elif t ==  HAL_S32:
            return set_s32_value(dp, v)
        elif t ==  HAL_U32:
            return set_u32_value(dp, v)
        elif t ==  HAL_S64:
            return set_s64_value(dp, v)
        elif t ==  HAL_U64:
            return set_u64_value(dp, v)
        else:
            raise RuntimeError("invalid HAL object type %d" % (t))
    else:
        raise RuntimeError("py2hal: float value not valid for type: %d" % (t))

