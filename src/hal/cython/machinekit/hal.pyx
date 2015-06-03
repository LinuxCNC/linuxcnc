# vim: sts=4 sw=4 et

# Copyright Pavel Shramov, 4/2014
# see http://psha.org.ru/cgit/psha/emc2.git/commit/?h=wip-cython
# License: MIT

# made usable: Michael Haberler 6/2014

cimport cython
cimport hal_const
cimport ring_const
from .hal cimport *
from .rtapi cimport *
from .hal_priv cimport *
from .hal_rcomp cimport *
from .hal_ring cimport *
from .hal_iter cimport *

from os import strerror,getpid

class ComponentExit(Exception):
    pass

class InternalError(Exception):
    pass


TYPE_INVALID = hal_const.TYPE_INVALID
TYPE_RT   =    hal_const.TYPE_RT
TYPE_USER =    hal_const.TYPE_USER
TYPE_REMOTE =  hal_const.TYPE_REMOTE
TYPE_HALLIB =  hal_const.TYPE_HALLIB

COMP_INVALID = hal_const.COMP_INVALID
COMP_INITIALIZING = hal_const.COMP_INITIALIZING
COMP_UNBOUND = hal_const.COMP_UNBOUND
COMP_BOUND = hal_const.COMP_BOUND
COMP_READY = hal_const.COMP_READY

HAL_FLOAT = hal_const.HAL_FLOAT
HAL_S32   = hal_const.HAL_S32
HAL_U32   = hal_const.HAL_U32
HAL_BIT   = hal_const.HAL_BIT
HAL_TYPE_UNSPECIFIED = hal_const.HAL_TYPE_UNSPECIFIED

HAL_IN  = hal_const.HAL_IN
HAL_OUT = hal_const.HAL_OUT
HAL_IO  = hal_const.HAL_IO
HAL_DIR_UNSPECIFIED = hal_const.HAL_DIR_UNSPECIFIED

HAL_RO  = hal_const.HAL_RO
HAL_RW  = hal_const.HAL_RW

PIN_DO_NOT_TRACK = hal_const.PIN_DO_NOT_TRACK

REPORT_BEGIN = hal_const.REPORT_BEGIN
REPORT_SIGNAL = hal_const.REPORT_SIGNAL
REPORT_PIN = hal_const.REPORT_PIN
REPORT_END = hal_const.REPORT_END

TYPE_INVALID = hal_const.TYPE_INVALID
TYPE_RT = hal_const.TYPE_RT
TYPE_USER = hal_const.TYPE_USER
TYPE_REMOTE = hal_const.TYPE_REMOTE


RINGTYPE_RECORD = ring_const.RINGTYPE_RECORD
RINGTYPE_MULTIPART = ring_const.RINGTYPE_MULTIPART
RINGTYPE_STREAM = ring_const.RINGTYPE_STREAM
RINGTYPE_MASK = ring_const.RINGTYPE_MASK

USE_RMUTEX = ring_const.USE_RMUTEX
USE_WMUTEX = ring_const.USE_WMUTEX
ALLOC_HALMEM = ring_const.ALLOC_HALMEM

# allow out pin reads
relaxed = True

include "hal_pin.pyx"
include "hal_pindict.pyx"
include "hal_signal.pyx"
include "hal_component.pyx"
include "hal_compdict.pyx"
include "hal_inst.pyx"
include "hal_instdict.pyx"
include "hal_threads.pyx"
include "hal_funct.pyx"
include "hal_sigdict.pyx"
include "hal_epsilon.pyx"
include "hal_net.pyx"
include "hal_ring.pyx"
include "hal_group.pyx"
include "hal_loadusr.pyx"
include "hal_rcomp.pyx"

# list of component ID's to hal_exit() on unloading the module
cdef list _comps = []

# pity we cant use decorators on cdefs
cdef hal_required():
    global _comps
    if not _comps:
        # dummy comp for connecting to HAL
        p = "machinekit::hal%d" % getpid()
        id = hal_xinit(TYPE_USER, 0, 0, NULL, NULL, p)
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


@cython.final
cdef class HALMutex(object):

    def  __enter__(self):
        rtapi_mutex_get(&hal_data.mutex)
        return hal_data.mutex

    def __exit__(self,exc_type, exc_value, exc_tb):
        rtapi_mutex_give(&hal_data.mutex)
        return 0
