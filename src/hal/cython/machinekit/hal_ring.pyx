# TODO:
# make create and attach free factory functions
# which return a Stream/Record/MessageRing wrapper depending on type
# make wrapper __dealloc__ a detach (in place)
# create StreamRing wrapper
# predefine ring flags bits in ring.pxd, export
# use the halpr_foreach_ring() iterator to create ring namelist

from libc.errno cimport EAGAIN
from libc.string cimport memcpy
from buffer cimport PyBuffer_FillInfo
from cpython.bytes cimport PyBytes_AsString, PyBytes_Size, PyBytes_FromStringAndSize

from .ring cimport *

cdef class mview:
    cdef void *base
    cdef int size

    def __cinit__(self, long base, size):
        self.base = <void *>base
        self.size = size

    def __getbuffer__(self, Py_buffer *view, int flags):
        r = PyBuffer_FillInfo(view, self, self.base, self.size, 0, flags)
        view.obj = self

cdef class Ring:
    cdef ringbuffer_t _rb
    cdef hal_ring_t *_hr
    cdef uint32_t aflags

    def __cinit__(self, char *name,
                  int size = 0,
                  int sp_size = 0,
                  unsigned int flags = 0):
        self._hr = NULL
        hal_required()
        if size:
            if hal_ring_new(name, size, sp_size, flags):
                raise RuntimeError("hal_ring_new(%s) failed: %s" %
                                   (name, hal_lasterror()))
        if hal_ring_attach(name, &self._rb, &self.aflags):
                raise RuntimeError("hal_ring_attach(%s) failed: %s" %
                                   (name, hal_lasterror()))

        with HALMutex():
            self._hr = halpr_find_ring_by_name(name)
            if self._hr == NULL:
                raise RuntimeError("halpr_find_ring_by_name(%s) failed: %s" %
                                   (name, hal_lasterror()))
    def __dealloc__(self):
        if self._hr != NULL:
            name = self._hr.name
            r = hal_ring_detach(self._hr.name, &self._rb)
            if r:
                raise RuntimeError("hal_ring_detach() failed: %d %s" %
                                       (r, hal_lasterror()))

    def write(self, s):
        cdef void * ptr
        cdef size_t size = PyBytes_Size(s)
        cdef int r = record_write_begin(&self._rb, &ptr, size)
        if r:
            if r != EAGAIN:
                raise RuntimeError("Ring %s write failed: %d %s" %
                                   (r,self._hr.name))
            return False
        memcpy(ptr, PyBytes_AsString(s), size)
        record_write_end(&self._rb, ptr, size)
        return True

    def read(self):
        cdef const void * ptr
        cdef size_t size

        cdef int r = record_read(&self._rb, &ptr, &size)
        if r:
            if r != EAGAIN:
                raise RuntimeError("Ring %s read failed: %d %s" %
                                   (r,self._hr.name))
            return None
        return memoryview(mview(<long>ptr, size))

    def shift(self):
        record_shift(&self._rb)

    def __iter__(self):
        return RingIter(self)

    property writer:
        '''ring writer attribute. Advisory in nature.'''
        def __get__(self): return self._rb.header.writer
        def __set__(self,int value):  self._rb.header.writer = value

    property reader:
        def __get__(self): return self._rb.header.reader
        def __set__(self,int value):  self._rb.header.reader = value

    property size:
        def __get__(self): return self._rb.header.size

    property sp_size:
        def __get__(self): return ring_scratchpad_size(&self._rb)

    property type:
        def __get__(self): return self._rb.header.type

    property in_halmem:
        def __get__(self): return self._hr.flags & ALLOC_HALMEM

    property rmutex_mode:
        def __get__(self): return ring_use_rmutex(&self._rb)

    property wmutex_mode:
        def __get__(self): return ring_use_wmutex(&self._rb)

    property name:
        def __get__(self): return self._hr.name

    property scratchpad_size:
        def __get__(self): return ring_scratchpad_size(&self._rb)

    property scratchpad:
        def __get__(self):
            cdef size_t ss = ring_scratchpad_size(&self._rb)
            if ss == 0:
                return None
            return memoryview(mview(<long>&self._rb.scratchpad, ss))


cdef class RingIter:
    cdef ringiter_t _iter

    def __cinit__(self, Ring ring):
        if record_iter_init(&(<Ring>(ring))._rb, &self._iter):
            raise RuntimeError("Failed to initialize ring iter")

    def read(self):
        cdef const void * ptr
        cdef size_t size
        cdef int r = record_iter_read(&self._iter, &ptr, &size)
        if r:
            if r != EAGAIN:
                raise RuntimeError("Ring read failed")
            return None
        return memoryview(mview(<long>ptr, size))

    def shift(self):
        record_iter_shift(&self._iter)

    def __iter__(self):
        return self

    def __next__(self):
        r = self.read()
        if r is None:
            raise StopIteration("Ring is empty")
        s = memoryview(r.tobytes())
        self.shift()
        return s

    def next(self): return self.__next__()

cdef class ringvec:
    cdef mview data
    cdef int flags

    def __cinit__(self, long base, size, flags):
        self.data = mview(base, size)
        self.flags = flags

    property data:
        def __get__(self):
            return memoryview(self.data)

    property flags:
        def __get__(self): return self.flags

cdef class BufRing:
    cdef msgbuffer_t _rb
    cdef object _pyring

    def __cinit__(self, ring):
        self._pyring = ring
        self._rb.ring = &(<Ring>ring)._rb

    def __init__(self, ring):
        pass

    def read(self):
        cdef ringvec_t rv
        msg_read_abort(&self._rb)
        while frame_readv(&self._rb, &rv) == 0:
            yield ringvec(<long>rv.rv_base, rv.rv_len, rv.rv_flags)
            frame_shift(&self._rb)

    def shift(self):
        msg_read_flush(&self._rb)

    def write(self, s, flags = 0):
        cdef void * ptr
        cdef size_t size = PyBytes_Size(s)
        r = frame_write(&self._rb, PyBytes_AsString(s), size, flags)
        if not r:
            return True
        if r != EAGAIN:
            raise RuntimeError("Ring write failed")
        return False

    def flush(self):
        msg_write_flush(&self._rb)

# hal_iter callback: add ring names into list
cdef int _collect_ring_names(hal_ring_t *ring,  void *userdata):
    arg =  <object>userdata
    arg.append(ring.name)
    return 0

def rings():
    ''' return list of ring names'''
    hal_required()
    names = []
    with HALMutex():
        rc = halpr_foreach_ring(NULL, _collect_ring_names, <void *>names);
        if rc < 0:
            raise RuntimeError("halpr_foreach_ring failed %d" % rc)
    return names
