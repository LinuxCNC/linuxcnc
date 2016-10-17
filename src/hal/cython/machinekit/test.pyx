
cimport cython
cimport hal_const
cimport ring_const
from .hal cimport *
from .rtapi cimport *
#from .hal_object cimport *
from .hal_priv cimport *
from .hal_rcomp cimport *
from .hal_ring cimport *
from .hal_objectops cimport *
from cpython.bool cimport *
from os import strerror,getpid


TYPE_USER =    hal_const.TYPE_USER

# testiter callback: add pin names into list
cdef int _testme(hal_object_ptr o,  foreach_args_t *args):
    print "CB!"
    arg =  <object>args.user_ptr1
    if  isinstance(arg, list):
        arg.append(hh_get_name(o.hdr))
        return 0
    else:
        return -1

cdef foreach_args_t nullargs

def testiter():
    hal_required()
    names = []
    # bit of a cludge - cython cant do struct initializers
    cdef foreach_args_t args = nullargs
    args.type = hal_const.HAL_PIN
    args.user_ptr1 = <void *>names
    # run iterator with HAL mutex held:
    rc = halg_foreach(1, &args, _testme)
    if rc < 0:
        raise RuntimeError("colossal failure!")
    return names




# ----- management noise: connect to/disconnect from HAL as needed

# list of component ID's to hal_exit() on unloading the module
cdef list _comps = []

# pity we cant use decorators on cdefs
cdef hal_required():
    global _comps
    if not _comps:
        # dummy comp for connecting to HAL
        p = "machinekit::hal%d" % getpid()
        id = hal_init(p)
        if hal_data == NULL:
            raise RuntimeError("cant connect to HAL - realtime not running?")
        hal_ready(id)
        _comps.append(id)

def _atexit():
    # remove all usercomps created herein, including dummy
    global _comps
    for c in _comps:
        hal_exit(c)
    _comps = []


import atexit

atexit.register(_atexit)

# halcmd will send a SIGTERM on unloadusr <name>
# make it look like a KeyboardInterrupt
(lambda s=__import__('signal'):
     s.signal(s.SIGTERM, s.default_int_handler))()
