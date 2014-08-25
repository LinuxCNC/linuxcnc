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
from cpython.string cimport PyString_FromStringAndSize
from cpython cimport bool

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
    cdef uint32_t flags,aflags

    def __cinit__(self, char *name,
                  int size = 0,
                  int scratchpad_size = 0,
                  int type = RINGTYPE_RECORD,
                  bool use_rmutex = False,
                  bool use_wmutex = False,
                  bool in_halmem = False):
        self._hr = NULL
        self.flags = (type & RINGTYPE_MASK);
        if use_rmutex: self.flags |= USE_RMUTEX;
        if use_wmutex: self.flags |= USE_RMUTEX;
        if in_halmem:  self.flags |= ALLOC_HALMEM;

        hal_required()
        if size:
            if hal_ring_new(name, size, scratchpad_size, self.flags):
                raise RuntimeError("hal_ring_new(%s) failed: %s" %
                                   (name, hal_lasterror()))
        if hal_ring_attach(name, &self._rb, &self.aflags):
                raise NameError("hal_ring_attach(%s) failed: %s" %
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
                raise IOError("Ring %s write failed: %d %s" %
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
                raise IOError("Ring %s read failed: %d %s" %
                                   (r,self._hr.name))
            return None
        return memoryview(mview(<long>ptr, size))

    def shift(self):
        record_shift(&self._rb)

    def __iter__(self):
        return RingIter(self)

    property available:
        def __get__(self): return record_write_space(self._rb.header)

    property writer:
        '''ring writer attribute. Advisory in nature.'''
        def __get__(self): return self._rb.header.writer
        def __set__(self,int value):  self._rb.header.writer = value

    property reader:
        def __get__(self): return self._rb.header.reader
        def __set__(self,int value):  self._rb.header.reader = value

    property size:
        def __get__(self): return self._rb.header.size

    property type:
        def __get__(self): return self._rb.header.type

    property in_halmem:
        def __get__(self): return (self._hr.flags & ALLOC_HALMEM != 0)

    property rmutex_mode:
        def __get__(self): return (ring_use_rmutex(&self._rb) != 0)

    property wmutex_mode:
        def __get__(self): return (ring_use_wmutex(&self._rb) != 0)

    property name:
        def __get__(self): return self._hr.name

    property scratchpad_size:
        def __get__(self): return ring_scratchpad_size(&self._rb)

    property scratchpad:
        def __get__(self):
            cdef size_t ss = ring_scratchpad_size(&self._rb)
            if ss == 0:
                return None
            return memoryview(mview(<long>self._rb.scratchpad, ss))


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
                raise IOError("Ring read failed")
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


cdef class StreamRing:
    cdef ringbuffer_t *_rb
    cdef hal_ring_t *_hr

    def __cinit__(self, ring):
        self._rb = &(<Ring>ring)._rb
        self._hr = (<Ring>ring)._hr
        if self._rb.header.type != RINGTYPE_STREAM:
            raise RuntimeError("ring '%s' not a stream ring: type=%d" %
                               (self._hr.name,self._rb.header.type))

    def spaceleft(self):
        '''return number of bytes available to write.'''

        return stream_write_space(self._rb.header)

    def flush(self):
        '''clear the buffer contents. Note this is not thread-safe
        unless all readers and writers use a r/w mutex.'''

        return stream_flush(self._rb)

    def consume(self, int nbytes):
        '''remove argument number of bytes from stream.
        May raise IOError if more than the number of
        available bytes are consumed'''

        cdef size_t avail
        avail = stream_read_space(self._rb.header);
        if (nbytes > <int>avail):
            raise IOError("consume(%d): argument larger than bytes available (%zu)" %
                          (nbytes, avail))
        stream_read_advance(self._rb, nbytes);

    def next(self):
        '''returns the number of bytes readable or 0 if no data is available.'''
        return stream_read_space(self._rb.header)

    def write(self, s):
        '''write to ring. Returns 0 on success.
        nozero return value indicates the number
	of bytes actually written. '''

        return stream_write(self._rb,  PyBytes_AsString(s), PyBytes_Size(s))

    def read(self):
        ''' return all bytes readable as a string, or None'''
        cdef ringvec_t v[2]
        stream_get_read_vector(self._rb, v)

        if v[0].rv_len:
            b1 = PyString_FromStringAndSize(<const char *>v[0].rv_base, v[0].rv_len)
            if v[1].rv_len == 0:
                stream_read_advance(self._rb,v[0].rv_len)
                return b1

            b2 = PyString_FromStringAndSize(<const char *>v[1].rv_base, v[1].rv_len)
            stream_read_advance(self._rb,v[0].rv_len + v[1].rv_len)
            return b1 + b2
        return None


cdef class MultiframeRing:
    cdef msgbuffer_t _rb
    cdef object _pyring

    def __cinit__(self, ring):
        self._pyring = ring
        self._rb.ring = &(<Ring>ring)._rb
        self._rb.ring.header.type = RINGTYPE_MULTIPART

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
            raise IOError("Ring write failed")
        return False

    def flush(self):
        msg_write_flush(&self._rb)

    def ready(self):
        return record_next_size(self._rb.ring) > -1

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
