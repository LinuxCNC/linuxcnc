# Cython cdefs for ring.h and multiframe.h

from libc.stdint cimport uint64_t, uint8_t, uint32_t, int32_t
from libc.stddef cimport size_t
from .rtapi cimport rtapi_atomic_type

cdef extern from "ring.h":
    int RINGTYPE_RECORD
    int RINGTYPE_MULTIPART
    int RINGTYPE_STREAM
    int RINGTYPE_MASK

    int USE_RMUTEX
    int USE_WMUTEX
    int ALLOC_HALMEM

    ctypedef int32_t ring_size_t

    ctypedef struct ringheader_t:
        uint8_t  type
        uint8_t  use_rmutex
        uint8_t  use_wmutex
        uint8_t  alloc_halmem
        uint32_t userflags
        int32_t refcount
        int32_t reader
        int32_t writer
        int32_t reader_instance
        int32_t writer_instance
        rtapi_atomic_type rmutex
        rtapi_atomic_type wmutex
        size_t trailer_size
        size_t size_mask
        size_t size
        size_t head
        uint64_t  generation
        uint8_t *buf

    ctypedef struct ringtrailer_t:
        size_t tail
        uint8_t scratchpad_buf[0]

    ctypedef struct ringbuffer_t:
        int32_t        magic
        ringheader_t  *header
        ringtrailer_t *trailer
        uint8_t       *buf
        void          *scratchpad

    ctypedef struct ringiter_t:
        const ringbuffer_t *ring
        uint64_t generation
        size_t   offset

    ctypedef struct ringvec_t:
        void  *rv_base
        size_t rv_len
        int    rv_flags

    int ringbuffer_attached(ringbuffer_t *rb)
    ring_size_t size_aligned(const ring_size_t x)
    size_t ring_memsize(int flags, size_t size, size_t  sp_size)
    int ring_refcount(ringheader_t *ringheader)
    size_t ring_scratchpad_size(ringbuffer_t *ring)
    void ringheader_init(ringheader_t *ringheader, int flags,
			 size_t size, size_t  sp_size)
    void ringbuffer_init(ringheader_t *ringheader, ringbuffer_t *ring)


    # record-oriented ring operations: frame boundaries are preserved

    int record_write_begin(ringbuffer_t *ring, void ** data, size_t size)
    int record_write_end(ringbuffer_t *ring, void * data, size_t size)
    int record_write(ringbuffer_t *ring, const void * data, size_t size)

    int record_read(const ringbuffer_t *ring, const void **data, size_t *size)
    int record_shift(ringbuffer_t *ring)
    void *record_next(ringbuffer_t *ring)
    ring_size_t record_next_size(ringbuffer_t *ring)

    size_t record_write_space(const ringheader_t *h)
    int record_shift(ringbuffer_t *ring)
    int record_flush(ringbuffer_t *ring)

    int record_iter_init(const ringbuffer_t *ring, ringiter_t *iter)
    int record_iter_invalid(const ringiter_t *iter)
    int record_iter_shift(ringiter_t *iter)
    int record_iter_read(const ringiter_t *iter, const void **data, size_t *size)

    int ring_isstream(ringbuffer_t *ring)
    int ring_ismultipart(ringbuffer_t *ring)
    int ring_use_wmutex(ringbuffer_t *ring)
    int ring_use_rmutex(ringbuffer_t *ring)

    # character-oriented ring operations - pretty much a pipe:
    size_t stream_get_read_vector(const ringbuffer_t *ring, ringvec_t *vec)
    void stream_get_write_vector(const ringbuffer_t *ring, ringvec_t *vec)
    size_t stream_read_space(const ringheader_t *h)
    size_t stream_read(ringbuffer_t *ring, char *dest, size_t cnt)
    size_t stream_peek(ringbuffer_t *ring, char *dest, size_t cnt)
    void stream_read_advance(ringbuffer_t *ring, size_t cnt)
    size_t stream_flush(ringbuffer_t *ring)
    size_t stream_write_space(const ringheader_t *h)
    size_t stream_write(ringbuffer_t *ring, const char *src, size_t cnt)
    void stream_write_advance(ringbuffer_t *ring, size_t cnt)

cdef extern from "multiframe.h":
    ctypedef struct msgbuffer_t:
        ringbuffer_t * ring
        void * _write
        size_t write_size
        size_t write_off
        const void * _read
        size_t read_size
        size_t read_off

    ctypedef struct frameheader_t:
        int32_t    size
        uint32_t   flags
        uint8_t    *data

    int msgbuffer_init(msgbuffer_t *mb, ringbuffer_t *ring)

    int frame_write_begin(msgbuffer_t *mb, void ** data, size_t size, uint32_t flags)
    int frame_write_end(msgbuffer_t *mb, size_t size)
    int frame_write(msgbuffer_t *mb, const void * data, size_t size, uint32_t flags)
    int frame_writev(msgbuffer_t *mb, ringvec_t *rv)
    int msg_write_flush(msgbuffer_t *mb)
    int msg_write_abort(msgbuffer_t *mb)

    int frame_read(msgbuffer_t *mb, const void **data, size_t *size, uint32_t *flags)
    int frame_readv(msgbuffer_t *mb, ringvec_t *rv)
    int frame_shift(msgbuffer_t *mb)
    int msg_read_flush(msgbuffer_t *mb)
    int msg_read_abort(msgbuffer_t *mb)
