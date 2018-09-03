from .nanopb  cimport *
from .msginfo cimport *
from libc.stdlib cimport malloc, free
from libc.string cimport memset
from buffer cimport PyBuffer_FillInfo
from cpython.bytes cimport PyBytes_AsString, PyBytes_Size, PyBytes_FromStringAndSize
from cpython.string cimport PyString_FromStringAndSize

cdef class mview:

    cdef void *base
    cdef int size

    def __cinit__(self, long base, size):
        self.base = <void *>base
        self.size = size

    def __getbuffer__(self, Py_buffer *view, int flags):
        r = PyBuffer_FillInfo(view, self, self.base, self.size, 0, flags)
        view.obj = self

# to-rt: takes a protobuf and a msgid, returns a memoryview of the npb cstruct
def pb2nanopb_cstruct(pbuf, unsigned msgid):

    cdef pbmsginfo_t *_mi
    cdef pb_istream_t _istream
    cdef size_t size = PyBytes_Size(pbuf)
    cdef uint8_t *pb = <uint8_t *>PyBytes_AsString(pbuf)

    _istream = pb_istream_from_buffer(pb, size)

    _mi = pbmsgdesc_by_id(msgid)
    if _mi == NULL:
        raise NameError("no such msgid: %d" % (msgid))

    if _mi.encoded_size < 0:
        raise RuntimeError("cannot decode variable-sized messages to a cstruct")

    cdef char* buffer = <char *>malloc(_mi.size)

    if pb_decode(&_istream, _mi.fields, buffer):
        r = memoryview(mview(<long>buffer, _mi.size))
        free(buffer)
        return r

    raise RuntimeError("pb_decode(%s) failed: %s, left=%u" % (
                       _mi.name, PB_GET_ERROR(&_istream), _istream.bytes_left))

# from-rt: takes a memoryview of the cstruct and a msgid, and returns a protobuf
def nanopb_cstruct2pb(cstruct, unsigned msgid):
    cdef pbmsginfo_t *_mi = pbmsgdesc_by_id(msgid)
    if _mi == NULL:
        raise NameError("no such msgid: %d" % (msgid))

    if _mi.encoded_size < 0:
        raise RuntimeError("cannot encode variable-sized messages to a cstruct")

    cdef pb_ostream_t sstream
    memset(&sstream, 0, sizeof(sstream))

    cdef uint8_t *cs = <uint8_t *>PyBytes_AsString(cstruct)
    cdef size_t csize = PyBytes_Size(cstruct)

    if not pb_encode(&sstream, _mi.fields, cs):
        raise RuntimeError("sizing(%s) failed, written=%d" % (
                _mi.name,  sstream.bytes_written))

    cdef bsize = sstream.bytes_written
    cdef uint8_t* buffer = <uint8_t *>malloc(bsize)

    cdef pb_ostream_t rstream = pb_ostream_from_buffer(buffer, bsize)

    if not pb_encode(&rstream, _mi.fields, cs):
        raise RuntimeError("decoding(%s) failed, left=%d" % (
                _mi.name,  rstream.bytes_written))

    r = memoryview(mview(<long>buffer, bsize))
    return r
