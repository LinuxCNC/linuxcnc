
# RTAPI python bindings
# shared memory
# RT logger
# rtapi_app command interface

from .rtapi cimport *
from .global_data cimport *
from .rtapi_app cimport *
from os import strerror, getpid
from libc.stdlib cimport malloc, free
from cpython.string cimport PyString_AsString


# the PyBuffer_FillInfo declaration is broken in cython 0.19
# from cpython.buffer cimport PyBuffer_FillInfo
# use a temporary replacement
# XXX make this conditional on cython version
from buffer cimport PyBuffer_FillInfo

_HAL_KEY                   = HAL_KEY
_RTAPI_KEY                 = RTAPI_KEY
_RTAPI_RING_SHM_KEY        = RTAPI_RING_SHM_KEY
_GLOBAL_KEY                = GLOBAL_KEY
_DEFAULT_MOTION_SHMEM_KEY  = DEFAULT_MOTION_SHMEM_KEY
_SCOPE_SHM_KEY             = SCOPE_SHM_KEY

_KERNEL = MSG_KERNEL
_RTUSER = MSG_RTUSER
_ULAPI = MSG_ULAPI

MSG_NONE = RTAPI_MSG_NONE
MSG_ERR =  RTAPI_MSG_ERR
MSG_WARN = RTAPI_MSG_WARN
MSG_INFO = RTAPI_MSG_INFO
MSG_DBG = RTAPI_MSG_DBG
MSG_ALL = RTAPI_MSG_ALL

cdef class mview:
    cdef void *base
    cdef int size

    def __cinit__(self, long base, size):
        self.base = <void *>base
        self.size = size

    def __getbuffer__(self, Py_buffer *view, int flags):
        r = PyBuffer_FillInfo(view, self, self.base, self.size, 0, flags)
        view.obj = self

cdef class RtapiModule:
    cdef char * _name
    cdef int _id

    def __cinit__(self, char * name=""):
        self._id = -1
        pyname = name.encode('UTF-8')
        if pyname == "":
            pyname = "cyrtapimodule%d" % getpid()
            py_byte_string = pyname.encode('UTF-8')
            self._name = py_byte_string
        else:
            self._name = name
        self._id = rtapi_init(self._name)
        if self._id < 0:
            raise RuntimeError("Fail to create RTAPI - realtime not running?  %s" % strerror(-self._id))

    def __dealloc__(self):
        if self._id > 0:
            #print "---__dealloc__ rtapi_exit", self._name
            rtapi_exit(self._id)

    def shmem(self, int key, unsigned long size = 0):
        cdef void *ptr
        cdef int shmid

        shmid = rtapi_shmem_new(key, self._id, size)
        if shmid < 0:
            raise RuntimeError("shm segment 0x%x/%d does not exist" % (key,key))
        retval = rtapi_shmem_getptr(shmid, &ptr, &size);
        if retval < 0:
            raise RuntimeError("getptr shm 0x%x/%d failed %d" % (key,key,retval))
        return memoryview(mview(<long>ptr, size))

    property name:
        def __get__(self): return self._name

    property id:
        def __get__(self): return self._id


# duck-type a File object so one can do this:
# log = RTAPILogger(level=rtapi.MSG_ERR,tag="marker")
# print >> log, "some message",
# and it will got to the shared RT log handled by msgd

cdef class RTAPILogger:
    cdef int _level
    cdef char *_tag

    def __init__(self, char *tag="cython", int level=RTAPI_MSG_ALL):
        rtapi_required()
        rtapi_set_logtag(tag)
        self._level = level
        self._tag = tag

    def write(self,line):
        l = line.rstrip(" \t\f\v\n\r")
        rtapi_print_msg(self._level, l)

    def flush(self):
        pass

    property tag:
        def __get__(self): return self._tag
        def __set__(self, char *tag):
            self._tag = tag
            rtapi_set_logtag(tag)

    property level:
        def __get__(self): return self._level
        def __set__(self, int level): self._level = level


# helper - mutate a string list into a char**argv type array, zero-terminated
# with a NULL pointer
cdef char ** _to_argv(args):
    l = len(args)
    cdef char **ret = <char **>malloc((l+1) * sizeof(char *))
    if not ret:
        raise MemoryError()
    for i in xrange(l):
        ret[i] = PyString_AsString(args[i])
    ret[l] =  NULL # zero-terminate
    return ret

import sys
import os
from machinekit import hal, config
if sys.version_info >= (3, 0):
    import configparser
else:
    import ConfigParser as configparser

# enums for classify_comp
CS_NOT_LOADED = 0
CS_NOT_RT = 1
CS_RTLOADED_NOT_INSTANTIABLE = 2
CS_RTLOADED_AND_INSTANTIABLE = 3

autoload = True  #  autoload components on newinst

# classifies a component for newinst
def classify_comp(comp):
    if not comp in hal.components:
        return CS_NOT_LOADED
    c = hal.components[comp]
    if c.type is not hal.TYPE_RT:
        return CS_NOT_RT
    if not c.has_ctor:
        return CS_RTLOADED_NOT_INSTANTIABLE
    return CS_RTLOADED_AND_INSTANTIABLE


class RTAPIcommand:
    ''' connect to the rtapi_app RT demon to pass commands '''

    def __init__(self, uuid="", instance=0, uri=""):
        rtapi_required()
        cdef char* c_uuid  = uuid
        cdef char* c_uri = uri
        if uri == "":
            c_uri = NULL
        if uuid == "" and uri == "":  # try to get the uuid from the ini
            mkconfig = config.Config()
            mkini = os.getenv("MACHINEKIT_INI")
            if mkini is None:
                mkini = mkconfig.MACHINEKIT_INI
            if not os.path.isfile(mkini):
                raise RuntimeError("MACHINEKIT_INI " + mkini + " does not exist")

            cfg = configparser.ConfigParser()
            cfg.read(mkini)
            try:
                uuid = cfg.get("MACHINEKIT", "MKUUID")
            except configparser.NoSectionError or configparser.NoOptionError:
                raise RuntimeError("need either a uuid=<uuid> or uri=<uri> parameter")
            c_uuid = uuid
        r = rtapi_connect(instance, c_uri, c_uuid)
        if r:
            raise RuntimeError("cant connect to rtapi: %s" % strerror(-r))

    def newthread(self,char *name, int period, instance=0,fp=0,cpu=-1, flags=0):
        cdef char *c_name = name
        r = rtapi_newthread(instance, c_name, period, cpu, fp, flags)
        if r:
            raise RuntimeError("rtapi_newthread failed:  %s" % strerror(-r))

    def delthread(self,char *name, instance=0):
        cdef char *c_name = name
        r = rtapi_delthread(instance, c_name)
        if r:
            raise RuntimeError("rtapi_delthread failed:  %s" % strerror(-r))

    def loadrt(self,*args, instance=0, **kwargs):
        cdef char** argv
        cdef char *name

        if len(args) < 1:
            raise RuntimeError("loadrt needs at least the module name as argument")
        name = args[0]
        for key in kwargs.keys():
            args +=('%s=%s' % (key, str(kwargs[key])), )
        argv = _to_argv(args[1:])
        r = rtapi_loadrt( instance, name, <const char **>argv)
        free(argv)
        if r:
            raise RuntimeError("rtapi_loadrt '%s' failed: %s" % (args,strerror(-r)))

        return hal.components[name]

    def unloadrt(self,char *name, int instance=0):
        if name == NULL:
            raise RuntimeError("unloadrt needs at least the module name as argument")
        r = rtapi_unloadrt( instance, name)
        if r:
            raise RuntimeError("rtapi_unloadrt '%s' failed: %s" % (name,strerror(-r)))

    def newinst(self, *args, instance=0, **kwargs):
        cdef char** argv
        cdef char** tmpArgv
        cdef char *name

        if len(args) < 2:
            raise RuntimeError("newinst needs at least module and instance name as argument")
        comp = args[0]
        instname = args[1]
        for key in kwargs.keys():
            args +=('%s=%s' % (key, str(kwargs[key])), )
        argv = _to_argv(args[2:])

        status = classify_comp(comp)
        if status is CS_NOT_LOADED:
            if autoload:  # flag to prevent creating a new instance
                tmpArgv = _to_argv([])
                rtapi_loadrt(instance, comp, <const char **>tmpArgv)
            else:
                raise RuntimeError("component '%s' not loaded\n" % comp)
        elif status is CS_NOT_RT:
            raise RuntimeError("'%s' not an RT component\n" % comp)
        elif status is CS_RTLOADED_NOT_INSTANTIABLE:
            raise RuntimeError("legacy component '%s' loaded, but not instantiable\n" % comp)
        elif status is CS_RTLOADED_AND_INSTANTIABLE:
            pass  # good

        #TODO check singleton

        if instname in hal.instances:
            raise RuntimeError('instance with name ' + instname + ' already exists')

        r = rtapi_newinst(instance, comp, instname, <const char **>argv)
        free(argv)
        if r:
            raise RuntimeError("rtapi_newinst '%s' failed: %s" % (args,strerror(-r)))

        return hal.instances[instname]

    def delinst(self, char *instname, instance=0):
        r = rtapi_delinst( instance, instname)
        if r:
            raise RuntimeError("rtapi_delinst '%s' failed: %s" % (instname,strerror(-r)))


# default module to connect to RTAPI:
__rtapimodule = None
cdef rtapi_required():
    global __rtapimodule
    if not __rtapimodule:
        __rtapimodule = RtapiModule("")
    if not __rtapimodule:
        raise RuntimeError("cant connect to RTAPI - realtime not running?")

def _atexit():
    if __rtapimodule:
        rtapi_exit(__rtapimodule.id)

import atexit
atexit.register(_atexit)

(lambda s=__import__('signal'):
     s.signal(s.SIGTERM, s.default_int_handler))()

# global RTAPIcommand to use in HAL config files
__rtapicmd = None
def init_RTAPI(**kwargs):
    global __rtapicmd
    if not __rtapicmd:
        __rtapicmd = RTAPIcommand(**kwargs)
        for method in dir(__rtapicmd):
            if callable(getattr(__rtapicmd, method)) and not method.startswith('__'):
                 setattr(sys.modules[__name__], method, getattr(__rtapicmd, method))
    else:
        raise RuntimeError('RTAPIcommand already initialized')
    if not __rtapicmd:
        raise RuntimeError('unable to initialize RTAPIcommand - realtime not running?')

